#pragma once
#include <string>
#include <unordered_set>
#include <cstddef>

// ─────────────────────────────────────────────────────────────────────────────
//  generateFingerprints
//
//  Converts a preprocessed text string into a compact set of fingerprint
//  hashes using two layered algorithms:
//
//    1. Rabin-Karp rolling hash  — produces a hash for every k-gram
//       (consecutive sequence of k words).
//
//    2. Winnowing                — slides a window of size w over those
//       hashes and selects the minimum in each window as a representative
//       fingerprint.  This is the algorithm used by Stanford's MOSS.
//
//  Parameters:
//    processedText — output of preprocess() — space-separated words
//    k             — k-gram size in words  (default 5)
//    w             — Winnowing window size  (default 4)
//
//  Returns an unordered_set<size_t> of fingerprint hashes.
//  Returns an empty set if the text is shorter than k words.
// ─────────────────────────────────────────────────────────────────────────────
std::unordered_set<std::size_t>
generateFingerprints(const std::string& processedText, int k = 1, int w = 1);