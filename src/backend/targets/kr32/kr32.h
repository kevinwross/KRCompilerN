#ifndef KRC_KR32_H
#define KRC_KR32_H

#include "target_descriptor.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace krc {

//------------------------------------------------------------------------------
// KR32 target-specific definitions (plan.md §9.5)
//------------------------------------------------------------------------------

// KR32 register numbers.
enum Kr32Reg : std::uint32_t {
    KR32_R0 = 0,   KR32_R1,   KR32_R2,   KR32_R3,
    KR32_R4,   KR32_R5,   KR32_R6,   KR32_R7,  // frame pointer
    KR32_R8,   KR32_R9,   KR32_R10,  KR32_R11, // thread pointer
    KR32_R12,  KR32_R13,  KR32_R14,  KR32_R15,  // stack pointer (by convention)
};

// Special-purpose registers (per KR32 ABI).
inline constexpr std::uint32_t KR32_FP_REG = KR32_R7;   // frame pointer
inline constexpr std::uint32_t KR32_TP_REG = KR32_R11;  // thread pointer
inline constexpr std::uint32_t KR32_SP_REG = KR32_R15;  // stack pointer
inline constexpr std::uint32_t KR32_RA_REG = KR32_R14;  // return address

// Return value register.
inline constexpr std::uint32_t KR32_RETVAL_REG = KR32_R0;

// Argument registers: R0-R3.
inline constexpr std::uint32_t KR32_ARG_REGS[4] = {
    KR32_R0, KR32_R1, KR32_R2, KR32_R3,
};

// Callee-saved registers: R7.
inline constexpr std::uint32_t KR32_CALLEE_SAVED_REGS[1] = {
    KR32_R7,
};

// Caller-saved registers: R0-R3.
inline constexpr std::uint32_t KR32_CALLER_SAVED_REGS[4] = {
    KR32_R0, KR32_R1, KR32_R2, KR32_R3,
};

// Reserved registers: R11 (thread pointer).
inline constexpr std::uint32_t KR32_RESERVED_REGS[1] = {
    KR32_R11,
};

//------------------------------------------------------------------------------
// KR32 instruction encoding
//------------------------------------------------------------------------------

// KR32 instruction formats (plan.md §9.5):
//   4-byte instructions: opcode(6) + rd(4) + rs1(4) + rs2(4) + imm11(11) + unused(3) = 32 bits
//   8-byte instructions: opcode(6) + ... + ext_word(32) = 64 bits
//
// For v1, we use a simplified encoding:
//   4-byte: [opcode:6][rd:4][rs1:4][rs2:4][imm11:11][pad:3]
//   8-byte: [opcode:6][rd:4][rs1:4][imm11:11][pad:7][ext:32]

enum class Kr32InstFmt : std::uint8_t {
    R,     // register-register (4 bytes)
    I,     // register-immediate (4 bytes)
    L,     // load (4 bytes)
    S,     // store (4 bytes)
    B,     // branch (4 bytes)
    J,     // jump (4 bytes)
    R2,    // register-register extended (8 bytes)
    I2,    // immediate extended (8 bytes)
};

// KR32 opcodes.
enum class Kr32Opcode : std::uint8_t {
    // Integer ALU (R-type)
    ADD  = 0x01,
    SUB  = 0x02,
    MUL  = 0x03,
    DIV  = 0x04,
    REM  = 0x05,
    AND  = 0x06,
    OR   = 0x07,
    XOR  = 0x08,
    SHL  = 0x09,
    SHR  = 0x0A,
    SRA  = 0x0B,
    // Integer ALU immediate (I-type)
    ADDI = 0x10,
    // Loads
    LB   = 0x20,  // load byte (sign-extended)
    LBU  = 0x21,  // load byte unsigned
    LH   = 0x22,  // load half (sign-extended)
    LHU  = 0x23,  // load half unsigned
    LW   = 0x24,  // load word
    // Stores
    SB   = 0x30,  // store byte
    SH   = 0x31,  // store half
    SW   = 0x32,  // store word
    // Branches (B-type)
    BEQ  = 0x40,
    BNE  = 0x41,
    BLT  = 0x42,
    BGT  = 0x43,
    BLE  = 0x44,
    BGE  = 0x45,
    // Jumps
    J    = 0x50,
    JAL  = 0x51,
    // Return
    RET  = 0x60,
    // Load immediate (pseudo, expanded to ADDI or LUI+ADDI)
    LI   = 0x70,
    NOP  = 0x7F,
};

// Encode a 4-byte KR32 instruction.
std::uint32_t encode_kr32_4byte(Kr32Opcode op,
                                std::uint32_t rd = 0,
                                std::uint32_t rs1 = 0,
                                std::uint32_t rs2 = 0,
                                std::int32_t imm11 = 0);

// Encode an 8-byte KR32 instruction (returns low and high 32 bits).
void encode_kr32_8byte(Kr32Opcode op,
                       std::uint32_t rd,
                       std::uint32_t rs1,
                       std::int32_t imm11,
                       std::uint32_t& low,
                       std::uint32_t& high);

// Check if an immediate fits in 11 bits (signed).
bool imm11_fits(std::int32_t imm);

// Return the register class name for printing.
std::string kr32_reg_name(std::uint32_t reg);

// Return true if the register is caller-saved.
bool kr32_is_caller_saved(std::uint32_t reg);

// Return true if the register is callee-saved.
bool kr32_is_callee_saved(std::uint32_t reg);

// Return true if the register is reserved (thread pointer, etc.).
bool kr32_is_reserved(std::uint32_t reg);

// Return the number of allocatable general-purpose registers.
std::uint32_t kr32_num_allocatable_gprs();

// Return the list of allocatable GPR numbers (excluding reserved).
std::vector<std::uint32_t> kr32_allocatable_gprs();

//------------------------------------------------------------------------------
// KR32 target descriptor accessor
//------------------------------------------------------------------------------

const TargetDescriptor& kr32_target_descriptor();

} // namespace krc

#endif // KRC_KR32_H
