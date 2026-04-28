#pragma once
#include <string>

// Read entire file into a string. Returns "" on failure.
std::string readFile(const std::string& path);

// Lowercase, strip punctuation, remove English stopwords.
// Returns a single space-separated token string ready for fingerprinting.
std::string preprocess(const std::string& rawText);