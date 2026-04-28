"""
app.py — PlagiaCheck Flask Backend
-----------------------------------
This server does 3 things:
  1. Serves the web/ folder as the frontend (index.html, main.js)
  2. Accepts file uploads via POST /analyze
  3. Runs the compiled C++ plagiacheck binary, reads results.json,
     and returns everything the browser needs (similarity data + file text)

Run with:
  python3 app.py
Then open: http://localhost:5000
"""

import os
import json
import shutil
import subprocess
import tempfile
from flask import Flask, request, jsonify, send_from_directory

# ── App Setup ────────────────────────────────────────────────────────────────

# __file__ is this app.py — BASE_DIR is the project root
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

app = Flask(
    __name__,
    static_folder=os.path.join(BASE_DIR, "web"),   # serve web/ as static files
    static_url_path=""
)

# Path to the compiled C++ binary
BINARY_PATH = os.path.join(BASE_DIR, "plagiacheck")

# Where results.json gets written by the C++ engine
RESULTS_JSON = os.path.join(BASE_DIR, "output", "results.json")


# ── Helpers ──────────────────────────────────────────────────────────────────

def allowed_file(filename):
    """
    Check if an uploaded file has an accepted extension.
    Right now we only support .txt — but adding PDF/DOCX later is easy:
    just add the extension to ALLOWED_EXTENSIONS below.
    """
    ALLOWED_EXTENSIONS = {".txt"}
    _, ext = os.path.splitext(filename.lower())
    return ext in ALLOWED_EXTENSIONS


def read_text_file(path):
    """Safely read a text file, returns empty string on failure."""
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def run_plagiacheck(source_path, targets_dir):
    """
    Run the C++ binary: ./plagiacheck <source_file> <targets_dir>
    Returns (success: bool, error_message: str)
    """
    if not os.path.isfile(BINARY_PATH):
        return False, (
            "C++ binary not found. Please compile first:\n"
            "  make all"
        )

    try:
        result = subprocess.run(
            [BINARY_PATH, source_path, targets_dir],
            capture_output=True,
            text=True,
            timeout=30,       # 30 second timeout — more than enough
            cwd=BASE_DIR      # run from project root so output/ is found
        )
        if result.returncode != 0:
            return False, result.stderr or "Binary returned non-zero exit code."
        return True, ""
    except subprocess.TimeoutExpired:
        return False, "Analysis timed out (>30s). Try with fewer/smaller files."
    except Exception as e:
        return False, str(e)


# ── Routes ───────────────────────────────────────────────────────────────────

@app.route("/")
def index():
    """Serve the main frontend page."""
    return send_from_directory(app.static_folder, "index.html")


@app.route("/analyze", methods=["POST"])
def analyze():
    """
    Main endpoint: receive uploaded files, run analysis, return results.

    Expected form data:
      source  — one file (the document to check)
      targets — one or more files (the documents to compare against)

    Returns JSON:
      {
        "source": "filename.txt",
        "source_text": "...",       ← raw text of source (for highlighting)
        "results": [
          {
            "file": "essay1.txt",
            "similarity": 73.5,     ← percentage (0–100)
            "level": "moderate",
            "matchingGrams": 42,
            "totalGrams": 120,
            "text": "..."           ← raw text of target (for highlighting)
          },
          ...
        ],
        "summary": { "high": 0, "moderate": 1, "low": 4 }
      }
    """
    # ── 1. Validate input ────────────────────────────────────────────────────
    if "source" not in request.files:
        return jsonify({"error": "No source file provided."}), 400

    source_file = request.files["source"]
    target_files = request.files.getlist("targets")

    if not source_file or source_file.filename == "":
        return jsonify({"error": "Source file is empty."}), 400

    if not target_files or all(f.filename == "" for f in target_files):
        return jsonify({"error": "No target files provided."}), 400

    if not allowed_file(source_file.filename):
        return jsonify({"error": "Source file must be a .txt file."}), 400

    for tf in target_files:
        if tf.filename and not allowed_file(tf.filename):
            return jsonify({
                "error": f"'{tf.filename}' is not a .txt file. Only .txt files are supported."
            }), 400

    # ── 2. Save files to a temporary directory ───────────────────────────────
    # We use a temp dir so parallel requests don't interfere with each other
    tmp_dir = tempfile.mkdtemp(prefix="plagiacheck_")

    try:
        # Save source file
        source_name = os.path.basename(source_file.filename)
        source_path = os.path.join(tmp_dir, source_name)
        source_file.save(source_path)

        # Save target files in a sub-folder called "targets"
        targets_dir = os.path.join(tmp_dir, "targets")
        os.makedirs(targets_dir)

        saved_targets = {}   # filename → file path (for reading text later)
        for tf in target_files:
            if tf.filename:
                name = os.path.basename(tf.filename)
                path = os.path.join(targets_dir, name)
                tf.save(path)
                saved_targets[name] = path

        if not saved_targets:
            return jsonify({"error": "No valid target files were saved."}), 400

        # ── 3. Run the C++ engine ────────────────────────────────────────────
        ok, err = run_plagiacheck(source_path, targets_dir)
        if not ok:
            return jsonify({"error": err}), 500

        # ── 4. Read results.json ─────────────────────────────────────────────
        if not os.path.isfile(RESULTS_JSON):
            return jsonify({"error": "Results file was not created by the engine."}), 500

        with open(RESULTS_JSON, "r", encoding="utf-8") as f:
            engine_output = json.load(f)

        # ── 5. Attach raw file text to each result (for text highlighting) ───
        results_with_text = []
        for r in engine_output.get("results", []):
            target_path = saved_targets.get(r["file"], "")
            r["text"] = read_text_file(target_path) if target_path else ""
            results_with_text.append(r)

        # ── 6. Build and return response ─────────────────────────────────────
        response = {
            "source": source_name,
            "source_text": read_text_file(source_path),
            "results": results_with_text,
            "summary": engine_output.get("summary", {}),
        }
        return jsonify(response)

    finally:
        # Always clean up temp files, even if something went wrong
        shutil.rmtree(tmp_dir, ignore_errors=True)


# ── Entry Point ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("\n  PlagiaCheck Server starting...")
    print("  Open your browser at: http://localhost:8080\n")
    app.run(host="0.0.0.0", port=8080, debug=False)
