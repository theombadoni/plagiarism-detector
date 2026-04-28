#pragma once
#include <unordered_set>
#include <vector>
#include <cstddef>

// ─────────────────────────────────────────────────────────────────────────────
//  jaccardSimilarity
//
//  Computes  |A ∩ B| / |A ∪ B|  on two fingerprint sets.
//  Returns a value in [0.0, 1.0].  Multiply by 100 for percentage.
//
//  Jaccard is ideal here because:
//    - It is symmetric: sim(A,B) == sim(B,A)
//    - It is normalised: 1.0 means identical content, 0.0 means no overlap
//    - It works directly on sets, so duplicates are already collapsed
// ─────────────────────────────────────────────────────────────────────────────
double jaccardSimilarity(const std::unordered_set<std::size_t>& a,
                         const std::unordered_set<std::size_t>& b);


// ─────────────────────────────────────────────────────────────────────────────
//  Kruskal's Minimum Spanning Tree
//
//  Models documents as graph nodes and similarity scores as weighted edges.
//  Edge weight = (100 - similarity%)  so that lower weight = more similar.
//
//  Kruskal's greedily picks the lowest-weight edges that don't form a cycle
//  using a Union-Find (Disjoint Set Union) data structure.
//
//  This finds the "backbone" of similarity relationships across all documents,
//  which the Three.js 3D UI uses to highlight clusters of plagiarised content.
// ─────────────────────────────────────────────────────────────────────────────

// An edge in the full document similarity graph
struct SimilarityEdge {
    int    u, v;      // document indices (0 = source, 1..n = targets)
    double weight;    // 100.0 - similarity%  (lower = more similar)
};

// An edge selected into the MST by Kruskal's
struct MSTEdge {
    int    u, v;
    double weight;    // same scale as SimilarityEdge::weight
};

// Run Kruskal's algorithm.
// numNodes — total number of documents (including the source file).
// edges    — all pairwise similarity edges.
// Returns the (numNodes - 1) MST edges in ascending weight order.
std::vector<MSTEdge> kruskalMST(int numNodes,
                                  const std::vector<SimilarityEdge>& edges);