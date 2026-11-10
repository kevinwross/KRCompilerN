#ifndef KRC_KDBG_H
#define KRC_KDBG_H

#include "object.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace krc {

//------------------------------------------------------------------------------
// KDBG debug format (plan.md §13)
//------------------------------------------------------------------------------

// Magic bytes for KDBG containers.
inline constexpr char KDBG_MAGIC[4] = {'K', 'D', 'B', 'G'};

// KDBG container version.
inline constexpr std::uint16_t KDBG_VERSION_MAJOR = 1;
inline constexpr std::uint16_t KDBG_VERSION_MINOR = 0;

// Section tags within a KDBG container.
enum class KdbgSectionTag : std::uint32_t {
    Header = 0,   // .kdbg_header
    Src = 1,      // .kdbg_src
    Line = 2,     // .kdbg_line
    Func = 3,     // .kdbg_func
    Frame = 4,    // .kdbg_frame
    Sym = 5,      // .kdbg_sym
    Type = 6,     // .kdbg_type
    Var = 7,      // .kdbg_var
    Scope = 8,    // .kdbg_scope
    Str = 9,      // .kdbg_str
    Build = 10,   // .kdbg_build
};

// Record tag + length encoding used inside each section.
// Records use a tag + length + payload encoding; unknown tags are skipped.
struct KdbgRecordHeader {
    std::uint32_t tag;    // record type tag
    std::uint32_t length; // payload length in bytes (not including this header)
};

//------------------------------------------------------------------------------
// KDBG record types (tags used within sections)
//------------------------------------------------------------------------------

// Line table record: address range -> source location.
struct KdbgLineRecord {
    std::uint64_t address;   // start address
    std::uint64_t size;      // byte length of the range
    std::uint32_t file_index; // index into source file table
    std::uint32_t line;      // source line (1-based)
    std::uint32_t column;    // source column (1-based)
};

// Function record.
struct KdbgFuncRecord {
    std::uint64_t address;     // start address
    std::uint64_t size;        // byte length
    std::uint32_t name_offset; // offset in string table
    std::uint32_t mangled_offset; // offset in string table (0 if not mangled)
    std::uint32_t return_type_id; // type ID (0 if unknown)
    std::uint32_t frame_size; // bytes
    std::uint32_t param_count;
    std::uint32_t flags;      // e.g. is_external, is_static, is_inline
};

// Frame/unwind record.
struct KdbgFrameRecord {
    std::uint64_t address;   // function start address
    std::uint32_t saved_fp_offset; // frame offset of saved FP (0xFFFFFFFF = none)
    std::uint32_t saved_ra_offset; // frame offset of saved RA (0xFFFFFFFF = none)
    std::uint32_t cfa_rule;  // CFA rule encoding
    std::uint32_t flags;
};

// Symbol record.
struct KdbgSymRecord {
    std::uint64_t value;     // address or offset
    std::uint64_t size;      // size in bytes
    std::uint32_t name_offset; // offset in string table
    std::uint32_t section_index; // section index or special value
    std::uint8_t binding;    // SymbolBinding
    std::uint8_t type;       // SymbolType
    std::uint8_t visibility; // SymbolVisibility
    std::uint8_t flags;
};

// Type record.
struct KdbgTypeRecord {
    std::uint32_t type_id;   // stable type ID
    std::uint8_t kind;       // builtin/pointer/array/struct/union/enum/typedef/function/reference/qualifier
    std::uint8_t qualifier;  // const/volatile/restrict/atomic
    std::uint32_t name_offset; // offset in string table
    std::uint32_t size;      // size in bytes
    std::uint32_t align;     // alignment in bytes
    std::uint32_t elem_type_id; // element type for pointer/array (0 if none)
    std::uint32_t elem_count;   // element count for array (0 if none)
};

// Variable record.
struct KdbgVarRecord {
    std::uint64_t address;   // address (for globals) or frame offset (for locals)
    std::uint32_t name_offset; // offset in string table
    std::uint32_t type_id;   // type ID
    std::uint32_t scope_id;  // enclosing scope ID
    std::uint32_t decl_line; // declaration line
    std::uint8_t location_kind; // frame_offset/register/addr/tls_offset
    std::uint8_t location_reg; // register number if location_kind == register
    std::uint8_t flags;      // param/local/static/global/TLS
    std::uint8_t reserved;
};

// Scope record.
struct KdbgScopeRecord {
    std::uint32_t scope_id;     // stable scope ID
    std::uint32_t parent_scope_id; // parent scope (0 = none)
    std::uint64_t address;     // start address (0 if not address-associated)
    std::uint64_t size;        // byte length
    std::uint32_t name_offset; // offset in string table
    std::uint8_t kind;       // namespace/class/function/block
    std::uint8_t flags;
};

// Source file record.
struct KdbgSrcRecord {
    std::uint32_t file_index;   // stable file index
    std::uint32_t path_offset;  // offset in string table
    std::uint32_t build_dir_offset; // offset in string table
    std::uint8_t language;      // 0=C, 1=C++, 2=other
    std::uint8_t reserved[3];
    std::uint64_t checksum;     // source content checksum (FNV-1a 64-bit)
};

// Build info record.
struct KdbgBuildRecord {
    std::uint32_t compiler_major;
    std::uint32_t compiler_minor;
    std::uint32_t target_features; // bitmask of target features
    std::uint32_t abi_version;
    std::uint32_t build_id_offset; // offset in string table
    std::uint64_t source_hash;     // hash of all source files
};

//------------------------------------------------------------------------------
// KDBG container (in-memory representation)
//------------------------------------------------------------------------------

struct KdbgContainer {
    std::string target_id;
    std::uint32_t target_numeric_id = 1;
    std::uint8_t pointer_width = 32;
    std::uint8_t endianness = 0; // 0 little, 1 big
    std::vector<std::uint8_t> build_id; // 16 bytes

    // String table (deduplicated).
    std::vector<std::uint8_t> string_table;
    std::unordered_map<std::string, std::uint32_t> string_offsets;

    // Source files.
    std::vector<KdbgSrcRecord> sources;

    // Line table.
    std::vector<KdbgLineRecord> lines;

    // Functions.
    std::vector<KdbgFuncRecord> functions;

    // Frame records.
    std::vector<KdbgFrameRecord> frames;

    // Symbols.
    std::vector<KdbgSymRecord> symbols;

    // Types.
    std::vector<KdbgTypeRecord> types;

    // Variables.
    std::vector<KdbgVarRecord> variables;

    // Scopes.
    std::vector<KdbgScopeRecord> scopes;

    // Build info.
    KdbgBuildRecord build_info{};

    // Raw payloads for sections that are written as-is (extensibility).
    std::vector<std::uint8_t> raw_header;
    std::vector<std::uint8_t> raw_build;
};

// Add a string to the string table, return its offset.
std::uint32_t kdbg_add_string(KdbgContainer& c, std::string_view s);

// Write a KDBG container to an output stream.
bool write_kdbg(std::ostream& out, const KdbgContainer& c);

// Read a KDBG container from an input stream.
bool read_kdbg(std::istream& in, KdbgContainer& c);

// Dump a KDBG container as text (human-readable) to an output stream.
bool dump_kdbg(std::ostream& out, const KdbgContainer& c);

} // namespace krc

#endif // KRC_KDBG_H
