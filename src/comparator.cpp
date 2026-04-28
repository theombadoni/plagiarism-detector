#include "comparator.h"

#include <algorithm>
#include <numeric>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  jaccardSimilarity
//
//  Formula:  |A ∩ B| / |A ∪ B|
//
//  We iterate over the smaller set and probe the larger for O(min(|A|,|B|))
//  intersection count.  Union size is derived from inclusion-exclusion:
//    |A ∪ B| = |A| + |B| - |A ∩ B|
// ─────────────────────────────────────────────────────────────────────────────
double jaccardSimilarity(const std::unordered_set<std::size_t>& a,
                         const std::unordered_set<std::size_t>& b)
{
    if (a.empty() && b.empty()) return 1.0;   // both empty → identical
    if (a.empty() || b.empty()) return 0.0;   // one empty  → no overlap

    // Always probe the larger set (hash-lookup is O(1))
    const auto& small = (a.size() <= b.size()) ? a : b;
    const auto& large = (a.size() <= b.size()) ? b : a;

    int intersect = 0;
    for (const auto& fp : small)
        if (large.count(fp)) ++intersect;

    // |A ∪ B| = |A| + |B| - |A ∩ B|
    int unionSize = static_cast<int>(a.size() + b.size()) - intersect;

    return static_cast<double>(intersect) / static_cast<double>(unionSize);
}


// ─────────────────────────────────────────────────────────────────────────────
//  Union-Find (Disjoint Set Union)  —  required by Kruskal's algorithm
//
//  Two optimisations:
//    1. Path compression  — flattens the tree on every find()
//    2. Union by rank     — always attaches the shorter tree under the taller
//
//  Together they give near-O(1) amortised operations, making Kruskal's
//  overall complexity O(E log E) dominated by the sort step.
// ─────────────────────────────────────────────────────────────────────────────
struct DSU {
    std::vector<int> parent;
    std::vector<int> rank;

    explicit DSU(int n) : parent(n), rank(n, 0) {
        std::iota(parent.begin(), parent.end(), 0);   // parent[i] = i
    }

    // Find root with path compression
    int find(int x) {
        if (parent[x] != x)
            parent[x] = find(parent[x]);   // recurse + compress
        return parent[x];
    }

    // Unite two sets; returns false if they were already in the same set
    // (adding the edge would form a cycle — Kruskal skips such edges)
    bool unite(int x, int y) {
        int px = find(x), py = find(y);
        if (px == py) return false;          // same component → cycle

        // Attach shorter tree under taller (union by rank)
        if (rank[px] < rank[py]) std::swap(px, py);
        parent[py] = px;
        if (rank[px] == rank[py]) ++rank[px];
        return true;
    }
};


// ─────────────────────────────────────────────────────────────────────────────
//  kruskalMST
//
//  Greedy algorithm:
//    1. Sort all edges by weight ascending
//    2. For each edge (u, v):
//         - If u and v are in different components → add edge, merge components
//         - If same component              → skip (would form a cycle)
//    3. Stop when MST has (numNodes - 1) edges
//
//  The resulting MST represents the "most similar" skeleton connecting all
//  documents.  The Three.js visualiser highlights these MST edges.
// ─────────────────────────────────────────────────────────────────────────────
std::vector<MSTEdge> kruskalMST(int numNodes,
                                  const std::vector<SimilarityEdge>& edges)
{
    if (numNodes <= 1) return {};

    // Step 1: Sort edges by weight (ascending = most similar first)
    std::vector<SimilarityEdge> sorted = edges;
    std::sort(sorted.begin(), sorted.end(),
        [](const SimilarityEdge& a, const SimilarityEdge& b) {
            return a.weight < b.weight;
        });

    DSU              dsu(numNodes);
    std::vector<MSTEdge> mst;
    mst.reserve(numNodes - 1);

    // Step 2: Greedily select edges
    for (const auto& e : sorted) {
        if (dsu.unite(e.u, e.v)) {
            mst.push_back({ e.u, e.v, e.weight });
            if (static_cast<int>(mst.size()) == numNodes - 1)
                break;   // MST is complete
        }
    }

    return mst;
}