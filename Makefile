CXX      = g++
CXXFLAGS = -std=c++17 -O2

SRC = src/main.cpp \
      src/preprocessor.cpp \
      src/fingerprint.cpp \
      src/comparator.cpp \
      src/json_writer.cpp

OUT = plagiacheck

# ── Build the C++ binary ──────────────────────────────────────────────────
all:
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC)
	@echo ""
	@echo "  ✅  Binary compiled: ./plagiacheck"
	@echo "  Run 'make server' to start the web server."
	@echo ""

# ── Install Python dependencies ───────────────────────────────────────────
setup:
	pip3 install -r requirements.txt
	@echo "  ✅  Python dependencies installed."

# ── Compile C++ + install Python deps + start Flask server (ONE COMMAND) ──
server: all setup
	@echo ""
	@echo "  🚀  Starting PlagiaCheck server..."
	@echo "  Open your browser at: http://localhost:8080"
	@echo ""
	python3 app.py

# ── Run the CLI tool directly on test files (no web server) ───────────────
run:
	./plagiacheck test_files/source.txt test_files/targets/

# ── Remove compiled binary ────────────────────────────────────────────────
clean:
	rm -f $(OUT)
	@echo "  🧹  Cleaned."
