#ifndef KRC_MACHINE_H
#define KRC_MACHINE_H

#include "target_descriptor.h"

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace krc {

//------------------------------------------------------------------------------
// Machine instruction representation (target-independent)
//------------------------------------------------------------------------------

// A machine operand: a register, an immediate, a stack slot, or a label.
struct MachineOperand {
    enum class Kind : std::uint8_t {
        Invalid,
        Reg,        // virtual or physical register
        Imm,        // 32-bit immediate
        FpImm,      // 64-bit float immediate (bits)
        StackSlot,  // stack slot offset (from frame pointer)
        Global,     // global symbol name (as string)
        Label,      // basic block label
    };

    Kind kind = Kind::Invalid;

    // For Reg:
    std::uint32_t reg = 0;        // register number (virtual or physical)
    bool is_def = false;          // true if this operand defines the reg
    bool is_use = false;          // true if this operand uses the reg

    // For Imm:
    std::int32_t imm = 0;

    // For FpImm:
    std::uint64_t fp_bits = 0;

    // For StackSlot:
    std::int32_t stack_offset = 0;

    // For Global / Label:
    std::string name;

    MachineOperand() = default;
    static MachineOperand RegUse(std::uint32_t r) {
        MachineOperand o; o.kind = Kind::Reg; o.reg = r; o.is_use = true; return o;
    }
    static MachineOperand RegDef(std::uint32_t r) {
        MachineOperand o; o.kind = Kind::Reg; o.reg = r; o.is_def = true; return o;
    }
    static MachineOperand Imm(std::int32_t v) {
        MachineOperand o; o.kind = Kind::Imm; o.imm = v; return o;
    }
    static MachineOperand StackSlot(std::int32_t off) {
        MachineOperand o; o.kind = Kind::StackSlot; o.stack_offset = off; return o;
    }
    static MachineOperand Global(std::string_view n) {
        MachineOperand o; o.kind = Kind::Global; o.name = n; return o;
    }
    static MachineOperand Label(std::string_view n) {
        MachineOperand o; o.kind = Kind::Label; o.name = n; return o;
    }
};

// A machine instruction: opcode plus operands.
struct MachineInstr {
    enum class Opcode : std::uint8_t {
        Invalid,
        // Pseudo instructions (lowered by backend passes)
        PseudoAlloca,
        PseudoLoad,
        PseudoStore,
        PseudoRet,
        PseudoBr,
        PseudoCondBr,
        PseudoCall,
        PseudoPhi,
        // KR32 instructions (will be encoded by the assembler)
        Kr32Add, Kr32Sub, Kr32Mul, Kr32Div, Kr32Rem,
        Kr32And, Kr32Or, Kr32Xor, Kr32Shl, Kr32Shr, Kr32Sra,
        Kr32Load, Kr32Store,
        Kr32Addi, Kr32Lw, Kr32Sw, Kr32Li,
        Kr32Beq, Kr32Bne, Kr32Blt, Kr32Bgt, Kr32Ble, Kr32Bge,
        Kr32J, Kr32Jal, Kr32Ret,
        Kr32Nop,
        // KR16 instructions (will be encoded by the KR16 assembler)
        Kr16Add, Kr16Sub, Kr16Mul, Kr16Div, Kr16Rem,
        Kr16And, Kr16Or, Kr16Xor, Kr16Shl, Kr16Shr,
        Kr16Addi,
        Kr16Load, Kr16Store,
        Kr16Lw, Kr16Sw, Kr16Li,
        Kr16Beq, Kr16Bne, Kr16Blt, Kr16Bgt, Kr16Ble, Kr16Bge,
        Kr16J, Kr16Jal, Kr16Ret,
        Kr16Nop,
    };

    Opcode opcode = Opcode::Invalid;
    std::vector<MachineOperand> operands;

    // Source location for debug info.
    std::string_view source_file;
    std::uint32_t source_line = 0;

    MachineInstr() = default;
    explicit MachineInstr(Opcode op) : opcode(op) {}
};

// A basic block of machine instructions.
struct MachineBasicBlock {
    std::string name;
    std::vector<std::unique_ptr<MachineInstr>> instructions;
    // Successor block names (for branch targets).
    std::vector<std::string> successors;

    MachineBasicBlock(std::string n) : name(std::move(n)) {}
};

// A machine function: a collection of basic blocks.
struct MachineFunction {
    std::string name;
    std::vector<std::unique_ptr<MachineBasicBlock>> blocks;
    MachineBasicBlock* entry_block = nullptr;
    std::int32_t frame_size = 0;  // set by frame layout

    // Debug info: function address range (set by assembler/linker after
    // code layout).
    std::uint64_t debug_start_address = 0;
    std::uint64_t debug_size = 0;

    // Debug info: frame/unwind record (set by frame layout).
    // 0xFFFFFFFF means "none" (matches KDBG convention).
    std::uint32_t debug_saved_fp_offset = 0xFFFFFFFFu;
    std::uint32_t debug_saved_ra_offset = 0xFFFFFFFFu;
    std::uint32_t debug_cfa_rule = 0;

    // Debug info: variable locations (collected by backend debug pass).
    struct MachineVarDebugInfo {
        std::string name;
        std::uint32_t type_id = 0;          // KDBG type record ID
        std::int32_t stack_offset = 0;      // frame-relative offset (-1 if register)
        std::uint8_t location_kind = 0;     // 0=frame_offset, 1=register, 2=global_addr
        std::uint8_t location_reg = 0;      // register number if location_kind == register
        std::uint8_t flags = 0;             // 0=local, 1=param, 2=static, 3=global, 4=TLS
        std::uint8_t reserved = 0;
    };
    std::vector<MachineVarDebugInfo> debug_variables;

    MachineFunction(std::string n) : name(std::move(n)) {}
};

// A machine global variable: name, size, initializer bytes, alignment.
// is_constant=true => .rodata, false => .data.
struct MachineGlobal {
    std::string name;
    std::uint64_t size = 0;
    std::vector<std::uint8_t> initializer; // initializer bytes (may be empty for .bss)
    std::uint32_t alignment = 4;
    bool is_constant = false;
    std::uint8_t symbol_type = 1; // 1=Object, 2=Function (mirrors SymbolType)
};

// A machine module: all functions and globals.
struct MachineModule {
    std::string target_id;
    std::vector<std::unique_ptr<MachineFunction>> functions;
    std::vector<std::unique_ptr<MachineGlobal>> globals;

    MachineModule(std::string tid) : target_id(std::move(tid)) {}
};

// Print a machine module (for debugging / assembly output).
void dump_machine_module(const MachineModule& mm, std::ostream& out);

} // namespace krc

#endif // KRC_MACHINE_H
