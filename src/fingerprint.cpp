#include "fingerprint.h"

#include <sstream>
#include <vector>
#include <deque>
#include <algorithm>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
//  Rabin-Karp constants
//  BASE and MOD are chosen to minimise hash collisions.
//  Two separate (base, mod) pairs are combined into one 64-bit value using
//  bit-packing, which further reduces false positives.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr std::size_t BASE1 = 31ULL;
static constexpr std::size_t MOD1  = 1'000'000'007ULL;
static constexpr std::size_t BASE2 = 37ULL;
static constexpr std::size_t MOD2  = 998'244'353ULL;

// ─────────────────────────────────────────────────────────────────────────────
//  hashWord  —  polynomial hash of a single word string
// ─────────────────────────────────────────────────────────────────────────────
static std::size_t hashWord(const std::string& w)
{
    std::size_t h = 0;
    for (unsigned char c : w)
        h = (h * BASE1 + c) % MOD1;
    return h;
}

// ─────────────────────────────────────────────────────────────────────────────
//  kgramHash  —  hash a consecutive window of k word-hashes
//
//  We treat the sequence of word hashes as "digits" in a base-BASE1 number.
//  The result is a single hash representing the k-gram.
//
//  This is the core of Rabin-Karp applied at the word level.
// ─────────────────────────────────────────────────────────────────────────────
static std::size_t kgramHash(const std::vector<std::size_t>& wordHashes,
                               int start, int k)
{
    std::size_t h1 = 0, h2 = 0;
    for (int i = 0; i < k; ++i) {
        h1 = (h1 * BASE1 + wordHashes[start + i]) % MOD1;
        h2 = (h2 * BASE2 + wordHashes[start + i]) % MOD2;
    }
    // Pack two hashes into one 64-bit fingerprint
    return (h1 << 32) | h2;
}

// ─────────────────────────────────────────────────────────────────────────────
//  generateFingerprints  —  main public function
//
//  Algorithm outline:
//    1. Tokenise processedText into words
//    2. Hash each word  (word-level Rabin-Karp)
//    3. Hash each k-gram of word-hashes  (k-gram Rabin-Karp)
//    4. Apply Winnowing: slide a window of size w, record the minimum
//       hash value found in each window position
//
//  Winnowing guarantees:
//    - At least one fingerprint per window of k+w-1 words
//    - Identical contiguous passages always produce at least one shared hash
//    - Robust against small local edits (reordering within a window is ignored)
// ─────────────────────────────────────────────────────────────────────────────
std::unordered_set<std::size_t>
generateFingerprints(const std::string& processedText, int k, int w)
{
    // ── Step 1: Tokenise ──────────────────────────────────────────────────────
    std::istringstream      stream(processedText);
    std::vector<std::string> words;
    std::string              word;
    while (stream >> word) words.push_back(word);

    int n = static_cast<int>(words.size());
    if (n < k) return {};   // text too short to form even one k-gram

    // ── Step 2: Hash each word ────────────────────────────────────────────────
    std::vector<std::size_t> wordHashes(n);
    for (int i = 0; i < n; ++i)
        wordHashes[i] = hashWord(words[i]);

    // ── Step 3: Compute k-gram hashes (Rabin-Karp) ───────────────────────────
    int numKgrams = n - k + 1;
    std::vector<std::size_t> kgramHashes(numKgrams);
    for (int i = 0; i < numKgrams; ++i)
        kgramHashes[i] = kgramHash(wordHashes, i, k);

    // ── Step 4: Winnowing ─────────────────────────────────────────────────────
    std::unordered_set<std::size_t> fingerprints;

    if (numKgrams < w) {
        // Fewer k-grams than window size — just take the minimum of all
        auto minIt = std::min_element(kgramHashes.begin(), kgramHashes.end());
        fingerprints.insert(*minIt);
        return fingerprints;
    }

    // Monotonic deque: front always holds index of minimum in current window.
    // This gives O(n) Winnowing instead of O(n*w).
    std::deque<int> dq;

    for (int i = 0; i < numKgrams; ++i) {
        // Evict indices that have slid out of the window
        while (!dq.empty() && dq.front() <= i - w)
            dq.pop_front();

        // Maintain ascending order: remove back elements with larger hash
        // so the minimum always stays at the front
        while (!dq.empty() && kgramHashes[dq.back()] >= kgramHashes[i])
            dq.pop_back();

        dq.push_back(i);

        // Record the window minimum once the first full window is reached
        if (i >= w - 1)
            fingerprints.insert(kgramHashes[dq.front()]);
    }

    return fingerprints;
}