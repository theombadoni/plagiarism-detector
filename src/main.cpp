//  main.cpp  —  PlagiaCheck entry point
//
//  Usage:
//    ./plagiacheck <source_file.txt> <targets_directory/>
//
//  What it does:
//    1. Reads + fingerprints the source file
//    2. Discovers every .txt file inside the targets directory
//    3. Fingerprints each target and computes Jaccard similarity
//    4. Prints a colour-coded terminal report
//    5. Writes output/results.json  (read by the Three.js 3D UI)
//
//  Compile (macOS M1, requires C++17):
//    g++ -std=c++17 -O2 -o plagiacheck src/*.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <filesystem>

#include "interfaces.h"   // readFile, preprocess, generateFingerprints, jaccardSimilarity
#include "json_writer.h"  // FileResult, writeResults

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  ANSI colour helpers  (disable by setting NO_COLOR env variable)
// ─────────────────────────────────────────────────────────────────────────────
namespace Color {
    static bool enabled = true;    // set to false on Windows / non-TTY if needed

    inline std::string code(const char* c) { return enabled ? c : ""; }

    const auto RED    = []{ return code("\033[31m"); };
    const auto YELLOW = []{ return code("\033[33m"); };
    const auto GREEN  = []{ return code("\033[32m"); };
    const auto CYAN   = []{ return code("\033[36m"); };
    const auto BOLD   = []{ return code("\033[1m");  };
    const auto DIM    = []{ return code("\033[2m");  };
    const auto RESET  = []{ return code("\033[0m");  };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Map a similarity percentage to a level string
static std::string getLevel(double pct)
{
    if (pct > 75.0) return "high";
    if (pct > 50.0) return "moderate";
    return "low";
}

// Print a horizontal progress/similarity bar with colour
static void printBar(double pct)
{
    constexpr int W = 28;
    int filled = static_cast<int>((pct / 100.0) * W);

    std::string color = (pct > 75.0) ? Color::RED()
                      : (pct > 50.0) ? Color::YELLOW()
                      :                Color::GREEN();

    std::cout << color << "[";
    for (int i = 0; i < W; ++i)
        std::cout << (i < filled ? "█" : "░");
    std::cout << "] "
              << std::fixed << std::setprecision(1) << pct << "%"
              << Color::RESET();
}

// Print usage / help text
static void printUsage(const char* prog)
{
    std::cout << "\n" << Color::BOLD() << "Usage:" << Color::RESET() << "\n"
              << "  " << prog << " <source_file> <targets_directory>\n\n"
              << Color::DIM()
              << "Example:\n"
              << "  " << prog << " source.txt test_files/targets/\n\n"
              << "  source_file       — the document you want to check\n"
              << "  targets_directory — folder containing .txt files to compare against\n"
              << Color::RESET() << "\n";
}

// Collect every regular .txt file inside a directory (sorted)
static std::vector<fs::path> discoverTargets(const fs::path& dir)
{
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

// Count how many fingerprints in A also appear in B
static int countIntersection(const std::unordered_set<std::size_t>& a,
                              const std::unordered_set<std::size_t>& b)
{
    // Iterate over the smaller set for efficiency
    const auto& small = (a.size() <= b.size()) ? a : b;
    const auto& large = (a.size() <= b.size()) ? b : a;

    int count = 0;
    for (const auto& fp : small)
        if (large.count(fp)) ++count;
    return count;
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    // ── Banner ────────────────────────────────────────────────────────────────
    std::cout << Color::BOLD() << Color::CYAN()
              << "\n╔══════════════════════════════════════════╗\n"
              <<   "║   PlagiaCheck  ·  DAA Project  ·  v1.0  ║\n"
              <<   "║   Algorithms: Rabin-Karp · Winnowing     ║\n"
              <<   "║              Jaccard · Kruskal MST        ║\n"
              <<   "╚══════════════════════════════════════════╝\n"
              << Color::RESET() << "\n";

    // ── Argument validation ───────────────────────────────────────────────────
    if (argc != 3) {
        std::cerr << Color::RED() << "[ERROR] Expected 2 arguments, got "
                  << (argc - 1) << "." << Color::RESET() << "\n";
        printUsage(argv[0]);
        return 1;
    }

    fs::path sourcePath(argv[1]);
    fs::path targetsDir(argv[2]);

    if (!fs::exists(sourcePath) || !fs::is_regular_file(sourcePath)) {
        std::cerr << Color::RED()
                  << "[ERROR] Source file not found: " << sourcePath
                  << Color::RESET() << "\n";
        return 1;
    }

    if (!fs::exists(targetsDir) || !fs::is_directory(targetsDir)) {
        std::cerr << Color::RED()
                  << "[ERROR] Targets directory not found: " << targetsDir
                  << Color::RESET() << "\n";
        return 1;
    }

    // ── Discover target files ─────────────────────────────────────────────────
    std::vector<fs::path> targets = discoverTargets(targetsDir);

    if (targets.empty()) {
        std::cerr << Color::YELLOW()
                  << "[WARN] No .txt files found in: " << targetsDir
                  << Color::RESET() << "\n";
        return 1;
    }

    std::cout << Color::BOLD() << "Source : " << Color::RESET()
              << sourcePath.string() << "\n"
              << Color::BOLD() << "Targets: " << Color::RESET()
              << targets.size() << " file(s) in " << targetsDir.string() << "\n\n";

    // ── Fingerprint the source file ───────────────────────────────────────────
    auto wallStart = std::chrono::steady_clock::now();

    std::cout << Color::DIM() << "  Reading + fingerprinting source..." << Color::RESET() << "\n";

    std::string sourceRaw   = readFile(sourcePath.string());
    if (sourceRaw.empty()) {
        std::cerr << Color::RED()
                  << "[ERROR] Source file is empty or could not be read.\n"
                  << Color::RESET();
        return 1;
    }

    std::string             sourceClean        = preprocess(sourceRaw);
    std::unordered_set<std::size_t> sourceFingerprints = generateFingerprints(sourceClean);

    if (sourceFingerprints.empty()) {
        std::cerr << Color::YELLOW()
                  << "[WARN] Source file is too short to generate fingerprints.\n"
                  << "       Make sure it contains at least 5 words.\n"
                  << Color::RESET();
        return 1;
    }

    std::cout << Color::DIM()
              << "  Source fingerprints: " << sourceFingerprints.size() << "\n\n"
              << Color::RESET();

    // ── Compare against every target ──────────────────────────────────────────
    std::cout << Color::DIM() << "  Comparing...\n\n" << Color::RESET();

    std::vector<FileResult> results;
    results.reserve(targets.size());

    for (std::size_t i = 0; i < targets.size(); ++i) {
        const fs::path& tp   = targets[i];
        std::string     name = tp.filename().string();

        // Progress prefix
        std::cout << Color::DIM()
                  << "  [" << std::setw(2) << (i + 1) << "/" << targets.size() << "] "
                  << Color::RESET()
                  << std::left << std::setw(30) << name << "  ";
        std::cout.flush();

        // Read + fingerprint the target
        std::string targetRaw          = readFile(tp.string());
        std::string targetClean        = preprocess(targetRaw);
        auto        targetFingerprints = generateFingerprints(targetClean);

        if (targetFingerprints.empty()) {
            std::cout << Color::DIM() << "(too short — skipped)\n" << Color::RESET();
            continue;
        }

        // Compute Jaccard similarity
        double similarity = jaccardSimilarity(sourceFingerprints, targetFingerprints) * 100.0;
        int    matching   = countIntersection(sourceFingerprints, targetFingerprints);

        printBar(similarity);
        std::cout << "\n";

        results.push_back({
            name,
            similarity,
            getLevel(similarity),
            matching,
            static_cast<int>(sourceFingerprints.size())
        });
    }

    if (results.empty()) {
        std::cerr << Color::YELLOW()
                  << "\n[WARN] All target files were too short to compare.\n"
                  << Color::RESET();
        return 1;
    }

    // ── Sort: highest similarity first ────────────────────────────────────────
    std::sort(results.begin(), results.end(),
        [](const FileResult& a, const FileResult& b) {
            return a.similarity > b.similarity;
        });

    // ── Print summary table ───────────────────────────────────────────────────
    auto   wallEnd = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(wallEnd - wallStart).count();

    std::cout << "\n" << Color::BOLD()
              << "  ┌────────────────────────────────────────────────┐\n"
              << "  │           Final Results (by similarity)        │\n"
              << "  ├──────────────────────────────┬───────┬─────────┤\n"
              << "  │ File                         │   %   │  Level  │\n"
              << "  ├──────────────────────────────┼───────┼─────────┤\n"
              << Color::RESET();

    int highCnt = 0, modCnt = 0, lowCnt = 0;
    for (const auto& r : results) {
        std::string color = (r.level == "high")      ? Color::RED()
                          : (r.level == "moderate")   ? Color::YELLOW()
                          :                             Color::GREEN();

        std::cout << "  │ " << color
                  << std::left  << std::setw(28) << r.filename
                  << " │ " << std::right << std::setw(5)
                  << std::fixed << std::setprecision(1) << r.similarity
                  << " │ " << std::setw(7) << r.level
                  << Color::RESET() << " │\n";

        if (r.level == "high")         ++highCnt;
        else if (r.level == "moderate") ++modCnt;
        else                            ++lowCnt;
    }

    std::cout << Color::BOLD()
              << "  └──────────────────────────────┴───────┴─────────┘\n\n"
              << Color::RESET();

    std::cout << "  " << Color::RED()    << "High     (> 75%): " << highCnt << Color::RESET() << "\n"
              << "  " << Color::YELLOW() << "Moderate (51-75%): " << modCnt  << Color::RESET() << "\n"
              << "  " << Color::GREEN()  << "Low      (<= 50%): " << lowCnt  << Color::RESET() << "\n"
              << "  " << Color::DIM()    << "Time: " << std::setprecision(3)
              << elapsed << "s\n\n" << Color::RESET();

    // ── Write results.json ────────────────────────────────────────────────────
    const std::string outputPath = "output/results.json";

    if (writeResults(sourcePath.filename().string(), results, outputPath)) {
        std::cout << Color::GREEN() << Color::BOLD() << "  [OK] " << Color::RESET()
                  << "Wrote: " << outputPath << "\n"
                  << Color::DIM()
                  << "       Start a local server and open web/index.html\n"
                  << "       to view the interactive 3D similarity graph.\n\n"
                  << "       python3 -m http.server 8080\n"
                  << "       then visit: http://localhost:8080/web/\n\n"
                  << Color::RESET();
    } else {
        std::cerr << Color::RED()
                  << "[ERROR] Failed to write results.json\n"
                  << Color::RESET();
        return 1;
    }

    return 0;
}