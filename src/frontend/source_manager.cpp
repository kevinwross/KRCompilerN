#include "source_manager.h"

#include <cstdio>
#include <fstream>
#include <system_error>

namespace krc {

SourceBuffer::SourceBuffer(int file_id, std::string path, std::string contents)
    : file_id_(file_id),
      path_(std::move(path)),
      contents_(std::move(contents)) {
    line_offsets_.reserve(contents_.size() / 16 + 1);
    line_offsets_.push_back(0);
    for (std::size_t i = 0; i < contents_.size(); ++i) {
        if (contents_[static_cast<std::string::size_type>(i)] == '\n') {
            line_offsets_.push_back(
                static_cast<std::uint64_t>(i) + 1ULL);
        }
    }
}

std::pair<std::uint32_t, std::uint32_t>
SourceBuffer::offset_to_line_column(std::uint64_t offset) const noexcept {
    if (offset > contents_.size()) {
        return {0, 0};
    }

    // Binary search for the line containing the offset.
    std::size_t lo = 0;
    std::size_t hi = line_offsets_.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (line_offsets_[mid] <= offset) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    // lo is the number of line_offsets_ entries <= offset, so the line is lo
    // (1-based) and the start of that line is line_offsets_[lo - 1].
    const std::uint32_t line = static_cast<std::uint32_t>(lo);
    const std::uint64_t line_start = line_offsets_[static_cast<std::size_t>(lo) - 1];
    const std::uint32_t column =
        static_cast<std::uint32_t>(offset - line_start) + 1u;
    return {line, column};
}

std::string_view SourceBuffer::line_text(std::uint32_t line) const noexcept {
    if (line == 0 || line > line_offsets_.size()) {
        return {};
    }
    const std::uint64_t start = line_offsets_[static_cast<std::size_t>(line) - 1];
    const std::uint64_t end =
        (line < line_offsets_.size())
            ? line_offsets_[static_cast<std::size_t>(line)]
            : static_cast<std::uint64_t>(contents_.size());
    // Trim a trailing newline/carriage return.
    std::uint64_t actual_end = end;
    while (actual_end > start &&
           (contents_[static_cast<std::string::size_type>(actual_end) - 1] == '\n' ||
            contents_[static_cast<std::string::size_type>(actual_end) - 1] == '\r')) {
        --actual_end;
    }
    return std::string_view(
        contents_.data() + static_cast<std::string::size_type>(start),
        static_cast<std::string::size_type>(actual_end - start));
}

int SourceManager::load_file(const std::string& path) {
    auto it = path_to_id_.find(path);
    if (it != path_to_id_.end()) {
        return it->second;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return -1;
    }

    std::string contents((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    if (!file.eof() && file.fail()) {
        return -1;
    }

    const int file_id = next_file_id_++;
    auto buffer = std::make_unique<SourceBuffer>(file_id, path, std::move(contents));
    buffers_.push_back(std::move(buffer));
    path_to_id_[path] = file_id;
    return file_id;
}

const SourceBuffer* SourceManager::get_buffer(int file_id) const noexcept {
    if (file_id < 0 ||
        static_cast<std::size_t>(file_id) >= buffers_.size()) {
        return nullptr;
    }
    return buffers_[static_cast<std::size_t>(file_id)].get();
}

std::vector<int> SourceManager::file_ids() const noexcept {
    std::vector<int> ids;
    ids.reserve(buffers_.size());
    for (std::size_t i = 0; i < buffers_.size(); ++i)
        ids.push_back(static_cast<int>(i));
    return ids;
}

SourceLocation SourceManager::make_location(int file_id,
                                            std::uint64_t offset) const noexcept {
    SourceLocation loc;
    loc.file_id = file_id;
    loc.offset = offset;
    if (const SourceBuffer* buffer = get_buffer(file_id)) {
        const auto [line, column] = buffer->offset_to_line_column(offset);
        loc.line = line;
        loc.column = column;
    }
    return loc;
}

void SourceManager::push_include(int file_id) {
    include_stack_.push_back(file_id);
}

int SourceManager::pop_include() {
    if (include_stack_.empty()) {
        return -1;
    }
    const int file_id = include_stack_.back();
    include_stack_.pop_back();
    return file_id;
}

} // namespace krc
