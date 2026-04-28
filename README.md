# PlagiaCheck 🔍

A plagiarism detector that uses **Rabin-Karp fingerprinting** and **Jaccard similarity** to compare text files. Upload a source file and multiple comparison files through the browser — get back similarity percentages, a bar graph, and highlighted copied text.

---

## What's Inside

```
plagiarism-detector/
├── src/               ← C++ engine (the core algorithms)
│   ├── main.cpp       ← Entry point: reads files, prints results, writes JSON
│   ├── preprocessor.cpp ← Cleans text (lowercase, removes stopwords)
│   ├── fingerprint.cpp  ← Rabin-Karp hashing + Winnowing
│   ├── comparator.cpp   ← Jaccard similarity + Kruskal MST
│   └── json_writer.cpp  ← Writes output/results.json
├── web/
│   ├── index.html     ← Modern dark-theme frontend
│   └── main.js        ← Upload logic, chart, text highlighting
├── test_files/        ← Sample files to try out
│   ├── source.txt
│   └── targets/
├── app.py             ← Flask server (bridges browser ↔ C++ engine)
├── requirements.txt   ← Python dependencies (just Flask)
├── Makefile           ← Build commands
└── output/            ← results.json is written here after each run
```

---

## Requirements

| Tool | Version |
|---|---|
| g++ (GCC or Clang) | C++17 support required |
| Python | 3.9 or newer |
| pip | comes with Python |

Check if you have them:
```bash
g++ --version
python3 --version
```

---

## How to Run (One Command)

```bash
make server
```

That's it. This single command will:
1. Compile the C++ engine → `./plagiacheck`
2. Install Flask (`pip3 install flask`)
3. Start the web server on port 5000

Then open your browser and go to:
```
http://localhost:8080
```

---

## How to Use the Web UI

1. **Source File** — Upload the document you want to check.
2. **How many files?** — Use the + / − buttons to set how many comparison files you want.
3. **Upload comparison files** — Upload each `.txt` file you want to compare against.
4. Click **Run Plagiarism Check**.

You'll see:
- **Summary cards** — counts of High / Moderate / Low similarity files
- **Bar graph** — visual overview of similarity percentages
- **Per-file cards** — click any card to see highlighted matching text

### Similarity Levels

| Level | Threshold | Color |
|---|---|---|
| 🔴 High | > 75% | Red |
| 🟡 Moderate | > 50% | Orange |
| 🟢 Low | ≤ 50% | Green |

---

## CLI Mode (No Browser)

You can also run the C++ tool directly from the terminal:

```bash
make all   # compile
make run   # compare test_files/source.txt against test_files/targets/
```

---

## How the Algorithm Works (Simple Version)

1. **Clean** — remove punctuation, lowercase, drop common words ("the", "and"...)
2. **Fingerprint** — split text into overlapping word groups (k-grams), hash each one using Rabin-Karp
3. **Winnow** — pick only the minimum hash from each sliding window (reduces fingerprint size)
4. **Compare** — count how many fingerprints are shared → Jaccard score = `shared / total`

---

## Clean Up

```bash
make clean      # removes compiled binary
```