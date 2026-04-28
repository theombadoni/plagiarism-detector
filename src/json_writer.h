#pragma once
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  FileResult  —  one comparison result (source vs. one target file)
// ─────────────────────────────────────────────────────────────────────────────
struct FileResult {
    std::string filename;     // just the filename, e.g. "essay1.txt"
    double      similarity;   // 0.0 – 100.0  (percentage)
    std::string level;        // "high" | "moderate" | "low"
    int         matchingGrams;// how many fingerprints matched
    int         totalGrams;   // total fingerprints in source file
};

// ─────────────────────────────────────────────────────────────────────────────
//  writeResults
//
//  Serialises results to a JSON file that the Three.js frontend reads.
//  Creates output/ directory automatically if it does not exist.
//
//  Returns true on success, false if the file could not be opened/written.
// ─────────────────────────────────────────────────────────────────────────────
bool writeResults(const std::string&            sourceName,
                  const std::vector<FileResult>& results,
                  const std::string&             outputPath = "output/results.json");