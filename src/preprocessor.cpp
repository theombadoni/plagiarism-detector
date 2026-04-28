#include "preprocessor.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
//  Common English stopwords — these add noise without adding meaning.
//  Removing them makes fingerprint matching more accurate.
// ─────────────────────────────────────────────────────────────────────────────
static const std::unordered_set<std::string> STOPWORDS = {
    "a","an","the","and","or","but","in","on","at","to","for","of","with",
    "by","from","is","are","was","were","be","been","being","have","has",
    "had","do","does","did","will","would","shall","should","may","might",
    "must","can","could","that","this","these","those","it","its","i","you",
    "he","she","we","they","me","him","her","us","them","my","your","his",
    "our","their","what","which","who","whom","not","no","nor","so","yet",
    "both","either","neither","as","if","then","than","because","while",
    "although","though","after","before","since","until","unless","also",
    "about","above","below","between","into","through","during","without",
    "very","just","more","also","such","only","same","other","each","all",
    "any","few","most","some","over","up","out","when","where","how","why"
};

// ─────────────────────────────────────────────────────────────────────────────
//  readFile  —  read entire file into a std::string
// ─────────────────────────────────────────────────────────────────────────────
std::string readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "[preprocessor] Cannot open file: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
//  preprocess  —  clean raw text into a normalised token string
//
//  Steps:
//    1. Lowercase every character
//    2. Replace all non-alpha characters with a space (removes punctuation,
//       digits, newlines, tabs — word boundaries are preserved)
//    3. Tokenise on whitespace
//    4. Drop single-character tokens and stopwords
//    5. Rejoin surviving tokens into a single space-separated string
//
//  The returned string is what generateFingerprints() operates on.
// ─────────────────────────────────────────────────────────────────────────────
std::string preprocess(const std::string& rawText)
{
    if (rawText.empty()) return "";

    // Step 1 + 2: lowercase and replace non-alpha with spaces
    std::string normalised;
    normalised.reserve(rawText.size());
    for (unsigned char c : rawText) {
        if (std::isalpha(c))
            normalised += static_cast<char>(std::tolower(c));
        else
            normalised += ' ';
    }

    // Step 3 + 4 + 5: tokenise, filter, rejoin
    std::istringstream stream(normalised);
    std::string        word;
    std::string        clean;
    clean.reserve(normalised.size());

    while (stream >> word) {
        // Drop single-char tokens and stopwords
        if (word.size() > 1 && STOPWORDS.find(word) == STOPWORDS.end()) {
            if (!clean.empty()) clean += ' ';
            clean += word;
        }
    }

    return clean;
}