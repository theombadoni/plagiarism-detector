#include "json_writer.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <ctime>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helper: escape a raw string for safe embedding in JSON.
//  Handles backslash, double-quote, and common control characters.
// ─────────────────────────────────────────────────────────────────────────────
static std::string escapeJSON(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                // Escape any other non-printable byte as \uXXXX
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helper: get current UTC timestamp as ISO-8601 string.
//  Example output: "2024-05-12T14:30:00Z"
// ─────────────────────────────────────────────────────────────────────────────
static std::string currentTimestamp()
{
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    return std::string(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
//  writeResults  —  main public function
// ─────────────────────────────────────────────────────────────────────────────
bool writeResults(const std::string&             sourceName,
                  const std::vector<FileResult>& results,
                  const std::string&             outputPath)
{
    // 1. Create the output directory if it doesn't exist yet
    fs::path outFile(outputPath);
    if (outFile.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(outFile.parent_path(), ec);
        if (ec) {
            std::cerr << "[json_writer] Cannot create directory: "
                      << outFile.parent_path() << " — " << ec.message() << "\n";
            return false;
        }
    }

    // 2. Open file for writing (overwrites any previous run)
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        std::cerr << "[json_writer] Cannot open for writing: " << outputPath << "\n";
        return false;
    }

    // 3. Count results by level — used in the summary block
    int highCount = 0, modCount = 0, lowCount = 0;
    double maxSimilarity = 0.0;
    for (const auto& r : results) {
        if (r.level == "high")         highCount++;
        else if (r.level == "moderate") modCount++;
        else                            lowCount++;
        if (r.similarity > maxSimilarity) maxSimilarity = r.similarity;
    }

    // 4. Write JSON
    //    We build it manually to avoid pulling in a third-party library.
    //    Every floating-point value is rounded to 2 decimal places.
    file << std::fixed << std::setprecision(2);

    file << "{\n";

    // — Metadata block —
    file << "  \"source\": \""        << escapeJSON(sourceName)   << "\",\n";
    file << "  \"generatedAt\": \""   << currentTimestamp()       << "\",\n";
    file << "  \"totalCompared\": "   << results.size()           << ",\n";
    file << "  \"maxSimilarity\": "   << maxSimilarity            << ",\n";

    // — Summary block (Three.js uses this to colour the legend) —
    file << "  \"summary\": {\n";
    file << "    \"high\": "          << highCount  << ",\n";
    file << "    \"moderate\": "      << modCount   << ",\n";
    file << "    \"low\": "           << lowCount   << "\n";
    file << "  },\n";

    // — Individual results array —
    file << "  \"results\": [\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const FileResult& r = results[i];

        file << "    {\n";
        file << "      \"file\": \""          << escapeJSON(r.filename) << "\",\n";
        file << "      \"similarity\": "       << r.similarity           << ",\n";
        file << "      \"level\": \""          << r.level                << "\",\n";
        file << "      \"matchingGrams\": "    << r.matchingGrams        << ",\n";
        file << "      \"totalGrams\": "       << r.totalGrams           << "\n";
        file << "    }";

        // Comma after every entry except the last
        if (i + 1 < results.size()) file << ",";
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";

    file.close();
    return true;
}