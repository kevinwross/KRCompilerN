#ifndef KRC_DEBUG_INFO_H
#define KRC_DEBUG_INFO_H

#include "ast.h"
#include "source_manager.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace krc {

// Frontend-native debug information collected from the AST. No dependency on
// the debug or backend modules; the driver converts this to KDBG.
struct DebugSourceFile {
    int file_id = -1;
    std::string path;
    std::uint64_t checksum = 0; // FNV-1a 64-bit of source contents
};

struct DebugFunction {
    std::string name;
    SourceLocation location;
    bool is_static = false;
    bool is_extern = false;
};

struct DebugLineEntry {
    SourceLocation location;
};

// Aggregated debug info for one translation unit.
struct DebugInfo {
    std::vector<DebugSourceFile> sources;
    std::vector<DebugFunction> functions;
    std::vector<DebugLineEntry> lines;
};

// Collects debug information from a parsed and semantically-analyzed
// translation unit.
class DebugInfoCollector {
public:
    DebugInfoCollector(const TranslationUnit& tu, const SourceManager& source_manager);

    bool collect();

    const DebugInfo& info() const { return info_; }

private:
    const TranslationUnit& tu_;
    const SourceManager& source_manager_;
    DebugInfo info_;

    // Maps a SourceManager file_id to an index into info_.sources.
    std::unordered_map<int, std::uint32_t> file_id_to_index_;

    std::uint32_t register_source(int file_id);

    // Recursively collect line entries from statements and expressions.
    void collect_statements(
        const std::vector<std::shared_ptr<Statement>>& stmts,
        std::uint32_t file_index);
    void collect_expression(const Expression& expr);

    static std::uint64_t fnv1a_64(std::string_view data);
};

} // namespace krc

#endif // KRC_DEBUG_INFO_H
