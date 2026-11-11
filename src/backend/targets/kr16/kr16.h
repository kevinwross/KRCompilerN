#ifndef KRC_KR16_H
#define KRC_KR16_H

#include "target_descriptor.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace krc {

//------------------------------------------------------------------------------
// KR16 target-specific definitions (plan.md §9.6)
//------------------------------------------------------------------------------

// KR16 register numbers.
enum Kr16Reg : std::uint32_t {
    KR16_R0 = 0,   // return value / argument
    KR16_R1 = 1,   // argument
    KR16_R2 = 2,   // argument
    KR16_R3 = 3,   // argument
    KR16_R4 = 4,   // frame pointer (callee-saved)
};

// Special-purpose registers (per KR16 ABI).
inline constexpr std::uint32_t KR16_FP_REG = KR16_R4;   // frame pointer
inline constexpr std::uint32_t KR16_SP_REG = KR16_R3;   // stack pointer (by convention)
inline constexpr std::uint32_t KR16_RA_REG = KR16_R4;   // return address (reuses FP)
inline constexpr std::uint32_t KR16_RETVAL_REG = KR16_R0;

// Argument registers: R0-R3.
inline constexpr std::uint32_t KR16_ARG_REGS[4] = {
    KR16_R0, KR16_R1, KR16_R2, KR16_R3,
};

// Callee-saved registers: R4.
inline constexpr std::uint32_t KR16_CALLEE_SAVED_REGS[1] = {
    KR16_R4,
};

// Caller-saved registers: R0-R3.
inline constexpr std::uint32_t KR16_CALLER_SAVED_REGS[4] = {
    KR16_R0, KR16_R1, KR16_R2, KR16_R3,
};

// No reserved registers on KR16 (no thread pointer).

//------------------------------------------------------------------------------
// KR16 instruction encoding
//------------------------------------------------------------------------------
//
// KR16 uses fixed 16-bit instructions:
//   R-type: [opcode:4][rd:2][rs1:2][rs2:2][funct:6] = 16 bits
//   I-type: [opcode:4][rd:2][rs1:2][imm:8] = 16 bits
//   L/S:    [opcode:4][rd:2][base:2][imm:8] = 16 bits
//   B:      [opcode:4][cond:2][rs1:2][imm:8] = 16 bits
//   J:      [opcode:4][rd:2][imm:10] = 16 bits

enum class Kr16InstFmt : std::uint8_t {
    R,     // register-register (2 bytes)
    I,     // register-immediate (2 bytes)
    L,     // load (2 bytes)
    S,     // store (2 bytes)
    B,     // branch (2 bytes)
    J,     // jump (2 bytes)
};

// KR16 opcodes.
enum class Kr16Opcode : std::uint8_t {
    // Integer ALU (R-type)
    ADD  = 0x1,
    SUB  = 0x2,
    MUL  = 0x3,
    DIV  = 0x4,
    REM  = 0x5,
    AND  = 0x6,
    OR   = 0x7,
    XOR  = 0x8,
    SHL  = 0x9,
    SHR  = 0xA,
    // Integer ALU immediate (I-type)
    ADDI = 0x10,
    // Loads
    LB   = 0x20,  // load byte (sign-extended)
    LBU  = 0x21,  // load byte unsigned
    LH   = 0x22,  // load half (sign-extended)
    LHU  = 0x23,  // load half unsigned
    LW   = 0x24,  // load word (16-bit on KR16)
    // Stores
    SB   = 0x30,  // store byte
    SH   = 0x31,  // store half
    SW   = 0x32,  // store word (16-bit on KR16)
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
    // Load immediate (pseudo, expanded to ADDI)
    LI   = 0x70,
    NOP  = 0x7F,
};

// Encode a 2-byte KR16 instruction.
std::uint16_t encode_kr16_2byte(Kr16Opcode op,
                                std::uint32_t rd = 0,
                                std::uint32_t rs1 = 0,
                                std::uint32_t rs2 = 0,
                                std::int32_t imm8 = 0);

// Check if an immediate fits in 8 bits (signed).
bool imm8_fits(std::int32_t imm);

// Return the register name for printing.
std::string kr16_reg_name(std::uint32_t reg);

// Return true if the register is caller-saved.
bool kr16_is_caller_saved(std::uint32_t reg);

// Return true if the register is callee-saved.
bool kr16_is_callee_saved(std::uint32_t reg);

// Return true if the register is reserved.
bool kr16_is_reserved(std::uint32_t reg);

// Return the number of allocatable general-purpose registers.
std::uint32_t kr16_num_allocatable_gprs();

// Return the list of allocatable GPR numbers (excluding reserved).
std::vector<std::uint32_t> kr16_allocatable_gprs();

//------------------------------------------------------------------------------
// KR16 target descriptor accessor
//------------------------------------------------------------------------------

const TargetDescriptor& kr16_target_descriptor();

} // namespace krc

#endif // KRC_KR16_H
