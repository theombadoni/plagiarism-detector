// ─────────────────────────────────────────────────────────────────────────────
//  interfaces.h  —  declarations for ALL modules
//
//  Person A implements: generateFingerprints()   (fingerprint.cpp)
//  Person B implements: readFile(), preprocess(), jaccardSimilarity()
//                       (preprocessor.cpp, comparator.cpp)
//
//  main.cpp includes only THIS file.  When you split into separate .h files
//  per module, just replace this include with the individual headers.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include <string>
#include <unordered_set>

// ── preprocessor.h (Person B) ─────────────────────────────────────────────
// Read raw text from a file path. Returns "" on failure.
std::string readFile(const std::string& path);

// Lowercase, strip punctuation, remove stopwords, return word-token string.
std::string preprocess(const std::string& rawText);

// ── fingerprint.h (Person A) ──────────────────────────────────────────────
// Generate a set of Winnowed Rabin-Karp fingerprints from preprocessed text.
//   k  = k-gram size  (default 5 words)
//   w  = window size for Winnowing (default 4)
std::unordered_set<std::size_t>
generateFingerprints(const std::string& processedText, int k = 5, int w = 4);

// ── comparator.h (Person B) ───────────────────────────────────────────────
// Jaccard similarity:  |A ∩ B| / |A ∪ B|
// Returns a value in [0.0, 1.0].  Multiply by 100 to get percentage.
double jaccardSimilarity(const std::unordered_set<std::size_t>& a,
                         const std::unordered_set<std::size_t>& b);