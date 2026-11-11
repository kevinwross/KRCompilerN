#include "linker.h"

#include "kdbg.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

namespace krc {

Linker::Linker(const LinkerOptions& opts) : opts_(opts) {
    output_.target_id = opts.target_id;
    output_.kind = ObjectKind::Executable;
    output_.flags = static_cast<std::uint16_t>(ObjectFlags::HasSymbols);
    output_.pointer_width = opts.pointer_width;
    output_.endianness = opts.endianness;
    if (opts.target_id == "kr16") {
        output_.target_numeric_id = 2; // KR16
    } else if (opts.target_id == "kr64") {
        output_.target_numeric_id = 3; // KR64
    } else {
        output_.target_numeric_id = 1; // KR32
    }
    output_.abi_version = opts.abi_version;
    output_.entry_address = 0;
}

bool Linker::add_object(const ObjectFile& obj) {
    input_objects_.push_back(obj);
    return true;
}

bool Linker::link() {
    if (!validate_objects())
        return false;
    if (!build_symbol_table())
        return false;
    if (!merge_sections())
        return false;
    if (!gc_sections())
        return false;
    if (!assign_addresses())
        return false;
    if (!apply_relocations())
        return false;
    // Write the debug sidecar file (--debug-file) before stripping, so the
    // debug info is preserved in the sidecar even if stripped from the main
    // object.
    if (!opts_.debug_file.empty()) {
        if (!write_debug_file(opts_.debug_file))
            return false;
    }
    if (!strip_symbols())
        return false;
    // Finalize output entry address.
    if (!opts_.entry_symbol.empty()) {
        auto it = defined_symbols_.find(opts_.entry_symbol);
        if (it != defined_symbols_.end()) {
            const auto& sym = symbols_[it->second];
            output_.entry_address = sym.value;
        }
    }
    return true;
}

void Linker::error(std::string msg) {
    errors_.push_back(std::move(msg));
}

bool Linker::validate_objects() {
    for (std::size_t i = 0; i < input_objects_.size(); ++i) {
        const auto& obj = input_objects_[i];
        if (obj.target_id != opts_.target_id) {
            error("object " + std::to_string(i) +
                  ": target ID mismatch (expected " + opts_.target_id +
                  ", got " + obj.target_id + ")");
            return false;
        }
        if (obj.pointer_width != opts_.pointer_width) {
            error("object " + std::to_string(i) +
                  ": pointer width mismatch");
            return false;
        }
        if (obj.endianness != opts_.endianness) {
            error("object " + std::to_string(i) +
                  ": endianness mismatch");
            return false;
        }
    }
    return true;
}

bool Linker::build_symbol_table() {
    // First pass: collect all defined symbols and detect duplicates.
    for (std::size_t oi = 0; oi < input_objects_.size(); ++oi) {
        const auto& obj = input_objects_[oi];
        for (std::uint32_t si = 0; si < obj.symbols.size(); ++si) {
            const auto& sym = obj.symbols[si];
            if (sym.binding == SymbolBinding::Local && !sym.name.empty()) {
                // Local symbols are not visible across objects.
                continue;
            }
            if (sym.name.empty())
                continue;

            auto it = defined_symbols_.find(sym.name);
            if (it != defined_symbols_.end()) {
                // Duplicate definition.
                if (sym.binding == SymbolBinding::Weak) {
                    // Weak definition, keep the strong one.
                    continue;
                }
                // Check if the existing one is weak — if so, override.
                auto& existing = symbols_[it->second];
                if (existing.binding == SymbolBinding::Weak &&
                    sym.binding == SymbolBinding::Global) {
                    // Override weak with strong.
                    existing.value = sym.value;
                    existing.size = sym.size;
                    existing.section_index = sym.section_index;
                    existing.binding = sym.binding;
                    existing.defined = true;
                    symbol_definitions_[sym.name].push_back(
                        {static_cast<std::uint32_t>(oi), si});
                    continue;
                }
                error("duplicate symbol definition: " + sym.name);
                return false;
            }

            ResolvedSymbol rsym;
            rsym.name = sym.name;
            rsym.value = sym.value;
            rsym.size = sym.size;
            rsym.section_index = sym.section_index;
            rsym.binding = sym.binding;
            rsym.type = sym.type;
            rsym.visibility = sym.visibility;
            rsym.defined = (sym.section_index != 0);

            std::uint32_t idx = static_cast<std::uint32_t>(symbols_.size());
            symbols_.push_back(std::move(rsym));
            defined_symbols_[sym.name] = idx;
            symbol_definitions_[sym.name].push_back(
                {static_cast<std::uint32_t>(oi), si});
        }
    }

    // Second pass: add undefined symbols (referenced but not defined).
    for (std::size_t oi = 0; oi < input_objects_.size(); ++oi) {
        const auto& obj = input_objects_[oi];
        for (const auto& rel : obj.relocations) {
            if (rel.symbol_index >= obj.symbols.size())
                continue;
            const auto& sym = obj.symbols[rel.symbol_index];
            if (sym.name.empty())
                continue;
            if (defined_symbols_.count(sym.name))
                continue; // already defined

            ResolvedSymbol rsym;
            rsym.name = sym.name;
            rsym.value = 0;
            rsym.size = 0;
            rsym.section_index = 0; // undefined
            rsym.binding = SymbolBinding::Global;
            rsym.type = SymbolType::Notype;
            rsym.visibility = SymbolVisibility::Default;
            rsym.defined = false;

            std::uint32_t idx = static_cast<std::uint32_t>(symbols_.size());
            symbols_.push_back(std::move(rsym));
            defined_symbols_[sym.name] = idx;
        }
    }

    // Report undefined symbols.
    for (const auto& sym : symbols_) {
        if (!sym.defined && !sym.name.empty()) {
            error("undefined symbol: " + sym.name);
        }
    }

    return true;
}

bool Linker::merge_sections() {
    // Default section order: .text, .rodata, .data, .bss, others.
    static const std::vector<std::string> default_order = {
        ".text", ".rodata", ".data", ".bss"};

    // Collect all unique section names from all objects.
    std::unordered_set<std::string> seen_names;
    std::vector<std::string> all_names;

    for (const auto& obj : input_objects_) {
        for (const auto& sec : obj.sections) {
            if (seen_names.insert(sec.name).second)
                all_names.push_back(sec.name);
        }
    }

    // Sort: default order first, then alphabetical for the rest.
    std::sort(all_names.begin(), all_names.end(),
              [&](const std::string& a, const std::string& b) {
                  auto a_pos = std::find(default_order.begin(),
                                         default_order.end(), a);
                  auto b_pos = std::find(default_order.begin(),
                                         default_order.end(), b);
                  bool a_in_default = (a_pos != default_order.end());
                  bool b_in_default = (b_pos != default_order.end());
                  if (a_in_default && b_in_default)
                      return (a_pos - default_order.begin()) <
                             (b_pos - default_order.begin());
                  if (a_in_default) return true;
                  if (b_in_default) return false;
                  return a < b;
              });

    // Create merged sections.
    std::uint32_t sec_idx = 1; // section 0 is undefined
    for (const auto& name : all_names) {
        MergedSection ms;
        ms.name = name;
        ms.alignment = 1;

        // Find the most restrictive flags and type among all input sections
        // with this name.
        for (const auto& obj : input_objects_) {
            for (const auto& sec : obj.sections) {
                if (sec.name == name) {
                    ms.type = sec.type;
                    ms.flags = static_cast<std::uint16_t>(
                        ms.flags | sec.flags);
                    if (sec.alignment > ms.alignment)
                        ms.alignment = sec.alignment;
                }
            }
        }

        section_name_to_index_[name] = sec_idx;
        merged_sections_.push_back(std::move(ms));
        ++sec_idx;
    }

    return true;
}

bool Linker::assign_addresses() {
    std::uint64_t addr = 0;

    for (auto& ms : merged_sections_) {
        // Align to section alignment.
        if (ms.alignment > 1)
            addr = (addr + ms.alignment - 1) / ms.alignment * ms.alignment;

        ms.address = addr;

        // Total payload size: sum of all input section payloads for this name.
        std::uint64_t total_size = 0;
        std::uint64_t offset = 0;

        for (std::size_t oi = 0; oi < input_objects_.size(); ++oi) {
            const auto& obj = input_objects_[oi];
            for (const auto& sec : obj.sections) {
                if (sec.name == ms.name) {
                    ms.input_offsets[static_cast<std::uint32_t>(oi)] = offset;
                    total_size += sec.payload.size();
                    offset += sec.payload.size();
                }
            }
        }

        ms.payload.resize(total_size);
        addr += total_size;
    }

    // Set entry address if entry symbol is specified.
    if (!opts_.entry_symbol.empty()) {
        auto it = defined_symbols_.find(opts_.entry_symbol);
        if (it != defined_symbols_.end()) {
            const auto& sym = symbols_[it->second];
            if (sym.defined) {
                output_.entry_address = sym.value;
            } else {
                error("entry symbol not defined: " + opts_.entry_symbol);
                return false;
            }
        } else {
            error("entry symbol not found: " + opts_.entry_symbol);
            return false;
        }
    }

    return true;
}

bool Linker::gc_sections() {
    if (!opts_.gc_sections)
        return true;

    if (opts_.verbose)
        fprintf(stderr, "krld: garbage-collecting unused sections\n");

    // Determine live merged sections by name. A section is live if:
    //   - it contains the entry symbol (if --entry is specified),
    //   - it contains a global (exported) symbol,
    //   - it is referenced by a relocation from a live section.
    std::unordered_set<std::string> live_sections;

    // Seed: entry symbol's section.
    if (!opts_.entry_symbol.empty()) {
        auto dit = defined_symbols_.find(opts_.entry_symbol);
        if (dit != defined_symbols_.end()) {
            const auto& sym = symbols_[dit->second];
            if (sym.defined && sym.section_index != 0) {
                // section_index is 1-based (like ELF st_shndx); convert to
                // 0-based vector index.
                std::uint32_t vec_idx = sym.section_index - 1;
                for (const auto& obj : input_objects_) {
                    if (vec_idx < obj.sections.size()) {
                        live_sections.insert(obj.sections[vec_idx].name);
                        break;
                    }
                }
            }
        }
    }

    // Seed: sections containing global (exported) symbols.
    for (const auto& obj : input_objects_) {
        for (const auto& sym : obj.symbols) {
            if (sym.binding == SymbolBinding::Global &&
                sym.section_index != 0 && !sym.name.empty()) {
                std::uint32_t vec_idx = sym.section_index - 1;
                if (vec_idx < obj.sections.size()) {
                    live_sections.insert(obj.sections[vec_idx].name);
                }
            }
        }
    }

    // Iterate: follow relocations from live sections to find more live sections.
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& obj : input_objects_) {
            for (std::uint32_t si = 0; si < obj.sections.size(); ++si) {
                const auto& sec = obj.sections[si];
                if (!live_sections.count(sec.name))
                    continue;
                // This section is live; follow its relocations.
                for (const auto& rel : obj.relocations) {
                    // rel.section_index is 1-based (section table index),
                    // si is 0-based vector index.
                    if (rel.section_index != si + 1)
                        continue;
                    if (rel.symbol_index >= obj.symbols.size())
                        continue;
                    const auto& sym = obj.symbols[rel.symbol_index];
                    if (sym.name.empty() || sym.section_index == 0)
                        continue; // undefined or absolute symbol
                    // Find the section name that defines this symbol.
                    // section_index is 1-based; convert to 0-based vector index.
                    std::uint32_t vec_idx = sym.section_index - 1;
                    for (const auto& obj2 : input_objects_) {
                        if (vec_idx < obj2.sections.size()) {
                            const auto& target_sec = obj2.sections[vec_idx];
                            if (!target_sec.name.empty() &&
                                !live_sections.count(target_sec.name)) {
                                live_sections.insert(target_sec.name);
                                changed = true;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    // Remove merged sections that are not live.
    std::vector<MergedSection> live_merged;
    for (auto& ms : merged_sections_) {
        if (live_sections.count(ms.name)) {
            live_merged.push_back(std::move(ms));
        } else if (opts_.verbose) {
            fprintf(stderr, "krld: gc-sections: removing section '%s'\n",
                    ms.name.c_str());
        }
    }
    merged_sections_ = std::move(live_merged);

    // Rebuild section_name_to_index_ for remaining sections.
    section_name_to_index_.clear();
    std::uint32_t sec_idx = 1;
    for (const auto& ms : merged_sections_) {
        section_name_to_index_[ms.name] = sec_idx++;
    }

    return true;
}

bool Linker::strip_symbols() {
    if (!opts_.strip)
        return true;

    if (opts_.verbose)
        fprintf(stderr, "krld: stripping symbols and debug info\n");

    // Remove all symbols from the output.
    output_.symbols.clear();

    // Remove debug table.
    output_.debug_table.clear();

    // Set the Stripped flag and clear HasSymbols.
    output_.flags = static_cast<std::uint16_t>(
        (output_.flags & ~static_cast<std::uint16_t>(ObjectFlags::HasSymbols))
        | static_cast<std::uint16_t>(ObjectFlags::Stripped));

    return true;
}

bool Linker::apply_relocations() {
    // Build symbol location map: symbol name -> (merged section index, offset
    // within that section).
    // This requires knowing the final address of each symbol, which depends on
    // section addresses and input section offsets within merged sections.

    // First, compute symbol locations within merged sections.
    for (std::size_t oi = 0; oi < input_objects_.size(); ++oi) {
        const auto& obj = input_objects_[oi];
        for (std::uint32_t si = 0; si < obj.symbols.size(); ++si) {
            const auto& sym = obj.symbols[si];
            if (sym.name.empty() || sym.section_index == 0)
                continue; // undefined symbol, no location yet

            // Find the merged section for this symbol's input section.
            if (sym.section_index >= obj.sections.size())
                continue;
            const auto& input_sec = obj.sections[sym.section_index];
            auto mit = section_name_to_index_.find(input_sec.name);
            if (mit == section_name_to_index_.end())
                continue;

            std::uint32_t merged_sec_idx = mit->second;
            auto iit = merged_sections_[merged_sec_idx].input_offsets.find(
                static_cast<std::uint32_t>(oi));
            if (iit == merged_sections_[merged_sec_idx].input_offsets.end())
                continue;

            std::uint64_t input_offset = iit->second;
            // The symbol's value is relative to its section start.
            std::uint64_t offset_within_section = sym.value;
            std::uint64_t offset_within_merged =
                input_offset + offset_within_section;

            symbol_locations_[sym.name] = {merged_sec_idx, offset_within_merged};
        }
    }

    // Now apply relocations.
    for (std::size_t oi = 0; oi < input_objects_.size(); ++oi) {
        const auto& obj = input_objects_[oi];
        for (const auto& rel : obj.relocations) {
            if (rel.symbol_index >= obj.symbols.size()) {
                error("relocation references invalid symbol index");
                return false;
            }

            const auto& sym = obj.symbols[rel.symbol_index];
            if (sym.name.empty())
                continue;

            auto sit = symbol_locations_.find(sym.name);
            if (sit == symbol_locations_.end()) {
                error("relocation references undefined symbol: " + sym.name);
                return false;
            }

            std::uint32_t merged_sec_idx = sit->second.first;
            std::uint64_t sym_offset_in_merged = sit->second.second;
            std::uint64_t sym_addr =
                merged_sections_[merged_sec_idx].address + sym_offset_in_merged;

            // Find where in the merged section this relocation applies.
            auto iit = merged_sections_[merged_sec_idx].input_offsets.find(
                static_cast<std::uint32_t>(oi));
            if (iit == merged_sections_[merged_sec_idx].input_offsets.end())
                continue;

            std::uint64_t input_offset = iit->second;
            std::uint64_t reloc_offset_in_merged =
                input_offset + rel.offset;

            // Determine patch width from the relocation's width field.
            std::size_t patch_bytes = (rel.width + 7) / 8;
            if (patch_bytes < 1) patch_bytes = 1;

            if (reloc_offset_in_merged + patch_bytes >
                merged_sections_[merged_sec_idx].payload.size()) {
                error("relocation offset out of range");
                return false;
            }

            // Apply the relocation based on type.
            std::uint8_t* patch_loc =
                merged_sections_[merged_sec_idx].payload.data() +
                reloc_offset_in_merged;

            switch (rel.type) {
                case RelocType::R_KR32_Branch26:
                case RelocType::R_Abs32: {
                    if (patch_bytes < 4) {
                        error("relocation width too small for 32-bit patch");
                        return false;
                    }
                    std::int64_t target = sym_addr + rel.addend;
                    std::uint32_t inst = 0;
                    std::memcpy(&inst, patch_loc, 4);
                    inst = (inst & 0xFFFFFFF8u) |
                           (static_cast<std::uint32_t>(target & 0x7FFu) << 3);
                    std::memcpy(patch_loc, &inst, 4);
                    break;
                }
                case RelocType::R_KR16_Branch10: {
                    if (patch_bytes < 2) {
                        error("relocation width too small for 16-bit patch");
                        return false;
                    }
                    std::int64_t target = sym_addr + rel.addend;
                    // KR16 J/JAL: 2-byte instruction, 10-bit immediate at
                    // bits [9:0]. Patch the low 10 bits.
                    std::uint16_t inst = 0;
                    std::memcpy(&inst, patch_loc, 2);
                    inst = static_cast<std::uint16_t>(
                        (inst & 0xFC00u) |
                        (static_cast<std::uint32_t>(target & 0x3FFu)));
                    patch_loc[0] = static_cast<std::uint8_t>(inst & 0xFF);
                    patch_loc[1] = static_cast<std::uint8_t>((inst >> 8) & 0xFF);
                    break;
                }
                case RelocType::R_KR16_Branch8: {
                    if (patch_bytes < 2) {
                        error("relocation width too small for 16-bit patch");
                        return false;
                    }
                    std::int64_t target = sym_addr + rel.addend;
                    // KR16 BEQ/BNE etc.: 2-byte instruction, 8-bit immediate
                    // at bits [7:0].
                    std::uint16_t inst = 0;
                    std::memcpy(&inst, patch_loc, 2);
                    inst = static_cast<std::uint16_t>(
                        (inst & 0xFF00u) |
                        (static_cast<std::uint32_t>(target & 0xFFu)));
                    patch_loc[0] = static_cast<std::uint8_t>(inst & 0xFF);
                    patch_loc[1] = static_cast<std::uint8_t>((inst >> 8) & 0xFF);
                    break;
                }
                case RelocType::R_KR16_Abs16:
                case RelocType::R_Abs16: {
                    if (patch_bytes < 2) {
                        error("relocation width too small for 16-bit patch");
                        return false;
                    }
                    std::int64_t target = sym_addr + rel.addend;
                    std::uint16_t val = static_cast<std::uint16_t>(target & 0xFFFF);
                    patch_loc[0] = static_cast<std::uint8_t>(val & 0xFF);
                    patch_loc[1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
                    break;
                }
                case RelocType::R_Abs8: {
                    if (patch_bytes < 1) {
                        error("relocation width too small for 8-bit patch");
                        return false;
                    }
                    std::int64_t target = sym_addr + rel.addend;
                    *patch_loc = static_cast<std::uint8_t>(target & 0xFF);
                    break;
                }
                case RelocType::R_Abs64: {
                    if (patch_bytes < 8) {
                        error("relocation width too small for 64-bit patch");
                        return false;
                    }
                    std::int64_t target = sym_addr + rel.addend;
                    std::uint64_t val = static_cast<std::uint64_t>(target);
                    for (int i = 0; i < 8; ++i)
                        patch_loc[i] =
                            static_cast<std::uint8_t>((val >> (i * 8)) & 0xFF);
                    break;
                }
                default:
                    error("unsupported relocation type: " +
                          std::to_string(
                              static_cast<std::uint32_t>(rel.type)));
                    return false;
            }
        }
    }

    // Build the output sections from merged sections.
    for (const auto& ms : merged_sections_) {
        ObjectSection os;
        os.name = ms.name;
        os.type = ms.type;
        os.flags = ms.flags;
        os.address = ms.address;
        os.payload = ms.payload;
        os.alignment = ms.alignment;
        output_.sections.push_back(os);
    }

    // Copy resolved symbols to output, remapping section indices and
    // values to the merged section layout.
    for (const auto& sym : symbols_) {
        ObjectSymbol osym;
        osym.name = sym.name;
        osym.value = sym.value;
        osym.size = sym.size;
        if (sym.section_index != 0) {
            // Remap from input section index to merged section index.
            // We need to find which input object and section this symbol
            // came from. Use the symbol_locations_ map if available.
            auto lit = symbol_locations_.find(sym.name);
            if (lit != symbol_locations_.end()) {
                osym.section_index = lit->second.first;
                osym.value = merged_sections_[lit->second.first].address +
                             lit->second.second;
            } else {
                osym.section_index = sym.section_index;
            }
        } else {
            osym.section_index = 0; // undefined
        }
        osym.binding = sym.binding;
        osym.type = sym.type;
        osym.visibility = sym.visibility;
        output_.symbols.push_back(osym);
    }

    output_.relocations = {}; // linked image has no relocations

    return true;
}

bool Linker::write_kro(const std::string& path) {
    std::ofstream os(path, std::ios::binary);
    if (!os) {
        error("cannot open output file: " + path);
        return false;
    }
    if (!write_kro_object(os, output_)) {
        error("failed to write output file: " + path);
        return false;
    }
    return true;
}

bool Linker::write_debug_file(const std::string& path) {
    krc::KdbgContainer merged;
    merged.target_id = opts_.target_id;
    merged.target_numeric_id = 1; // KR32
    merged.pointer_width = opts_.pointer_width;
    merged.endianness = opts_.endianness;
    merged.build_info.compiler_major = 0;
    merged.build_info.compiler_minor = 1;

    // Merge debug tables from all input objects.
    for (const auto& obj : input_objects_) {
        if (obj.debug_table.empty())
            continue;

        std::istringstream iss(
            std::string(obj.debug_table.begin(), obj.debug_table.end()));
        krc::KdbgContainer src;
        if (!krc::read_kdbg(iss, src)) {
            if (opts_.verbose) {
                fprintf(stderr,
                        "krld: warning: failed to read debug table from "
                        "input object\n");
            }
            continue;
        }

        // Merge string table: add all strings from src to merged, recording
        // the offset mapping for remapping record offsets.
        std::unordered_map<std::uint32_t, std::uint32_t> str_offset_map;
        for (std::size_t i = 0; i < src.string_table.size(); ) {
            // Find the null-terminated string at position i.
            const char* s = reinterpret_cast<const char*>(
                src.string_table.data() + i);
            std::string_view sv(s);
            auto null = sv.find('\0');
            if (null == std::string_view::npos)
                break;
            sv = sv.substr(0, null);
            std::uint32_t new_off =
                krc::kdbg_add_string(merged, std::string(sv));
            str_offset_map[static_cast<std::uint32_t>(i)] = new_off;
            i += null + 1;
        }

        // Merge sources.
        for (const auto& src_rec : src.sources) {
            krc::KdbgSrcRecord rec{};
            rec.file_index = static_cast<std::uint32_t>(merged.sources.size());
            auto pit = str_offset_map.find(src_rec.path_offset);
            rec.path_offset =
                (pit != str_offset_map.end()) ? pit->second : src_rec.path_offset;
            auto bit = str_offset_map.find(src_rec.build_dir_offset);
            rec.build_dir_offset =
                (bit != str_offset_map.end()) ? bit->second : src_rec.build_dir_offset;
            rec.language = src_rec.language;
            rec.checksum = src_rec.checksum;
            merged.sources.push_back(rec);
        }

        // Merge lines.
        for (const auto& line : src.lines) {
            krc::KdbgLineRecord rec{};
            rec.address = line.address;
            rec.size = line.size;
            // file_index is an index into the source table, not a string
            // offset. Remap based on how many sources we already have.
            // (Simplification: file_index 0 maps to the first source from
            // this object, which becomes merged.sources.size() - count.)
            rec.file_index = line.file_index; // TODO: proper remapping
            rec.line = line.line;
            rec.column = line.column;
            merged.lines.push_back(rec);
        }

        // Merge functions.
        for (const auto& fn : src.functions) {
            krc::KdbgFuncRecord rec{};
            rec.address = fn.address;
            rec.size = fn.size;
            auto pit = str_offset_map.find(fn.name_offset);
            rec.name_offset =
                (pit != str_offset_map.end()) ? pit->second : fn.name_offset;
            auto pit2 = str_offset_map.find(fn.mangled_offset);
            rec.mangled_offset =
                (pit2 != str_offset_map.end()) ? pit2->second : fn.mangled_offset;
            rec.return_type_id = fn.return_type_id;
            rec.frame_size = fn.frame_size;
            rec.param_count = fn.param_count;
            rec.flags = fn.flags;
            merged.functions.push_back(rec);
        }

        // Merge frames.
        for (const auto& frame : src.frames) {
            krc::KdbgFrameRecord rec{};
            rec.address = frame.address;
            rec.saved_fp_offset = frame.saved_fp_offset;
            rec.saved_ra_offset = frame.saved_ra_offset;
            rec.cfa_rule = frame.cfa_rule;
            rec.flags = frame.flags;
            merged.frames.push_back(rec);
        }

        // Merge variables.
        for (const auto& var : src.variables) {
            krc::KdbgVarRecord rec{};
            rec.address = var.address;
            auto pit = str_offset_map.find(var.name_offset);
            rec.name_offset =
                (pit != str_offset_map.end()) ? pit->second : var.name_offset;
            rec.type_id = var.type_id;
            rec.scope_id = var.scope_id;
            rec.decl_line = var.decl_line;
            rec.location_kind = var.location_kind;
            rec.location_reg = var.location_reg;
            rec.flags = var.flags;
            rec.reserved = var.reserved;
            merged.variables.push_back(rec);
        }

        // Merge symbols.
        for (const auto& sym : src.symbols) {
            krc::KdbgSymRecord rec{};
            rec.value = sym.value;
            rec.size = sym.size;
            auto pit = str_offset_map.find(sym.name_offset);
            rec.name_offset =
                (pit != str_offset_map.end()) ? pit->second : sym.name_offset;
            rec.section_index = sym.section_index;
            rec.binding = sym.binding;
            rec.type = sym.type;
            rec.visibility = sym.visibility;
            rec.flags = sym.flags;
            merged.symbols.push_back(rec);
        }

        // Merge types.
        for (const auto& ty : src.types) {
            krc::KdbgTypeRecord rec{};
            rec.type_id = ty.type_id;
            rec.kind = ty.kind;
            rec.qualifier = ty.qualifier;
            auto pit = str_offset_map.find(ty.name_offset);
            rec.name_offset =
                (pit != str_offset_map.end()) ? pit->second : ty.name_offset;
            rec.size = ty.size;
            rec.align = ty.align;
            rec.elem_type_id = ty.elem_type_id;
            rec.elem_count = ty.elem_count;
            merged.types.push_back(rec);
        }

        // Merge scopes.
        for (const auto& sc : src.scopes) {
            krc::KdbgScopeRecord rec{};
            rec.scope_id = sc.scope_id;
            rec.parent_scope_id = sc.parent_scope_id;
            rec.address = sc.address;
            rec.size = sc.size;
            auto pit = str_offset_map.find(sc.name_offset);
            rec.name_offset =
                (pit != str_offset_map.end()) ? pit->second : sc.name_offset;
            rec.kind = sc.kind;
            rec.flags = sc.flags;
            merged.scopes.push_back(rec);
        }

        // Use build info from the first object that has it.
        if (merged.build_info.compiler_major == 0 &&
            merged.build_info.compiler_minor == 0) {
            merged.build_info = src.build_info;
        }
    }

    std::ofstream os(path, std::ios::binary);
    if (!os) {
        error("cannot open debug file: " + path);
        return false;
    }
    if (!krc::write_kdbg(os, merged)) {
        error("failed to write debug file: " + path);
        return false;
    }
    return true;
}

} // namespace krc
