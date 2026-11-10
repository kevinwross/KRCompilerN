#include "kr32.h"

#include <algorithm>

namespace krc {

//------------------------------------------------------------------------------
// Register helpers
//------------------------------------------------------------------------------

std::string kr32_reg_name(std::uint32_t reg) {
    static const char* names[16] = {
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    };
    if (reg < 16) return names[reg];
    return "r" + std::to_string(reg);
}

bool kr32_is_caller_saved(std::uint32_t reg) {
    // R0-R3 are caller-saved.
    return reg <= KR32_R3;
}

bool kr32_is_callee_saved(std::uint32_t reg) {
    // R7 is callee-saved.
    return reg == KR32_R7;
}

bool kr32_is_reserved(std::uint32_t reg) {
    // R11 is reserved (thread pointer).
    return reg == KR32_R11;
}

std::uint32_t kr32_num_allocatable_gprs() {
    // R0-R10, R12-R15 are allocatable (excluding R7 FP, R11 TP).
    return 13;
}

std::vector<std::uint32_t> kr32_allocatable_gprs() {
    std::vector<std::uint32_t> regs;
    for (std::uint32_t r = 0; r < 16; ++r) {
        if (r == KR32_FP_REG || r == KR32_TP_REG) continue;
        regs.push_back(r);
    }
    return regs;
}

//------------------------------------------------------------------------------
// Immediate helpers
//------------------------------------------------------------------------------

bool imm11_fits(std::int32_t imm) {
    return imm >= -1024 && imm <= 1023;
}

//------------------------------------------------------------------------------
// Instruction encoding
//------------------------------------------------------------------------------

// KR32 4-byte encoding:
//   [31:26] opcode (6 bits)
//   [25:22] rd (4 bits)
//   [21:18] rs1 (4 bits)
//   [17:14] rs2 (4 bits)
//   [13:3]  imm11 (11 bits, sign-extended)
//   [2:0]   pad (3 bits, zero)
std::uint32_t encode_kr32_4byte(Kr32Opcode op,
                                std::uint32_t rd,
                                std::uint32_t rs1,
                                std::uint32_t rs2,
                                std::int32_t imm11) {
    std::uint32_t inst = 0;
    inst |= (static_cast<std::uint32_t>(op) & 0x3F) << 26;
    inst |= (rd & 0xF) << 22;
    inst |= (rs1 & 0xF) << 18;
    inst |= (rs2 & 0xF) << 14;
    inst |= (static_cast<std::uint32_t>(imm11) & 0x7FF) << 3;
    return inst;
}

// KR32 8-byte encoding:
//   Low 32 bits: [31:26] opcode (6) [rd:4] [rs1:4] [imm11:11] [pad:7]
//   High 32 bits: extension word (full 32-bit immediate or address)
void encode_kr32_8byte(Kr32Opcode op,
                       std::uint32_t rd,
                       std::uint32_t rs1,
                       std::int32_t imm11,
                       std::uint32_t& low,
                       std::uint32_t& high) {
    low = 0;
    low |= (static_cast<std::uint32_t>(op) & 0x3F) << 26;
    low |= (rd & 0xF) << 22;
    low |= (rs1 & 0xF) << 18;
    low |= (static_cast<std::uint32_t>(imm11) & 0x7FF) << 7;
    high = static_cast<std::uint32_t>(imm11);
}

//------------------------------------------------------------------------------
// Target descriptor
//------------------------------------------------------------------------------

const TargetDescriptor& kr32_target_descriptor() {
    return kr32_descriptor();
}

} // namespace krc
