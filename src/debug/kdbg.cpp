#include "kdbg.h"

#include <cstring>
#include <fstream>
#include <iomanip>
#include <istream>
#include <iterator>
#include <utility>

namespace krc {

namespace {

//------------------------------------------------------------------------------
// Little-endian helpers
//------------------------------------------------------------------------------

static void put_le16(std::vector<std::uint8_t>& buf, std::size_t off,
                     std::uint16_t v) {
    buf[off + 0] = static_cast<std::uint8_t>(v & 0xFF);
    buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

static void put_le32(std::vector<std::uint8_t>& buf, std::size_t off,
                     std::uint32_t v) {
    buf[off + 0] = static_cast<std::uint8_t>(v & 0xFF);
    buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    buf[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
    buf[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
}

static void put_le64(std::vector<std::uint8_t>& buf, std::size_t off,
                     std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
        buf[off + i] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
}

static std::uint16_t read_le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

static std::uint32_t read_le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

static std::uint64_t read_le64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= static_cast<std::uint64_t>(p[i]) << (i * 8);
    return v;
}

//------------------------------------------------------------------------------
// CRC32 (ISO 3309, same as object.cpp)
//------------------------------------------------------------------------------

static std::uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void crc32_init() {
    if (crc32_initialized)
        return;
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int j = 0; j < 8; ++j)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_initialized = true;
}

static std::uint32_t crc32_compute(const std::uint8_t* data, std::size_t len) {
    crc32_init();
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i)
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

} // namespace

//------------------------------------------------------------------------------
// String table
//------------------------------------------------------------------------------

std::uint32_t kdbg_add_string(KdbgContainer& c, std::string_view s) {
    auto it = c.string_offsets.find(std::string(s));
    if (it != c.string_offsets.end())
        return it->second;
    std::uint32_t off = static_cast<std::uint32_t>(c.string_table.size());
    c.string_table.insert(c.string_table.end(), s.begin(), s.end());
    c.string_table.push_back(0); // null terminator
    c.string_offsets[std::string(s)] = off;
    return off;
}

//------------------------------------------------------------------------------
// KDBG container writer
//------------------------------------------------------------------------------

bool write_kdbg(std::ostream& out, const KdbgContainer& c) {
    // Build all section payloads.
    std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> sections;

    // ---- .kdbg_header ----
    {
        std::vector<std::uint8_t> payload(56, 0);
        std::memcpy(payload.data(), KDBG_MAGIC, 4);
        put_le16(payload, 4, KDBG_VERSION_MAJOR);
        put_le16(payload, 6, KDBG_VERSION_MINOR);
        put_le32(payload, 8, c.target_numeric_id);
        put_le32(payload, 12, c.build_info.abi_version);
        payload[16] = c.pointer_width;
        payload[17] = c.endianness;
        // Build ID (16 bytes) at offset 18
        if (c.build_id.size() >= 16)
            std::memcpy(payload.data() + 18, c.build_id.data(), 16);
        else
            std::memcpy(payload.data() + 18, c.build_id.data(), c.build_id.size());
        put_le32(payload, 34,
                 static_cast<std::uint32_t>(c.sources.size())); // source count
        put_le32(payload, 38,
                 static_cast<std::uint32_t>(c.string_table.size())); // string table size
        // CRC at offset 42 (computed over the whole file later)
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Header),
                              std::move(payload));
    }

