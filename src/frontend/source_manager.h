#ifndef KRC_SOURCE_MANAGER_H
#define KRC_SOURCE_MANAGER_H

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace krc {

// A source location: file, line, column, and byte offset.
// file_id == -1 indicates an invalid/unknown location.
struct SourceLocation {
    int file_id = -1;             // index into SourceManager's file table
    std::uint32_t line = 0;       // 1-based
    std::uint32_t column = 0;     // 1-based
    std::uint64_t offset = 0;     // byte offset within the file
};

// A loaded source buffer: the file contents and its line/column map.
// line_offsets_[i] is the byte offset of the start of line i+1; line_offsets_[0] == 0.
class SourceBuffer {
public:
    SourceBuffer(int file_id, std::string path, std::string contents);

    int file_id() const noexcept { return file_id_; }
    const std::string& path() const noexcept { return path_; }
    std::string_view contents() const noexcept { return contents_; }
    std::size_t size() const noexcept { return contents_.size(); }

    // Convert a byte offset to a (line, column) pair. Returns {0, 0} if the
    // offset is out of range.
    std::pair<std::uint32_t, std::uint32_t> offset_to_line_column(
        std::uint64_t offset) const noexcept;

    // Get the text of a line (1-based). Returns an empty string_view if the
    // line number is out of range.
    std::string_view line_text(std::uint32_t line) const noexcept;

private:
    int file_id_;
    std::string path_;
    std::string contents_;
    std::vector<std::uint64_t> line_offsets_;
};

// Manages all source files: loading, the include stack, and #line remapping.
class SourceManager {
public:
    SourceManager() = default;

    // Load a file from disk. Returns its file_id, or -1 if the file could not
    // be read. Loading the same path twice returns the existing file_id.
    int load_file(const std::string& path);

    // Get a source buffer by file_id. Returns nullptr if no such file exists.
    const SourceBuffer* get_buffer(int file_id) const noexcept;

    // Get all loaded file IDs.
    std::vector<int> file_ids() const noexcept;

    // Create a SourceLocation from a file_id and byte offset within that file.
    SourceLocation make_location(int file_id, std::uint64_t offset) const noexcept;

    // The include stack: currently open files, innermost file last.
    const std::vector<int>& include_stack() const noexcept { return include_stack_; }

    void push_include(int file_id);
    int pop_include();

private:
    int next_file_id_ = 0;
    std::vector<std::unique_ptr<SourceBuffer>> buffers_;
    std::unordered_map<std::string, int> path_to_id_;
    std::vector<int> include_stack_;
};

} // namespace krc

#endif // KRC_SOURCE_MANAGER_H
