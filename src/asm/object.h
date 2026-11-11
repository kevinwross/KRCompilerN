#ifndef KRC_OBJECT_H
#define KRC_OBJECT_H

#include "target_descriptor.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace krc {

//------------------------------------------------------------------------------
// KRO object format (plan.md §11)
//------------------------------------------------------------------------------

// Magic bytes for KRO objects.
inline constexpr char KRO_MAGIC[4] = {'K', 'R', 'O', '\0'};

// Object file kind.
enum class ObjectKind : std::uint16_t {
    Object = 0,      // relocatable object file
    Executable = 1,  // linked executable image
    ArchiveMember = 2, // member of an archive/library
};

// Object flags.
enum class ObjectFlags : std::uint16_t {
    None = 0,
    HasSymbols = 1 << 0,
    HasDebug = 1 << 1,
    Stripped = 1 << 2,
    Relocatable = 1 << 3,
};

// Section types.
enum class SectionType : std::uint16_t {
    Text = 0,
    Rodata = 1,
    Data = 2,
    Bss = 3,
    InitArray = 4,
    FiniArray = 5,
    Tls = 6,
    Debug = 7,
    Note = 8,
    Other = 9,
};

// Section flags.
enum class SectionFlags : std::uint16_t {
    None = 0,
    Alloc = 1 << 0,
    Write = 1 << 1,
    Execute = 1 << 2,
    Tls = 1 << 3,
    Merge = 1 << 4,
    Strings = 1 << 5,
    Discardable = 1 << 6,
};

// Symbol binding.
enum class SymbolBinding : std::uint8_t {
    Local = 0,
    Global = 1,
    Weak = 2,
};

// Symbol type.
enum class SymbolType : std::uint8_t {
    Notype = 0,
    Object = 1,
    Function = 2,
    Section = 3,
    File = 4,
    Tls = 5,
};

// Symbol visibility.
enum class SymbolVisibility : std::uint8_t {
    Default = 0,
    Internal = 1,
    Hidden = 2,
    Protected = 3,
};

// Relocation types.
enum class RelocType : std::uint32_t {
    // Absolute relocations.
    R_Abs8 = 0,
    R_Abs16 = 1,
    R_Abs32 = 2,
    R_Abs64 = 3,
    // PC-relative relocations.
    R_PCRel8 = 4,
    R_PCRel16 = 5,
    R_PCRel32 = 6,
    R_PCRel64 = 7,
    // Paired hi/lo (for large constants).
    R_Hi = 8,
    R_Lo = 9,
    // GOT/PLT reserved (unused in v1).
    R_GOT = 10,
    R_PLT = 11,
    // Target-specific: KR32 starts at 0x100.
    R_KR32_Branch26 = 0x100,
    R_KR32_Branch11 = 0x101,
    R_KR32_Abs32 = 0x102,
    R_KR32_PCRel32 = 0x103,
    // Target-specific: KR16 starts at 0x200.
    R_KR16_Branch10 = 0x200,
    R_KR16_Branch8 = 0x201,
    R_KR16_Abs16 = 0x202,
};

//------------------------------------------------------------------------------
// Object section
//------------------------------------------------------------------------------

struct ObjectSection {
    std::string name;
    SectionType type = SectionType::Other;
    std::uint16_t flags = static_cast<std::uint16_t>(SectionFlags::None);
    std::uint64_t address = 0;       // virtual address (0 for relocatable objects)
    std::vector<std::uint8_t> payload; // section contents
    std::uint64_t alignment = 1;
    std::uint32_t link_info = 0;     // section index or symbol index
    std::uint32_t entry_size = 0;    // for symbol tables etc.
};

//------------------------------------------------------------------------------
// Object symbol
//------------------------------------------------------------------------------

struct ObjectSymbol {
    std::string name;
    std::uint64_t value = 0;
    std::uint64_t size = 0;
    std::uint32_t section_index = 0; // section index, or special values
    SymbolBinding binding = SymbolBinding::Local;
    SymbolType type = SymbolType::Notype;
    SymbolVisibility visibility = SymbolVisibility::Default;
    std::uint8_t flags = 0;          // COMDAT, constructor, etc.
    std::uint32_t target_specific = 0;
};

//------------------------------------------------------------------------------
// Object relocation
//------------------------------------------------------------------------------

struct ObjectRelocation {
    std::uint32_t section_index = 0;
    std::uint64_t offset = 0;        // offset within the section
    RelocType type = RelocType::R_Abs32;
    std::uint32_t symbol_index = 0;  // index into symbol table
    std::int64_t addend = 0;
    std::uint8_t width = 32;         // bit width of the field
    std::uint8_t flags = 0;          // relaxation, sign-extend, etc.
};

//------------------------------------------------------------------------------
// Object file
//------------------------------------------------------------------------------

struct ObjectFile {
    std::string target_id;
    std::uint32_t target_numeric_id = 0; // numeric target ID
    std::uint32_t abi_version = 1;
    std::uint8_t pointer_width = 32;
    std::uint8_t endianness = 0; // 0 little, 1 big
    ObjectKind kind = ObjectKind::Object;
    std::uint16_t flags = static_cast<std::uint16_t>(ObjectFlags::None);
    std::uint64_t entry_address = 0;
    std::uint64_t image_base = 0;

    std::vector<ObjectSection> sections;
    std::vector<ObjectSymbol> symbols;
    std::vector<ObjectRelocation> relocations;

    // String table (built by the writer).
    std::vector<std::uint8_t> string_table;
    std::unordered_map<std::string, std::uint32_t> string_offsets;

    // Debug section (raw bytes, written as-is).
    std::vector<std::uint8_t> debug_table;

    ObjectFile() = default;
    explicit ObjectFile(std::string tid) : target_id(std::move(tid)) {}
};

// Add a string to the string table, return its offset.
std::uint32_t object_add_string(ObjectFile& obj, std::string_view s);

// Write a KRO object file to an output stream.
bool write_kro_object(std::ostream& out, const ObjectFile& obj);

// Read a KRO object file from an input stream.
bool read_kro_object(std::istream& in, ObjectFile& obj);

} // namespace krc

#endif // KRC_OBJECT_H