    // ---- .kdbg_src ----
    {
        std::vector<std::uint8_t> payload;
        for (const auto& src : c.sources) {
            std::vector<std::uint8_t> rec(48, 0);
            put_le32(rec, 0, static_cast<std::uint32_t>(KdbgSectionTag::Src));
            put_le32(rec, 4, 40); // payload length
            put_le32(rec, 8, src.file_index);
            put_le32(rec, 12, src.path_offset);
            put_le32(rec, 16, src.build_dir_offset);
            rec[20] = src.language;
            rec[21] = src.reserved[0];
            rec[22] = src.reserved[1];
            rec[23] = src.reserved[2];
            put_le64(rec, 24, src.checksum);
            payload.insert(payload.end(), rec.begin(), rec.end());
        }
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Src),
                              std::move(payload));
    }

    // ---- .kdbg_line ----
    {
        std::vector<std::uint8_t> payload;
        for (const auto& line : c.lines) {
            std::vector<std::uint8_t> rec(40, 0);
            put_le32(rec, 0, static_cast<std::uint32_t>(KdbgSectionTag::Line));
            put_le32(rec, 4, 32); // payload length
            put_le64(rec, 8, line.address);
            put_le64(rec, 16, line.size);
            put_le32(rec, 24, line.file_index);
            put_le32(rec, 28, line.line);
            put_le32(rec, 32, line.column);
            payload.insert(payload.end(), rec.begin(), rec.end());
        }
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Line),
                              std::move(payload));
    }

    // ---- .kdbg_func ----
    {
        std::vector<std::uint8_t> payload;
        for (const auto& fn : c.functions) {
            std::vector<std::uint8_t> rec(48, 0);
            put_le32(rec, 0, static_cast<std::uint32_t>(KdbgSectionTag::Func));
            put_le32(rec, 4, 40); // payload length
            put_le64(rec, 8, fn.address);
            put_le64(rec, 16, fn.size);
            put_le32(rec, 24, fn.name_offset);
            put_le32(rec, 28, fn.mangled_offset);
            put_le32(rec, 32, fn.return_type_id);
            put_le32(rec, 36, fn.frame_size);
            put_le32(rec, 40, fn.param_count);
            put_le32(rec, 44, fn.flags);
            payload.insert(payload.end(), rec.begin(), rec.end());
        }
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Func),
                              std::move(payload));
    }

    // ---- .kdbg_frame ----
    {
        std::vector<std::uint8_t> payload;
        for (const auto& fr : c.frames) {
            std::vector<std::uint8_t> rec(36, 0);
            put_le32(rec, 0, static_cast<std::uint32_t>(KdbgSectionTag::Frame));
            put_le32(rec, 4, 28); // payload length
            put_le64(rec, 8, fr.address);
            put_le32(rec, 16, fr.saved_fp_offset);
            put_le32(rec, 20, fr.saved_ra_offset);
            put_le32(rec, 24, fr.cfa_rule);
            put_le32(rec, 28, fr.flags);
            payload.insert(payload.end(), rec.begin(), rec.end());
        }
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Frame),
                              std::move(payload));
    }

    // ---- .kdbg_sym ----
    {
        std::vector<std::uint8_t> payload;
        for (const auto& sym : c.symbols) {
            std::vector<std::uint8_t> rec(40, 0);
            put_le32(rec, 0, static_cast<std::uint32_t>(KdbgSectionTag::Sym));
            put_le32(rec, 4, 32); // payload length
            put_le64(rec, 8, sym.value);
            put_le64(rec, 16, sym.size);
            put_le32(rec, 24, sym.name_offset);
            put_le32(rec, 28, sym.section_index);
            rec[32] = sym.binding;
            rec[33] = sym.type;
            rec[34] = sym.visibility;
            rec[35] = sym.flags;
            payload.insert(payload.end(), rec.begin(), rec.end());
        }
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Sym),
                              std::move(payload));
    }

    // ---- .kdbg_type ----
    {
        std::vector<std::uint8_t> payload;
        for (const auto& ty : c.types) {
            std::vector<std::uint8_t> rec(40, 0);
            put_le32(rec, 0, static_cast<std::uint32_t>(KdbgSectionTag::Type));
            put_le32(rec, 4, 32); // payload length
            put_le32(rec, 8, ty.type_id);
            rec[12] = ty.kind;
            rec[13] = ty.qualifier;
            put_le32(rec, 14, ty.name_offset);
            put_le32(rec, 18, ty.size);
            put_le32(rec, 22, ty.align);
            put_le32(rec, 26, ty.elem_type_id);
            put_le32(rec, 30, ty.elem_count);
            payload.insert(payload.end(), rec.begin(), rec.end());
        }
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Type),
                              std::move(payload));
    }

    // ---- .kdbg_var ----
    {
        std::vector<std::uint8_t> payload;
        for (const auto& var : c.variables) {
            std::vector<std::uint8_t> rec(40, 0);
            put_le32(rec, 0, static_cast<std::uint32_t>(KdbgSectionTag::Var));
            put_le32(rec, 4, 32); // payload length
            put_le64(rec, 8, var.address);
            put_le32(rec, 16, var.name_offset);
            put_le32(rec, 20, var.type_id);
            put_le32(rec, 24, var.scope_id);
            put_le32(rec, 28, var.decl_line);
            rec[32] = var.location_kind;
            rec[33] = var.location_reg;
            rec[34] = var.flags;
            rec[35] = var.reserved;
            payload.insert(payload.end(), rec.begin(), rec.end());
        }
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Var),
                              std::move(payload));
    }

    // ---- .kdbg_scope ----
    {
        std::vector<std::uint8_t> payload;
        for (const auto& sc : c.scopes) {
            std::vector<std::uint8_t> rec(44, 0);
            put_le32(rec, 0, static_cast<std::uint32_t>(KdbgSectionTag::Scope));
            put_le32(rec, 4, 36); // payload length
            put_le32(rec, 8, sc.scope_id);
            put_le32(rec, 12, sc.parent_scope_id);
            put_le64(rec, 16, sc.address);
            put_le64(rec, 24, sc.size);
            put_le32(rec, 32, sc.name_offset);
            rec[36] = sc.kind;
            rec[37] = sc.flags;
            payload.insert(payload.end(), rec.begin(), rec.end());
        }
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Scope),
                              std::move(payload));
    }

    // ---- .kdbg_str ----
    {
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Str),
                              c.string_table);
    }

    // ---- .kdbg_build ----
    {
        std::vector<std::uint8_t> payload(40, 0);
        put_le32(payload, 0, static_cast<std::uint32_t>(KdbgSectionTag::Build));
        put_le32(payload, 4, 32); // payload length
        put_le32(payload, 8, c.build_info.compiler_major);
        put_le32(payload, 12, c.build_info.compiler_minor);
        put_le32(payload, 16, c.build_info.target_features);
        put_le32(payload, 20, c.build_info.abi_version);
        put_le32(payload, 24, c.build_info.build_id_offset);
        put_le64(payload, 28, c.build_info.source_hash);
        sections.emplace_back(static_cast<std::uint32_t>(KdbgSectionTag::Build),
                              std::move(payload));
    }

    // ---- Build the file ----
    // Layout:
    //   [container header: 52 bytes]
    //   [section directory: N * 24 bytes]
    //   [string table]
    //   [section payloads...]
    //   [CRC32: 4 bytes]

    const std::uint32_t section_count = static_cast<std::uint32_t>(sections.size());
    const std::uint64_t container_header_size = 52;
    const std::uint64_t directory_size =
        static_cast<std::uint64_t>(section_count) * 24;

    // Compute offsets: directory, then string table, then payloads.
    std::uint64_t offset = container_header_size + directory_size;

    // Place string table right after directory.
    std::uint64_t string_table_off = offset;
    offset += c.string_table.size();

    // Place each section payload after the string table.
    std::vector<std::uint64_t> section_payload_offsets(section_count);
    for (std::uint32_t i = 0; i < section_count; ++i) {
        section_payload_offsets[i] = offset;
        offset += sections[i].second.size();
    }

    // Total file size (excluding CRC, which is appended at the end).
    std::uint64_t content_size = offset;

    // Build the content buffer (without CRC).
    std::vector<std::uint8_t> content(content_size, 0);
    std::size_t fpos = 0;

    // Container header (52 bytes).
    put_le32(content, 0, 0x4742444B); // "KDBG" little-endian
    put_le16(content, 4, KDBG_VERSION_MAJOR);
    put_le16(content, 6, KDBG_VERSION_MINOR);
    put_le32(content, 8, c.target_numeric_id);
    put_le32(content, 12, c.build_info.abi_version);
    content[16] = c.pointer_width;
    content[17] = c.endianness;
    if (c.build_id.size() >= 16)
        std::memcpy(content.data() + 18, c.build_id.data(), 16);
    else
        std::memcpy(content.data() + 18, c.build_id.data(), c.build_id.size());
    put_le32(content, 34, section_count);
    put_le32(content, 38, static_cast<std::uint32_t>(c.string_table.size()));
    put_le64(content, 42, string_table_off);

    fpos = container_header_size;

    // Section directory: tag(4) + offset(8) + size(8) = 20 bytes (padded to 24)
    for (std::uint32_t i = 0; i < section_count; ++i) {
        put_le32(content, fpos, sections[i].first);        // tag
        put_le64(content, fpos + 4, section_payload_offsets[i]); // offset
        put_le64(content, fpos + 12, sections[i].second.size()); // size
        fpos += 24;
    }

    // String table.
    std::memcpy(content.data() + fpos, c.string_table.data(), c.string_table.size());
    fpos += c.string_table.size();

    // Section payloads.
    for (std::uint32_t i = 0; i < section_count; ++i) {
        std::memcpy(content.data() + fpos, sections[i].second.data(),
                    sections[i].second.size());
        fpos += sections[i].second.size();
    }

    // Append CRC32: compute over content with CRC field (at end) zeroed.
    std::uint32_t crc = crc32_compute(content.data(), content.size());

    // Build final file: content + CRC.
    std::vector<std::uint8_t> file(content_size + 4);
    std::memcpy(file.data(), content.data(), content_size);
    put_le32(file, content_size, crc);

    out.write(reinterpret_cast<const char*>(file.data()),
              static_cast<std::streamsize>(file.size()));
    return true;
}

