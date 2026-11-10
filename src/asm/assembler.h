#ifndef KRC_ASSEMBLER_H
#define KRC_ASSEMBLER_H

#include "machine.h"
#include "object.h"
#include "targets/kr32/kr32.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace krc {

// Assembles a MachineModule into a KRO ObjectFile.
// Handles instruction encoding (including pseudo-instruction expansion),
// section layout, symbol emission, and relocation generation for the KR32
// target.
class Kr32Assembler {
public:
    explicit Kr32Assembler(const TargetDescriptor& target);

    // Assemble a machine module into an object file.
    bool assemble(const MachineModule& mm, ObjectFile& obj);

private:
    const TargetDescriptor& target_;

    // Encode a single machine instruction (possibly a pseudo) into bytes.
    // Returns the encoded bytes (4 or 8 bytes per KR32 instruction).
    // Pseudo instructions may expand to multiple instructions; relocations
    // are recorded via add_relocation() with the current section offset.
    std::vector<std::uint8_t> encode_instruction(const MachineInstr& inst,
                                                 std::uint64_t current_offset);

    // Encode a KR32 4-byte instruction.
    std::vector<std::uint8_t> encode_kr32_4byte(Kr32Opcode op,
                                                std::uint32_t rd,
                                                std::uint32_t rs1,
                                                std::uint32_t rs2,
                                                std::int32_t imm11);

    // Encode a KR32 8-byte instruction.
    std::vector<std::uint8_t> encode_kr32_8byte(Kr32Opcode op,
                                                std::uint32_t rd,
                                                std::uint32_t rs1,
                                                std::int32_t imm11);

    // Map virtual register to physical register (or stack slot).
    std::uint32_t map_vreg(std::uint32_t vreg);

    // Look up (or create) a symbol index for a global name.
    std::uint32_t symbol_index_for(std::string_view name,
                                   SymbolBinding binding,
                                   SymbolType type);

    // Add a relocation entry.
    void add_relocation(std::uint32_t section_index, std::uint64_t offset,
                        RelocType type, std::uint32_t symbol_index,
                        std::int64_t addend, std::uint8_t width);

    // Get the .text section index.
    std::uint32_t text_section_index() const { return 1; }

    // Get the .rodata section index (created on demand).
    std::uint32_t rodata_section_index() const { return 2; }

    // Get the .data section index (created on demand).
    std::uint32_t data_section_index() const { return 3; }

    std::unordered_map<std::uint32_t, std::uint32_t> vreg_map_;
    std::unordered_map<std::string, std::uint32_t> symbol_index_map_;
    std::vector<ObjectRelocation> pending_relocations_;
    std::uint32_t next_phys_reg_ = 0;
};

} // namespace krc

#endif // KRC_ASSEMBLER_H
