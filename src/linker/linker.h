#ifndef KRC_LINKER_H
#define KRC_LINKER_H

#include "object.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace krc {

// Merged section: combines payloads from multiple object files with the same
// section name.
struct MergedSection {
    std::string name;
    SectionType type = SectionType::Other;
    std::uint16_t flags = 0;
    std::uint64_t address = 0;
    std::vector<std::uint8_t> payload;
    std::uint64_t alignment = 1;
    std::uint32_t entry_size = 0;

    // Offset within this merged section where each input section's payload
    // was placed, keyed by input object index.
    std::unordered_map<std::uint32_t, std::uint64_t> input_offsets;
};

// Resolved symbol: the final symbol after linking.
struct ResolvedSymbol {
    std::string name;
    std::uint64_t value = 0;
    std::uint64_t size = 0;
    std::uint32_t section_index = 0; // index into merged sections
    SymbolBinding binding = SymbolBinding::Local;
    SymbolType type = SymbolType::Notype;
    SymbolVisibility visibility = SymbolVisibility::Default;
    bool defined = false;
};

// Linker configuration.
struct LinkerOptions {
    std::string target_id;          // required target ID for validation
    std::uint32_t abi_version = 1;  // required ABI version
    std::uint8_t pointer_width = 32;
    std::uint8_t endianness = 0;    // 0 little, 1 big
    std::string entry_symbol;       // entry point symbol name
    bool gc_sections = false;       // garbage-collect unused sections
    bool strip = false;             // strip symbols/debug
    bool verbose = false;
    std::string debug_file;         // write sidecar KDBG debug file
};

// The linker: reads object files, resolves symbols, merges sections,
// applies relocations, and produces a linked object/image.
class Linker {
public:
    explicit Linker(const LinkerOptions& opts);

    // Add an object file to link.
    bool add_object(const ObjectFile& obj);

    // Run the link. Returns true on success.
    bool link();

    // Get the merged output object (kind = Executable).
    const ObjectFile& output() const { return output_; }

    // Get resolved symbols.
    const std::vector<ResolvedSymbol>& symbols() const { return symbols_; }

    // Get error messages.
    const std::vector<std::string>& errors() const { return errors_; }

    // Write the output object to a file as KRO.
    bool write_kro(const std::string& path);

private:
    LinkerOptions opts_;
    ObjectFile output_;
    std::vector<ResolvedSymbol> symbols_;
    std::vector<std::string> errors_;

    // Input objects (kept for relocation application).
    std::vector<ObjectFile> input_objects_;

    // Symbol name -> index into symbols_ (for defined symbols).
    std::unordered_map<std::string, std::uint32_t> defined_symbols_;
    // Symbol name -> list of (object_index, symbol_index) for definitions.
    std::unordered_map<std::string, std::vector<std::pair<std::uint32_t, std::uint32_t>>>
        symbol_definitions_;

    // Merged sections by name.
    std::unordered_map<std::string, std::uint32_t> section_name_to_index_;
    std::vector<MergedSection> merged_sections_;

    // Symbol name -> merged section index and offset (for defined symbols).
    std::unordered_map<std::string, std::pair<std::uint32_t, std::uint64_t>>
        symbol_locations_;

    // Validate that all input objects are compatible.
    bool validate_objects();

    // Build the global symbol table.
    bool build_symbol_table();

    // Merge sections from all input objects.
    bool merge_sections();

    // Assign addresses to merged sections.
    bool assign_addresses();

    // Garbage-collect unused sections (--gc-sections).
    bool gc_sections();

    // Apply relocations from all input objects.
    bool apply_relocations();

    // Strip symbols and debug info from the output (--strip).
    bool strip_symbols();

    // Write the merged KDBG debug sidecar file (--debug-file).
    bool write_debug_file(const std::string& path);

    void error(std::string msg);
};

} // namespace krc

#endif // KRC_LINKER_H
