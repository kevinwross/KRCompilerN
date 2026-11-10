#include "object.h"

#include <cstring>
#include <istream>
#include <iterator>
#include <vector>

namespace krc {

//------------------------------------------------------------------------------
// String table
//------------------------------------------------------------------------------

std::uint32_t object_add_string(ObjectFile& obj, std::string_view s) {
    auto it = obj.string_offsets.find(std::string(s));
    if (it != obj.string_offsets.end()) {
        return it->second;
    }
    std::uint32_t offset = static_cast<std::uint32_t>(obj.string_table.size());
    obj.string_table.insert(obj.string_table.end(), s.begin(), s.end());
    obj.string_table.push_back('\0'); // null-terminated
    obj.string_offsets[std::string(s)] = offset;
    return offset;
}

//------------------------------------------------------------------------------
// Little-endian write helpers (operate on a byte vector)
//------------------------------------------------------------------------------

static void put_le16(std::vector<std::uint8_t>& buf, std::size_t off,
                     std::uint16_t v) {
    buf[off] = static_cast<std::uint8_t>(v & 0xFF);
    buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

static void put_le32(std::vector<std::uint8_t>& buf, std::size_t off,
                     std::uint32_t v) {
    buf[off] = static_cast<std::uint8_t>(v & 0xFF);
    buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    buf[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
    buf[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
}

static void put_le64(std::vector<std::uint8_t>& buf, std::size_t off,
                     std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
        buf[off + i] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
}

//------------------------------------------------------------------------------
// CRC32
//------------------------------------------------------------------------------

static std::uint32_t crc32_compute(const std::uint8_t* data, std::size_t len) {
    static std::uint32_t table[256];
    static bool initialized = false;
    if (!initialized) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        initialized = true;
    }
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

//------------------------------------------------------------------------------
// Object writer
//------------------------------------------------------------------------------

// KRO header layout (plan.md §11.1), total 120 bytes:
//   0-3   Magic "KRO\0"                      [4]
//   4-5   Version major/minor                [2]
//   6-7   Kind (object/executable/archive)   [2]
//   8-9   Header size                        [2]
//   10-11 Flags                              [2]
//   12-15 Target ID                          [4]
//   16-19 ABI version                        [4]
//   20    Pointer width (16/32/64)           [1]
//   21    Endianness (0=LE, 1=BE)            [1]
//   22-23 Reserved (zero)                    [2]
//   24-27 Section count                      [4]
//   28-31 Symbol count                       [4]
//   32-35 Relocation count                   [4]
//   36-43 String table offset                [8]
//   44-51 String table size                  [8]
//   52-59 Debug table offset                 [8]
//   60-67 Debug table size                   [8]
//   68-75 Entry address                      [8]
//   76-83 Image base                         [8]
//   84-91 File size                          [8]
//   92-107 Build ID                          [16]
//   108-111 CRC32 (zeroed for computation)   [4]
//   112-119 Reserved (zero)                  [8]
static constexpr std::size_t KRO_HEADER_SIZE = 120;
static constexpr std::size_t KRO_SECTION_ENTRY_SIZE = 52;
static constexpr std::size_t KRO_SYMBOL_ENTRY_SIZE = 32;
static constexpr std::size_t KRO_RELOCATION_ENTRY_SIZE = 32;

bool write_kro_object(std::ostream& out, const ObjectFile& obj) {
    // Build string table from all names referenced by sections/symbols.
    std::vector<std::uint8_t> string_table;
    std::unordered_map<std::string, std::uint32_t> string_offsets;

    auto add_string = [&](std::string_view s) -> std::uint32_t {
        auto it = string_offsets.find(std::string(s));
        if (it != string_offsets.end())
            return it->second;
        std::uint32_t off = static_cast<std::uint32_t>(string_table.size());
        string_table.insert(string_table.end(), s.begin(), s.end());
        string_table.push_back('\0');
        string_offsets[std::string(s)] = off;
        return off;
    };

    // Add all section and symbol names.
    for (const auto& sec : obj.sections)
        add_string(sec.name);
    for (const auto& sym : obj.symbols)
        add_string(sym.name);

    // Compute layout offsets.
    std::size_t section_table_off = KRO_HEADER_SIZE;
    std::size_t symbol_table_off =
        section_table_off +
        obj.sections.size() * KRO_SECTION_ENTRY_SIZE;
    std::size_t relocation_table_off =
        symbol_table_off + obj.symbols.size() * KRO_SYMBOL_ENTRY_SIZE;
    std::size_t string_table_off =
        relocation_table_off + obj.relocations.size() * KRO_RELOCATION_ENTRY_SIZE;
    std::size_t debug_table_off = string_table_off + string_table.size();

    // Section payloads follow debug table, each aligned.
    std::size_t payload_off = debug_table_off + obj.debug_table.size();
    std::vector<std::size_t> section_payload_offsets(obj.sections.size());
    for (std::size_t i = 0; i < obj.sections.size(); ++i) {
        std::size_t align = obj.sections[i].alignment;
        if (align < 1) align = 1;
        payload_off = (payload_off + align - 1) / align * align;
        section_payload_offsets[i] = payload_off;
        payload_off += obj.sections[i].payload.size();
    }
    std::size_t file_size = payload_off;

    // Build ID: deterministic hash of target_id + section contents.
    std::uint8_t build_id[16] = {0};
    {
        std::string hash_input = obj.target_id;
        for (const auto& sec : obj.sections)
            hash_input.append(reinterpret_cast<const char*>(sec.payload.data()),
                              sec.payload.size());
        std::uint32_t crc = crc32_compute(
            reinterpret_cast<const std::uint8_t*>(hash_input.data()),
            hash_input.size());
        std::memcpy(build_id, &crc, 4);
    }

    // Build the entire file in memory.
    std::vector<std::uint8_t> file;
    file.resize(file_size);

    // ---- Header ----
    // Magic
    std::memcpy(file.data(), KRO_MAGIC, 4);
    // Version (major=1, minor=0)
    file[4] = 1;
    file[5] = 0;
    // Kind
    put_le16(file, 6, static_cast<std::uint16_t>(obj.kind));
    // Header size
    put_le16(file, 8, static_cast<std::uint16_t>(KRO_HEADER_SIZE));
    // Flags
    put_le16(file, 10, obj.flags);
    // Target ID
    put_le32(file, 12, obj.target_numeric_id);
    // ABI version
    put_le32(file, 16, obj.abi_version);
    // Pointer width
    file[20] = obj.pointer_width;
    // Endianness
    file[21] = obj.endianness;
    // Reserved (zero) - already zeroed by resize
    // Section count
    put_le32(file, 24, static_cast<std::uint32_t>(obj.sections.size()));
    // Symbol count
    put_le32(file, 28, static_cast<std::uint32_t>(obj.symbols.size()));
    // Relocation count
    put_le32(file, 32, static_cast<std::uint32_t>(obj.relocations.size()));
    // String table offset/size
    put_le64(file, 36, static_cast<std::uint64_t>(string_table_off));
    put_le64(file, 44, static_cast<std::uint64_t>(string_table.size()));
    // Debug table offset/size
    put_le64(file, 52, static_cast<std::uint64_t>(debug_table_off));
    put_le64(file, 60, static_cast<std::uint64_t>(obj.debug_table.size()));
    // Entry address / Image base (zero for objects) - already zeroed
    // File size
    put_le64(file, 84, static_cast<std::uint64_t>(file_size));
    // Build ID
    std::memcpy(file.data() + 92, build_id, 16);
    // CRC32 (zeroed for now at 108-111) - already zeroed
    // Reserved (zero at 112-119) - already zeroed

    // ---- Section table ----
    for (std::size_t i = 0; i < obj.sections.size(); ++i) {
        const auto& sec = obj.sections[i];
        std::size_t off = section_table_off + i * KRO_SECTION_ENTRY_SIZE;
        std::uint32_t name_off = add_string(sec.name);
        put_le32(file, off + 0, name_off);                       // Name offset
        put_le16(file, off + 4, static_cast<std::uint16_t>(sec.type)); // Type
        put_le16(file, off + 6, sec.flags);                      // Flags
        put_le64(file, off + 8, sec.address);                    // Address
        put_le64(file, off + 16, static_cast<std::uint64_t>(section_payload_offsets[i])); // Payload offset
        put_le64(file, off + 24, sec.payload.size());            // Size
        put_le32(file, off + 32, static_cast<std::uint32_t>(sec.alignment)); // Alignment
        put_le32(file, off + 36, sec.link_info);                 // Link/info
        put_le32(file, off + 40, sec.entry_size);                // Entry size
        // Reserved at off+44 to off+51 (zero)
    }

    // ---- Symbol table ----
    for (std::size_t i = 0; i < obj.symbols.size(); ++i) {
        const auto& sym = obj.symbols[i];
        std::size_t off = symbol_table_off + i * KRO_SYMBOL_ENTRY_SIZE;
        std::uint32_t name_off = add_string(sym.name);
        put_le32(file, off + 0, name_off);                       // Name offset
        put_le64(file, off + 4, sym.value);                      // Value
        put_le64(file, off + 12, sym.size);                      // Size
        put_le32(file, off + 20, sym.section_index);             // Section index
        file[off + 24] = static_cast<std::uint8_t>(sym.binding); // Binding
        file[off + 25] = static_cast<std::uint8_t>(sym.type);    // Type
        file[off + 26] =
            static_cast<std::uint8_t>(sym.visibility);           // Visibility
        file[off + 27] = sym.flags;                              // Flags
        put_le32(file, off + 28, sym.target_specific);           // Target-specific
        // Reserved at off+32 to off+35 (zero)
    }

    // ---- Relocation table ----
    for (std::size_t i = 0; i < obj.relocations.size(); ++i) {
        const auto& rel = obj.relocations[i];
        std::size_t off = relocation_table_off + i * KRO_RELOCATION_ENTRY_SIZE;
        put_le32(file, off + 0, rel.section_index);  // Section index
        put_le64(file, off + 4, rel.offset);         // Offset
        put_le32(file, off + 12,
                 static_cast<std::uint32_t>(rel.type)); // Type
        put_le32(file, off + 16, rel.symbol_index);    // Symbol index
        put_le64(file, off + 20,
                 static_cast<std::uint64_t>(rel.addend)); // Addend
        file[off + 28] = rel.width;                        // Width
        file[off + 29] = rel.flags;                        // Flags
        // off+30, off+31 zero
    }

    // ---- String table ----
    std::memcpy(file.data() + string_table_off, string_table.data(),
                string_table.size());

    // ---- Debug table ----
    if (!obj.debug_table.empty())
        std::memcpy(file.data() + debug_table_off, obj.debug_table.data(),
                    obj.debug_table.size());

    // ---- Section payloads ----
    for (std::size_t i = 0; i < obj.sections.size(); ++i) {
        const auto& sec = obj.sections[i];
        if (!sec.payload.empty())
            std::memcpy(file.data() + section_payload_offsets[i],
                        sec.payload.data(), sec.payload.size());
    }

    // ---- CRC32 (over file with CRC field zeroed) ----
    std::uint32_t crc = crc32_compute(file.data(), file.size());
    put_le32(file, 108, crc);

    // Write to output stream.
    out.write(reinterpret_cast<const char*>(file.data()),
              static_cast<std::streamsize>(file.size()));
    return out.good();
}

//------------------------------------------------------------------------------
// Object reader
//------------------------------------------------------------------------------

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

bool read_kro_object(std::istream& in, ObjectFile& obj) {
    // Read the entire file into memory.
    std::vector<std::uint8_t> file(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    if (file.size() < KRO_HEADER_SIZE)
        return false;

    const std::uint8_t* p = file.data();

    // ---- Validate header ----
    if (std::memcmp(p, KRO_MAGIC, 4) != 0)
        return false;
    // Version at p[4-5], kind at p[6-7]
    std::uint16_t kind = read_le16(p + 6);
    std::uint16_t hsz = read_le16(p + 8);
    if (hsz < KRO_HEADER_SIZE || file.size() < hsz)
        return false;
    obj.kind = static_cast<ObjectKind>(kind);
    obj.flags = read_le16(p + 10);
    obj.target_numeric_id = read_le32(p + 12);
    obj.abi_version = read_le32(p + 16);
    obj.pointer_width = p[20];
    obj.endianness = p[21];

    std::uint32_t section_count = read_le32(p + 24);
    std::uint32_t symbol_count = read_le32(p + 28);
    std::uint32_t relocation_count = read_le32(p + 32);

    std::uint64_t string_table_off = read_le64(p + 36);
    std::uint64_t string_table_size = read_le64(p + 44);
    std::uint64_t debug_table_off = read_le64(p + 52);
    std::uint64_t debug_table_size = read_le64(p + 60);
    obj.entry_address = read_le64(p + 68);
    obj.image_base = read_le64(p + 76);
    std::uint64_t file_size = read_le64(p + 84);
    // Build ID at p[92-107] — not used by reader.
    // CRC32 at p[108-111] — could validate here

    // Derive target_id string from target_numeric_id.
    switch (obj.target_numeric_id) {
        case 1: obj.target_id = "kr32"; break;
        case 2: obj.target_id = "kr16"; break;
        case 3: obj.target_id = "kr64"; break;
        default: obj.target_id = "unknown"; break;
    }

    if (file_size > file.size())
        return false;

    // ---- Read string table ----
    if (string_table_off + string_table_size > file.size())
        return false;
    obj.string_table.assign(p + string_table_off,
                            p + string_table_off + string_table_size);
    // Build string offset map.
    std::uint64_t str_cursor = 0;
    while (str_cursor < string_table_size) {
        const char* s = reinterpret_cast<const char*>(p + string_table_off + str_cursor);
        std::uint32_t off = static_cast<std::uint32_t>(str_cursor);
        size_t len = std::strlen(s);
        obj.string_offsets[std::string(s)] = off;
        str_cursor += len + 1;
    }

    auto read_string = [&](std::uint32_t off) -> std::string {
        if (off >= string_table_size) return "";
        const char* s = reinterpret_cast<const char*>(p + string_table_off + off);
        return std::string(s);
    };

    // ---- Read section table ----
    std::uint64_t section_table_off = KRO_HEADER_SIZE;
    for (std::uint32_t i = 0; i < section_count; ++i) {
        std::size_t entry_off = section_table_off + i * KRO_SECTION_ENTRY_SIZE;
        if (entry_off + KRO_SECTION_ENTRY_SIZE > file.size())
            return false;
        const std::uint8_t* e = p + entry_off;

        ObjectSection sec;
        sec.name = read_string(read_le32(e + 0));
        sec.type = static_cast<SectionType>(read_le16(e + 4));
        sec.flags = read_le16(e + 6);
        sec.address = read_le64(e + 8);
        std::uint64_t payload_off = read_le64(e + 16);
        std::uint64_t payload_size = read_le64(e + 24);
        sec.alignment = static_cast<std::uint32_t>(read_le32(e + 32));
        sec.link_info = read_le32(e + 36);
        sec.entry_size = read_le32(e + 40);

        if (payload_off + payload_size > file_size)
            return false;
        sec.payload.assign(p + payload_off, p + payload_off + payload_size);
        obj.sections.push_back(std::move(sec));
    }

    // ---- Read symbol table ----
    std::uint64_t symbol_table_off =
        section_table_off + section_count * KRO_SECTION_ENTRY_SIZE;
    for (std::uint32_t i = 0; i < symbol_count; ++i) {
        std::size_t entry_off = symbol_table_off + i * KRO_SYMBOL_ENTRY_SIZE;
        if (entry_off + KRO_SYMBOL_ENTRY_SIZE > file.size())
            return false;
        const std::uint8_t* e = p + entry_off;

        ObjectSymbol sym;
        sym.name = read_string(read_le32(e + 0));
        sym.value = read_le64(e + 4);
        sym.size = read_le64(e + 12);
        sym.section_index = read_le32(e + 20);
        sym.binding = static_cast<SymbolBinding>(e[24]);
        sym.type = static_cast<SymbolType>(e[25]);
        sym.visibility = static_cast<SymbolVisibility>(e[26]);
        sym.flags = e[27];
        sym.target_specific = read_le32(e + 28);
        obj.symbols.push_back(std::move(sym));
    }

    // ---- Read relocation table ----
    std::uint64_t relocation_table_off =
        symbol_table_off + symbol_count * KRO_SYMBOL_ENTRY_SIZE;
    for (std::uint32_t i = 0; i < relocation_count; ++i) {
        std::size_t entry_off = relocation_table_off + i * KRO_RELOCATION_ENTRY_SIZE;
        if (entry_off + KRO_RELOCATION_ENTRY_SIZE > file.size())
            return false;
        const std::uint8_t* e = p + entry_off;

        ObjectRelocation rel;
        rel.section_index = read_le32(e + 0);
        rel.offset = read_le64(e + 4);
        rel.type = static_cast<RelocType>(read_le32(e + 12));
        rel.symbol_index = read_le32(e + 16);
        rel.addend = static_cast<std::int64_t>(read_le64(e + 20));
        rel.width = e[28];
        rel.flags = e[29];
        obj.relocations.push_back(std::move(rel));
    }

    // ---- Read debug table ----
    if (debug_table_size > 0) {
        if (debug_table_off + debug_table_size > file.size())
            return false;
        obj.debug_table.assign(p + debug_table_off,
                               p + debug_table_off + debug_table_size);
    }

    // ---- Read section payloads (already read during section table parsing) ----
    // Payloads were read inline above.

    return true;
}

} // namespace krc
