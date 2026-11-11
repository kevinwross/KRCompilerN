#include "kr16.h"

#include <algorithm>

namespace krc {

//------------------------------------------------------------------------------
// Register helpers
//------------------------------------------------------------------------------

std::string kr16_reg_name(std::uint32_t reg) {
    static const char* names[5] = {
        "r0", "r1", "r2", "r3", "r4",
    };
    if (reg < 5) return names[reg];
    return "r" + std::to_string(reg);
}

bool kr16_is_caller_saved(std::uint32_t reg) {
    // R0-R3 are caller-saved.
    return reg <= KR16_R3;
}

bool kr16_is_callee_saved(std::uint32_t reg) {
    // R4 is callee-saved.
    return reg == KR16_FP_REG;
}

bool kr16_is_reserved(std::uint32_t /*reg*/) {
    // No reserved registers on KR16.
    return false;
}

std::uint32_t kr16_num_allocatable_gprs() {
    // All 5 registers are allocatable (R4 is callee-saved but allocatable).
    return 5;
}

std::vector<std::uint32_t> kr16_allocatable_gprs() {
    std::vector<std::uint32_t> regs;
    for (std::uint32_t r = 0; r < 5; ++r) {
        regs.push_back(r);
    }
    return regs;
}

//------------------------------------------------------------------------------
// Immediate helpers
//------------------------------------------------------------------------------

bool imm8_fits(std::int32_t imm) {
    return imm >= -128 && imm <= 127;
}

//------------------------------------------------------------------------------
// Instruction encoding
//------------------------------------------------------------------------------

// KR16 2-byte encoding (all formats):
//
// R-type: [15:12] opcode (4) [11:10] rd (2) [9:8] rs1 (2) [7:6] rs2 (2) [5:0] funct (6)
// I-type: [15:12] opcode (4) [11:10] rd (2) [9:8] rs1 (2) [7:0]  imm8 (8)
// L-type: [15:12] opcode (4) [11:10] rd (2) [9:8] base (2) [7:0]  imm8 (8)
// S-type: [15:12] opcode (4) [11:10] rs2 (2) [9:8] base (2) [7:0]  imm8 (8)
// B-type: [15:12] opcode (4) [11:10] cond (2) [9:8] rs1 (2)  [7:0]  imm8 (8)
// J-type: [15:12] opcode (4) [11:10] rd (2) [9:0]  imm10 (10)

std::uint16_t encode_kr16_2byte(Kr16Opcode op,
                                std::uint32_t rd,
                                std::uint32_t rs1,
                                std::uint32_t rs2,
                                std::int32_t imm8) {
    std::uint16_t inst = 0;
    inst |= (static_cast<std::uint16_t>(op) & 0xF) << 12;
    inst |= (static_cast<std::uint16_t>(rd) & 0x3) << 10;
    inst |= (static_cast<std::uint16_t>(rs1) & 0x3) << 8;
    inst |= (static_cast<std::uint16_t>(rs2) & 0x3) << 6;
    inst |= (static_cast<std::uint16_t>(imm8) & 0xFF);
    return inst;
}

//------------------------------------------------------------------------------
// Target descriptor
//------------------------------------------------------------------------------

const TargetDescriptor& kr16_target_descriptor() {
    return kr16_descriptor();
}

} // namespace krc