bool read_kdbg(std::istream& in, KdbgContainer& c) {
    // Read the entire file into memory.
    std::vector<std::uint8_t> file(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    if (file.size() < 56)  // minimum: 52-byte header + 4-byte CRC
        return false;

    const std::uint8_t* p = file.data();

    // ---- Validate magic ----
    if (std::memcmp(p, KDBG_MAGIC, 4) != 0)
        return false;

    // ---- Parse container header (52 bytes) ----
    std::uint16_t version_major = read_le16(p + 4);
    std::uint16_t version_minor = read_le16(p + 6);
    (void)version_minor;
    if (version_major != KDBG_VERSION_MAJOR)
        return false;

    c.target_numeric_id = read_le32(p + 8);
    c.build_info.abi_version = read_le32(p + 12);
    c.pointer_width = p[16];
    c.endianness = p[17];
    c.build_id.assign(p + 18, p + 34);  // 16 bytes

    std::uint32_t section_count = read_le32(p + 34);
    std::uint32_t string_table_size = read_le32(p + 38);
    std::uint64_t string_table_off = read_le64(p + 42);

    // CRC is at the end of the file.
    if (file.size() < 56)
        return false;
    std::uint32_t stored_crc = read_le32(p + file.size() - 4);

    // Validate CRC: compute over content (everything except the last 4 CRC bytes).
    {
        std::vector<std::uint8_t> file_copy = file;
        std::memcpy(file_copy.data() + file.size() - 4, "\0\0\0\0", 4);
        std::uint32_t computed = crc32_compute(file_copy.data(), file.size() - 4);
        if (computed != stored_crc)
            return false;
    }

    // ---- Parse section directory ----
    std::uint64_t dir_off = 52;
    std::uint64_t dir_end = dir_off + static_cast<std::uint64_t>(section_count) * 24;
    if (dir_end > file.size())
        return false;

    // Map: section tag -> (offset, size)
    std::unordered_map<std::uint32_t, std::pair<std::uint64_t, std::uint64_t>> section_map;

    for (std::uint32_t i = 0; i < section_count; ++i) {
        std::uint32_t tag = read_le32(p + dir_off + i * 24 + 0);
        std::uint64_t offset = read_le64(p + dir_off + i * 24 + 4);
        std::uint64_t size = read_le64(p + dir_off + i * 24 + 12);
        section_map[tag] = {offset, size};
    }

    // ---- Read string table ----
    if (string_table_off + string_table_size > file.size())
        return false;
    c.string_table.assign(p + string_table_off,
                          p + string_table_off + string_table_size);

    // Build string offset map.
    std::uint64_t str_cursor = 0;
    while (str_cursor < string_table_size) {
        const char* s = reinterpret_cast<const char*>(p + string_table_off + str_cursor);
        std::uint32_t off = static_cast<std::uint32_t>(str_cursor);
        size_t len = std::strlen(s);
        c.string_offsets[std::string(s)] = off;
        str_cursor += len + 1;
    }

    // Helper to read a section's records.
    auto read_records = [&](std::uint32_t tag) -> std::vector<std::uint8_t> {
        auto it = section_map.find(tag);
        if (it == section_map.end()) return {};
        std::uint64_t off = it->second.first;
        std::uint64_t sz = it->second.second;
        if (off + sz > file.size()) return {};
        return std::vector<std::uint8_t>(p + off, p + off + sz);
    };

    // ---- .kdbg_header (tag 0) ----
    {
        auto hdr = read_records(static_cast<std::uint32_t>(KdbgSectionTag::Header));
        if (!hdr.empty()) {
            c.target_id = "kr32";  // derive from target_numeric_id
            switch (c.target_numeric_id) {
                case 1: c.target_id = "kr32"; break;
                case 2: c.target_id = "kr16"; break;
                case 3: c.target_id = "kr64"; break;
                default: c.target_id = "unknown"; break;
            }
        }
    }

    // ---- .kdbg_src (tag 1) ----
    {
        auto src_sec = read_records(static_cast<std::uint32_t>(KdbgSectionTag::Src));
        std::size_t off = 0;
        while (off + 8 <= src_sec.size()) {
            std::uint32_t rec_tag = read_le32(src_sec.data() + off);
            std::uint32_t rec_len = read_le32(src_sec.data() + off + 4);
            if (rec_tag != static_cast<std::uint32_t>(KdbgSectionTag::Src)) break;
            if (off + 8 + rec_len > src_sec.size()) break;

            KdbgSrcRecord src;
            src.file_index = read_le32(src_sec.data() + off + 8);
            src.path_offset = read_le32(src_sec.data() + off + 12);
            src.build_dir_offset = read_le32(src_sec.data() + off + 16);
            src.language = src_sec.data()[off + 20];
            src.reserved[0] = src_sec.data()[off + 21];
            src.reserved[1] = src_sec.data()[off + 22];
            src.reserved[2] = src_sec.data()[off + 23];
            src.checksum = read_le64(src_sec.data() + off + 24);

            c.sources.push_back(std::move(src));
            off += 8 + rec_len;
        }
    }

    // ---- .kdbg_line (tag 2) ----
    {
        auto line_sec = read_records(static_cast<std::uint32_t>(KdbgSectionTag::Line));
        std::size_t off = 0;
        while (off + 8 <= line_sec.size()) {
            std::uint32_t rec_tag = read_le32(line_sec.data() + off);
            std::uint32_t rec_len = read_le32(line_sec.data() + off + 4);
            if (rec_tag != static_cast<std::uint32_t>(KdbgSectionTag::Line)) break;
            if (off + 8 + rec_len > line_sec.size()) break;

            KdbgLineRecord line;
            line.address = read_le64(line_sec.data() + off + 8);
            line.size = read_le64(line_sec.data() + off + 16);
            line.file_index = read_le32(line_sec.data() + off + 24);
            line.line = read_le32(line_sec.data() + off + 28);
            line.column = read_le32(line_sec.data() + off + 32);

            c.lines.push_back(std::move(line));
            off += 8 + rec_len;
        }
    }

    // ---- .kdbg_func (tag 3) ----
    {
        auto func_sec = read_records(static_cast<std::uint32_t>(KdbgSectionTag::Func));
        std::size_t off = 0;
        while (off + 8 <= func_sec.size()) {
            std::uint32_t rec_tag = read_le32(func_sec.data() + off);
            std::uint32_t rec_len = read_le32(func_sec.data() + off + 4);
            if (rec_tag != static_cast<std::uint32_t>(KdbgSectionTag::Func)) break;
            if (off + 8 + rec_len > func_sec.size()) break;

            KdbgFuncRecord fn;
            fn.address = read_le64(func_sec.data() + off + 8);
            fn.size = read_le64(func_sec.data() + off + 16);
            fn.name_offset = read_le32(func_sec.data() + off + 24);
            fn.mangled_offset = read_le32(func_sec.data() + off + 28);
            fn.return_type_id = read_le32(func_sec.data() + off + 32);
            fn.frame_size = read_le32(func_sec.data() + off + 36);
            fn.param_count = read_le32(func_sec.data() + off + 40);
            fn.flags = read_le32(func_sec.data() + off + 44);

            c.functions.push_back(std::move(fn));
            off += 8 + rec_len;
        }
    }

    // ---- .kdbg_frame (tag 4) ----
    {
        auto frame_sec = read_records(static_cast<std::uint32_t>(KdbgSectionTag::Frame));
        std::size_t off = 0;
        while (off + 8 <= frame_sec.size()) {
            std::uint32_t rec_tag = read_le32(frame_sec.data() + off);
            std::uint32_t rec_len = read_le32(frame_sec.data() + off + 4);
            if (rec_tag != static_cast<std::uint32_t>(KdbgSectionTag::Frame)) break;
            if (off + 8 + rec_len > frame_sec.size()) break;

            KdbgFrameRecord fr;
            fr.address = read_le64(frame_sec.data() + off + 8);
            fr.saved_fp_offset = read_le32(frame_sec.data() + off + 16);
            fr.saved_ra_offset = read_le32(frame_sec.data() + off + 20);
            fr.cfa_rule = read_le32(frame_sec.data() + off + 24);
            fr.flags = read_le32(frame_sec.data() + off + 28);

            c.frames.push_back(std::move(fr));
            off += 8 + rec_len;
        }
    }

    // ---- .kdbg_sym (tag 5) ----
    {
        auto sym_sec = read_records(static_cast<std::uint32_t>(KdbgSectionTag::Sym));
        std::size_t off = 0;
        while (off + 8 <= sym_sec.size()) {
            std::uint32_t rec_tag = read_le32(sym_sec.data() + off);
            std::uint32_t rec_len = read_le32(sym_sec.data() + off + 4);
            if (rec_tag != static_cast<std::uint32_t>(KdbgSectionTag::Sym)) break;
            if (off + 8 + rec_len > sym_sec.size()) break;

            KdbgSymRecord sym;
            sym.value = read_le64(sym_sec.data() + off + 8);
            sym.size = read_le64(sym_sec.data() + off + 16);
            sym.name_offset = read_le32(sym_sec.data() + off + 24);
            sym.section_index = read_le32(sym_sec.data() + off + 28);
            sym.binding = sym_sec.data()[off + 32];
            sym.type = sym_sec.data()[off + 33];
            sym.visibility = sym_sec.data()[off + 34];
            sym.flags = sym_sec.data()[off + 35];

            c.symbols.push_back(std::move(sym));
            off += 8 + rec_len;
        }
    }

    // ---- .kdbg_type (tag 6) ----
    {
        auto type_sec = read_records(static_cast<std::uint32_t>(KdbgSectionTag::Type));
        std::size_t off = 0;
        while (off + 8 <= type_sec.size()) {
            std::uint32_t rec_tag = read_le32(type_sec.data() + off);
            std::uint32_t rec_len = read_le32(type_sec.data() + off + 4);
            if (rec_tag != static_cast<std::uint32_t>(KdbgSectionTag::Type)) break;
            if (off + 8 + rec_len > type_sec.size()) break;

            KdbgTypeRecord ty;
            ty.type_id = read_le32(type_sec.data() + off + 8);
            ty.kind = type_sec.data()[off + 12];
            ty.qualifier = type_sec.data()[off + 13];
            ty.name_offset = read_le32(type_sec.data() + off + 14);
            ty.size = read_le32(type_sec.data() + off + 18);
            ty.align = read_le32(type_sec.data() + off + 22);
            ty.elem_type_id = read_le32(type_sec.data() + off + 26);
            ty.elem_count = read_le32(type_sec.data() + off + 30);

            c.types.push_back(std::move(ty));
            off += 8 + rec_len;
        }
    }

    // ---- .kdbg_var (tag 7) ----
    {
        auto var_sec = read_records(static_cast<std::uint32_t>(KdbgSectionTag::Var));
        std::size_t off = 0;
        while (off + 8 <= var_sec.size()) {
            std::uint32_t rec_tag = read_le32(var_sec.data() + off);
            std::uint32_t rec_len = read_le32(var_sec.data() + off + 4);
            if (rec_tag != static_cast<std::uint32_t>(KdbgSectionTag::Var)) break;
            if (off + 8 + rec_len > var_sec.size()) break;

            KdbgVarRecord var;
            var.address = read_le64(var_sec.data() + off + 8);
            var.name_offset = read_le32(var_sec.data() + off + 16);
            var.type_id = read_le32(var_sec.data() + off + 20);
            var.scope_id = read_le32(var_sec.data() + off + 24);
            var.decl_line = read_le32(var_sec.data() + off + 28);
            var.location_kind = var_sec.data()[off + 32];
            var.location_reg = var_sec.data()[off + 33];
            var.flags = var_sec.data()[off + 34];
            var.reserved = var_sec.data()[off + 35];

            c.variables.push_back(std::move(var));
            off += 8 + rec_len;
        }
    }

    // ---- .kdbg_scope (tag 8) ----
    {
        auto scope_sec = read_records(static_cast<std::uint32_t>(KdbgSectionTag::Scope));
        std::size_t off = 0;
        while (off + 8 <= scope_sec.size()) {
            std::uint32_t rec_tag = read_le32(scope_sec.data() + off);
            std::uint32_t rec_len = read_le32(scope_sec.data() + off + 4);
            if (rec_tag != static_cast<std::uint32_t>(KdbgSectionTag::Scope)) break;
            if (off + 8 + rec_len > scope_sec.size()) break;

            KdbgScopeRecord sc;
            sc.scope_id = read_le32(scope_sec.data() + off + 8);
            sc.parent_scope_id = read_le32(scope_sec.data() + off + 12);
            sc.address = read_le64(scope_sec.data() + off + 16);
            sc.size = read_le64(scope_sec.data() + off + 24);
            sc.name_offset = read_le32(scope_sec.data() + off + 32);
            sc.kind = scope_sec.data()[off + 36];
            sc.flags = scope_sec.data()[off + 37];

            c.scopes.push_back(std::move(sc));
            off += 8 + rec_len;
        }
    }

    // ---- .kdbg_str (tag 9) ----
    // String table already read above.

    // ---- .kdbg_build (tag 10) ----
    {
        auto build_sec = read_records(static_cast<std::uint32_t>(KdbgSectionTag::Build));
        if (build_sec.size() >= 40) {
            std::uint32_t rec_tag = read_le32(build_sec.data() + 0);
            (void)read_le32(build_sec.data() + 4); // rec_len, not needed
            if (rec_tag == static_cast<std::uint32_t>(KdbgSectionTag::Build)) {
                c.build_info.compiler_major = read_le32(build_sec.data() + 8);
                c.build_info.compiler_minor = read_le32(build_sec.data() + 12);
                c.build_info.target_features = read_le32(build_sec.data() + 16);
                c.build_info.abi_version = read_le32(build_sec.data() + 20);
                c.build_info.build_id_offset = read_le32(build_sec.data() + 24);
                c.build_info.source_hash = read_le64(build_sec.data() + 28);
            }
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// KDBG dumper (text output)
//------------------------------------------------------------------------------

namespace {

const char* binding_name(std::uint8_t b) {
    switch (static_cast<SymbolBinding>(b)) {
        case SymbolBinding::Local: return "local";
        case SymbolBinding::Global: return "global";
        case SymbolBinding::Weak: return "weak";
        default: return "unknown";
    }
}

const char* type_name(std::uint8_t t) {
    switch (static_cast<SymbolType>(t)) {
        case SymbolType::Notype: return "notype";
        case SymbolType::Function: return "func";
        case SymbolType::Object: return "object";
        default: return "unknown";
    }
}

const char* visibility_name(std::uint8_t v) {
    switch (static_cast<SymbolVisibility>(v)) {
        case SymbolVisibility::Default: return "default";
        case SymbolVisibility::Internal: return "internal";
        case SymbolVisibility::Hidden: return "hidden";
        default: return "unknown";
    }
}

std::string read_string_at(const std::vector<std::uint8_t>& str_table,
                           std::uint32_t off) {
    if (off >= str_table.size()) return "";
    const char* s = reinterpret_cast<const char*>(str_table.data() + off);
    return std::string(s);
}

} // namespace

bool dump_kdbg(std::ostream& out, const KdbgContainer& c) {
    out << "KDBG Container Dump\n";
    out << "===================\n\n";

    out << "Header:\n";
    out << "  target_id:        " << c.target_id << "\n";
    out << "  target_numeric_id: " << c.target_numeric_id << "\n";
    out << "  pointer_width:    " << static_cast<int>(c.pointer_width) << "\n";
    out << "  endianness:       " << static_cast<int>(c.endianness)
        << " (0=little, 1=big)\n";
    out << "  build_id:         ";
    for (std::size_t i = 0; i < c.build_id.size(); ++i)
        out << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(c.build_id[i]);
    out << std::dec << "\n";
    out << "  abi_version:      " << c.build_info.abi_version << "\n";
    out << "  string_table:     " << c.string_table.size() << " bytes\n\n";

    out << "Sources (" << c.sources.size() << "):\n";
    for (const auto& s : c.sources) {
        out << "  file_index=" << s.file_index
            << " path_offset=" << s.path_offset
            << " build_dir_offset=" << s.build_dir_offset
            << " language=" << static_cast<int>(s.language)
            << " checksum=0x" << std::hex << s.checksum << std::dec
            << " path=" << read_string_at(c.string_table, s.path_offset)
            << "\n";
    }

    out << "\nLines (" << c.lines.size() << "):\n";
    for (const auto& l : c.lines) {
        out << "  address=0x" << std::hex << l.address << std::dec
            << " size=" << l.size
            << " file_index=" << l.file_index
            << " line=" << l.line
            << " column=" << l.column << "\n";
    }

    out << "\nFunctions (" << c.functions.size() << "):\n";
    for (const auto& f : c.functions) {
        out << "  address=0x" << std::hex << f.address << std::dec
            << " size=" << f.size
            << " name=" << read_string_at(c.string_table, f.name_offset)
            << " mangled=" << read_string_at(c.string_table, f.mangled_offset)
            << " return_type_id=" << f.return_type_id
            << " frame_size=" << f.frame_size
            << " param_count=" << f.param_count
            << " flags=0x" << std::hex << f.flags << std::dec << "\n";
    }

    out << "\nFrames (" << c.frames.size() << "):\n";
    for (const auto& f : c.frames) {
        out << "  address=0x" << std::hex << f.address << std::dec
            << " saved_fp_offset=" << f.saved_fp_offset
            << " saved_ra_offset=" << f.saved_ra_offset
            << " cfa_rule=" << f.cfa_rule
            << " flags=" << f.flags << "\n";
    }

    out << "\nSymbols (" << c.symbols.size() << "):\n";
    for (const auto& s : c.symbols) {
        out << "  value=0x" << std::hex << s.value << std::dec
            << " size=" << s.size
            << " name=" << read_string_at(c.string_table, s.name_offset)
            << " section_index=" << s.section_index
            << " binding=" << binding_name(s.binding)
            << " type=" << type_name(s.type)
            << " visibility=" << visibility_name(s.visibility)
            << " flags=0x" << std::hex << (int)s.flags << std::dec << "\n";
    }

    out << "\nTypes (" << c.types.size() << "):\n";
    for (const auto& t : c.types) {
        out << "  type_id=" << t.type_id
            << " kind=" << static_cast<int>(t.kind)
            << " qualifier=" << static_cast<int>(t.qualifier)
            << " name=" << read_string_at(c.string_table, t.name_offset)
            << " size=" << t.size
            << " align=" << t.align
            << " elem_type_id=" << t.elem_type_id
            << " elem_count=" << t.elem_count << "\n";
    }

    out << "\nVariables (" << c.variables.size() << "):\n";
    for (const auto& v : c.variables) {
        out << "  address=0x" << std::hex << v.address << std::dec
            << " name=" << read_string_at(c.string_table, v.name_offset)
            << " type_id=" << v.type_id
            << " scope_id=" << v.scope_id
            << " decl_line=" << v.decl_line
            << " location_kind=" << static_cast<int>(v.location_kind)
            << " location_reg=" << static_cast<int>(v.location_reg)
            << " flags=0x" << std::hex << (int)v.flags << std::dec << "\n";
    }

    out << "\nScopes (" << c.scopes.size() << "):\n";
    for (const auto& s : c.scopes) {
        out << "  scope_id=" << s.scope_id
            << " parent_scope_id=" << s.parent_scope_id
            << " address=0x" << std::hex << s.address << std::dec
            << " size=" << s.size
            << " name=" << read_string_at(c.string_table, s.name_offset)
            << " kind=" << static_cast<int>(s.kind)
            << " flags=" << s.flags << "\n";
    }

    out << "\nBuild Info:\n";
    out << "  compiler_major:  " << c.build_info.compiler_major << "\n";
    out << "  compiler_minor:  " << c.build_info.compiler_minor << "\n";
    out << "  target_features: " << c.build_info.target_features << "\n";
    out << "  abi_version:     " << c.build_info.abi_version << "\n";
    out << "  build_id_offset: " << c.build_info.build_id_offset << "\n";
    out << "  source_hash:     0x" << std::hex << c.build_info.source_hash
        << std::dec << "\n";

    out << "\nString Table (" << c.string_table.size() << " bytes):\n";
    std::uint64_t cursor = 0;
    while (cursor < c.string_table.size()) {
        const char* s = reinterpret_cast<const char*>(
            c.string_table.data() + cursor);
        out << "  [" << cursor << "] " << s << "\n";
        cursor += std::strlen(s) + 1;
    }

    return out.good();
}

} // namespace krc
