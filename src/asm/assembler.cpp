#include "assembler.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace krc {

Kr32Assembler::Kr32Assembler(const TargetDescriptor& target) : target_(target) {
    (void)target_;
}

std::uint32_t Kr32Assembler::map_vreg(std::uint32_t vreg) {
    auto it = vreg_map_.find(vreg);
    if (it != vreg_map_.end())
        return it->second;
    auto allocatable = kr32_allocatable_gprs();
    std::uint32_t phys;
    if (next_phys_reg_ < allocatable.size()) {
        phys = allocatable[next_phys_reg_];
    } else {
        phys = allocatable[next_phys_reg_ % allocatable.size()];
    }
    vreg_map_[vreg] = phys;
    ++next_phys_reg_;
    return phys;
}

std::uint32_t Kr32Assembler::symbol_index_for(std::string_view name,
                                              SymbolBinding binding,
                                              SymbolType type) {
    auto it = symbol_index_map_.find(std::string(name));
    if (it != symbol_index_map_.end())
        return it->second;
    std::uint32_t idx = static_cast<std::uint32_t>(symbol_index_map_.size());
    symbol_index_map_[std::string(name)] = idx;

    ObjectSymbol sym;
    sym.name = std::string(name);
    sym.value = 0;
    sym.size = 0;
    sym.section_index = 0; // undefined until resolved
    sym.binding = binding;
    sym.type = type;
    sym.visibility = SymbolVisibility::Default;

    return idx;
}

void Kr32Assembler::add_relocation(std::uint32_t section_index,
                                   std::uint64_t offset, RelocType type,
                                   std::uint32_t symbol_index,
                                   std::int64_t addend, std::uint8_t width) {
    ObjectRelocation rel;
    rel.section_index = section_index;
    rel.offset = offset;
    rel.type = type;
    rel.symbol_index = symbol_index;
    rel.addend = addend;
    rel.width = width;
    pending_relocations_.push_back(std::move(rel));
}

std::vector<std::uint8_t> Kr32Assembler::encode_kr32_4byte(
    Kr32Opcode op, std::uint32_t rd, std::uint32_t rs1, std::uint32_t rs2,
    std::int32_t imm11) {
    std::uint32_t inst = krc::encode_kr32_4byte(op, rd, rs1, rs2, imm11);
    std::vector<std::uint8_t> bytes(4);
    bytes[0] = static_cast<std::uint8_t>(inst & 0xFF);
    bytes[1] = static_cast<std::uint8_t>((inst >> 8) & 0xFF);
    bytes[2] = static_cast<std::uint8_t>((inst >> 16) & 0xFF);
    bytes[3] = static_cast<std::uint8_t>((inst >> 24) & 0xFF);
    return bytes;
}

std::vector<std::uint8_t> Kr32Assembler::encode_kr32_8byte(
    Kr32Opcode op, std::uint32_t rd, std::uint32_t rs1, std::int32_t imm11) {
    std::uint32_t low, high;
    krc::encode_kr32_8byte(op, rd, rs1, imm11, low, high);
    std::vector<std::uint8_t> bytes(8);
    for (int i = 0; i < 4; ++i) {
        bytes[i] = static_cast<std::uint8_t>((low >> (i * 8)) & 0xFF);
        bytes[i + 4] = static_cast<std::uint8_t>((high >> (i * 8)) & 0xFF);
    }
    return bytes;
}

std::vector<std::uint8_t> Kr32Assembler::encode_instruction(
    const MachineInstr& inst, std::uint64_t current_offset) {
    (void)current_offset;
    std::vector<std::uint8_t> bytes;

    auto append = [&](const std::vector<std::uint8_t>& b) {
        bytes.insert(bytes.end(), b.begin(), b.end());
    };

    switch (inst.opcode) {
        //-----------------------------------------------------------------------------
        // Pseudo instructions — expand into real KR32 instructions
        //-----------------------------------------------------------------------------

        case MachineInstr::Opcode::PseudoAlloca:
            // Stack allocation is handled by frame layout; no code needed.
            return {};

        case MachineInstr::Opcode::PseudoLoad: {
            // PseudoLoad: operand[0] = pointer (stack slot, global, or reg), operand[1] = def reg
            if (inst.operands.size() < 2)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rd = map_vreg(inst.operands[1].reg);
            if (inst.operands[0].kind == MachineOperand::Kind::StackSlot) {
                // Load from stack slot: LW rd, offset(fp)
                std::int32_t offset = inst.operands[0].stack_offset;
                return encode_kr32_4byte(Kr32Opcode::LW, rd, KR32_FP_REG, 0,
                                         offset);
            } else if (inst.operands[0].kind == MachineOperand::Kind::Global) {
                // Load from global variable: LW rd, (R11) with R_Abs32 relocation
                // Use TP (R11) as a scratch base register; linker patches the
                // address via R_Abs32 relocation.
                std::string target_name = inst.operands[0].name;
                std::uint32_t sym_idx = symbol_index_for(target_name,
                                                         SymbolBinding::Global,
                                                         SymbolType::Object);
                add_relocation(text_section_index(),
                               current_offset, // actual offset within .text
                               RelocType::R_Abs32,
                               sym_idx, 0, 32);
                // Emit a placeholder LW using TP as base (will be patched)
                return encode_kr32_4byte(Kr32Opcode::LW, rd, KR32_TP_REG, 0,
                                         0);
            } else {
                // Load from register-indirect: LW rd, (rs)
                std::uint32_t rs = map_vreg(inst.operands[0].reg);
                return encode_kr32_4byte(Kr32Opcode::LW, rd, rs, 0, 0);
            }
        }

        case MachineInstr::Opcode::PseudoStore: {
            // PseudoStore: operand[0] = value (reg), operand[1] = pointer (stack slot, global, or reg)
            if (inst.operands.size() < 2)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            if (inst.operands[1].kind == MachineOperand::Kind::StackSlot) {
                // Store to stack slot: SW rs, offset(fp)
                std::uint32_t rs = map_vreg(inst.operands[0].reg);
                std::int32_t offset = inst.operands[1].stack_offset;
                return encode_kr32_4byte(Kr32Opcode::SW, KR32_FP_REG, rs, 0,
                                         offset);
            } else if (inst.operands[1].kind == MachineOperand::Kind::Global) {
                // Store to global variable: SW rs, (R11) with R_Abs32 relocation
                std::string target_name = inst.operands[1].name;
                std::uint32_t sym_idx = symbol_index_for(target_name,
                                                         SymbolBinding::Global,
                                                         SymbolType::Object);
                add_relocation(text_section_index(),
                               current_offset, // actual offset within .text
                               RelocType::R_Abs32,
                               sym_idx, 0, 32);
                // Emit a placeholder SW using TP as base (will be patched)
                return encode_kr32_4byte(Kr32Opcode::SW, KR32_TP_REG,
                                         map_vreg(inst.operands[0].reg), 0, 0);
            } else {
                // Store to register-indirect: SW base, rs
                std::uint32_t rs = map_vreg(inst.operands[0].reg);
                std::uint32_t base = map_vreg(inst.operands[1].reg);
                return encode_kr32_4byte(Kr32Opcode::SW, base, rs, 0, 0);
            }
        }

        case MachineInstr::Opcode::PseudoRet:
            return encode_kr32_4byte(Kr32Opcode::RET, 0, 0, 0, 0);

        case MachineInstr::Opcode::PseudoBr: {
            // Unconditional branch to a label.
            // Emit J with a relocation filled in by the linker.
            std::string target_name;
            if (!inst.operands.empty() &&
                inst.operands[0].kind == MachineOperand::Kind::Label)
                target_name = inst.operands[0].name;

            std::uint32_t sym_idx = symbol_index_for(target_name,
                                                     SymbolBinding::Global,
                                                     SymbolType::Function);
            add_relocation(text_section_index(),
                           current_offset, // actual offset within .text
                           RelocType::R_KR32_Branch26,
                           sym_idx, 0, 26);
            return encode_kr32_4byte(Kr32Opcode::J, 0, 0, 0, 0);
        }

        case MachineInstr::Opcode::PseudoCondBr: {
            // Conditional branch: operand[0] = condition reg, operand[1] = true label, operand[2] = false label
            // For v1: emit BEQ cond, R0, target (treats R0 as zero; known limitation).
            // The false fall-through is implicit (next instruction).
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t cond = map_vreg(inst.operands[0].reg);
            std::string target_name;
            if (inst.operands[1].kind == MachineOperand::Kind::Label)
                target_name = inst.operands[1].name;

            std::uint32_t sym_idx = symbol_index_for(target_name,
                                                     SymbolBinding::Global,
                                                     SymbolType::Function);
            add_relocation(text_section_index(),
                           current_offset, // actual offset within .text
                           RelocType::R_KR32_Branch26,
                           sym_idx, 0, 26);
            return encode_kr32_4byte(Kr32Opcode::BEQ, cond, KR32_R0, 0,
                                     0);
        }

        case MachineInstr::Opcode::PseudoCall: {
            // PseudoCall: operand[0] = Global(func name), def = return value reg
            // Emit JAL ra, func_addr + NOP (delay slot), with relocation.
            if (inst.operands.empty())
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::string func_name;
            if (inst.operands[0].kind == MachineOperand::Kind::Global)
                func_name = inst.operands[0].name;

            std::uint32_t sym_idx = symbol_index_for(func_name,
                                                     SymbolBinding::Global,
                                                     SymbolType::Function);
            add_relocation(text_section_index(),
                           current_offset, // actual offset within .text
                           RelocType::R_KR32_Branch26,
                           sym_idx, 0, 26);

            std::vector<std::uint8_t> result;
            // JAL ra, func
            append(encode_kr32_4byte(Kr32Opcode::JAL, KR32_RA_REG, 0, 0, 0));
            // NOP delay slot
            append(encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0));
            return result;
        }

        case MachineInstr::Opcode::PseudoPhi:
            // Phi nodes are SSA bookkeeping; no code needed after regalloc.
            return {};

        //-----------------------------------------------------------------------------
        // Real KR32 instructions — encode directly
        //-----------------------------------------------------------------------------

        case MachineInstr::Opcode::Kr32Nop:
            return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);

        case MachineInstr::Opcode::Kr32Add: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr32_4byte(Kr32Opcode::ADD, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr32Sub: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr32_4byte(Kr32Opcode::SUB, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr32Mul: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr32_4byte(Kr32Opcode::MUL, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr32And: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr32_4byte(Kr32Opcode::AND, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr32Or: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr32_4byte(Kr32Opcode::OR, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr32Xor: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr32_4byte(Kr32Opcode::XOR, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr32Shl: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr32_4byte(Kr32Opcode::SHL, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr32Shr: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr32_4byte(Kr32Opcode::SHR, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr32Sra: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr32_4byte(Kr32Opcode::SRA, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr32Addi: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::int32_t imm = inst.operands[1].imm;
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr32_4byte(Kr32Opcode::ADDI, rd, rs1, 0, imm);
        }
        case MachineInstr::Opcode::Kr32Lw: {
            if (inst.operands.size() < 2)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rd = map_vreg(inst.operands[0].reg);
            std::uint32_t base = map_vreg(inst.operands[1].reg);
            std::int32_t offset = 0;
            if (inst.operands.size() > 2)
                offset = inst.operands[2].imm;
            return encode_kr32_4byte(Kr32Opcode::LW, rd, base, 0, offset);
        }
        case MachineInstr::Opcode::Kr32Sw: {
            if (inst.operands.size() < 2)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs = map_vreg(inst.operands[0].reg);
            std::uint32_t base = map_vreg(inst.operands[1].reg);
            std::int32_t offset = 0;
            if (inst.operands.size() > 2)
                offset = inst.operands[2].imm;
            return encode_kr32_4byte(Kr32Opcode::SW, base, rs, 0, offset);
        }
        case MachineInstr::Opcode::Kr32Li: {
            std::uint32_t rd = 0;
            std::int32_t imm = 0;
            if (!inst.operands.empty()) {
                rd = map_vreg(inst.operands[0].reg);
                if (inst.operands.size() > 1)
                    imm = inst.operands[1].imm;
            }
            if (imm11_fits(imm)) {
                return encode_kr32_4byte(Kr32Opcode::ADDI, rd, KR32_R0, 0,
                                         imm);
            } else {
                return encode_kr32_8byte(Kr32Opcode::LI, rd, KR32_R0,
                                         imm);
            }
        }
        case MachineInstr::Opcode::Kr32Beq: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            return encode_kr32_4byte(Kr32Opcode::BEQ, rs1, rs2, 0, 0);
        }
        case MachineInstr::Opcode::Kr32Bne: {
            if (inst.operands.size() < 3)
                return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            return encode_kr32_4byte(Kr32Opcode::BNE, rs1, rs2, 0, 0);
        }
        case MachineInstr::Opcode::Kr32J: {
            return encode_kr32_4byte(Kr32Opcode::J, 0, 0, 0, 0);
        }
        case MachineInstr::Opcode::Kr32Jal: {
            return encode_kr32_4byte(Kr32Opcode::JAL, KR32_RA_REG, 0, 0, 0);
        }
        case MachineInstr::Opcode::Kr32Ret:
            return encode_kr32_4byte(Kr32Opcode::RET, 0, 0, 0, 0);

        default:
            return encode_kr32_4byte(Kr32Opcode::NOP, 0, 0, 0, 0);
    }
}

bool Kr32Assembler::assemble(const MachineModule& mm, ObjectFile& obj) {
    obj.target_id = mm.target_id;
    obj.kind = ObjectKind::Object;
    obj.flags = static_cast<std::uint16_t>(ObjectFlags::HasSymbols);
    obj.pointer_width = 32;
    obj.endianness = 0; // little-endian
    obj.target_numeric_id = 1; // KR32

    // ---- First pass: create the undefined symbol (index 0) ----
    {
        ObjectSymbol undef_sym;
        undef_sym.name = "";
        undef_sym.value = 0;
        undef_sym.size = 0;
        undef_sym.section_index = 0; // undefined
        undef_sym.binding = SymbolBinding::Local;
        undef_sym.type = SymbolType::Notype;
        obj.symbols.push_back(std::move(undef_sym));
    }

    // ---- Second pass: create function symbols ----
    std::unordered_map<std::string, std::uint32_t> func_symbol_index;
    for (const auto& mfn : mm.functions) {
        std::uint32_t idx = static_cast<std::uint32_t>(obj.symbols.size());
        func_symbol_index[mfn->name] = idx;

        ObjectSymbol sym;
        sym.name = mfn->name;
        sym.value = 0;           // filled after layout
        sym.size = 0;            // filled after layout
        sym.section_index = 1;   // .text is section index 1
        sym.binding = SymbolBinding::Global;
        sym.type = SymbolType::Function;
        sym.visibility = SymbolVisibility::Default;
        obj.symbols.push_back(std::move(sym));
    }

    // ---- Third pass: create global variable symbols ----
    // Globals are placed in .rodata (constants) or .data (mutable).
    // Section indices: 1=.text, 2=.rodata, 3=.data.
    std::unordered_map<std::string, std::uint32_t> global_symbol_index;
    for (const auto& mg : mm.globals) {
        std::uint32_t idx = static_cast<std::uint32_t>(obj.symbols.size());
        global_symbol_index[mg->name] = idx;

        ObjectSymbol sym;
        sym.name = mg->name;
        sym.value = 0; // offset within section, filled after layout
        sym.size = mg->size;
        sym.section_index = mg->is_constant ? 2 : 3; // .rodata or .data
        sym.binding = SymbolBinding::Global;
        sym.type = static_cast<SymbolType>(mg->symbol_type);
        sym.visibility = SymbolVisibility::Default;
        obj.symbols.push_back(std::move(sym));
    }

    // ---- Fourth pass: create .text section and encode all instructions ----
    ObjectSection text_sec;
    text_sec.name = ".text";
    text_sec.type = SectionType::Text;
    text_sec.flags = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(SectionFlags::Alloc)
        | static_cast<std::uint16_t>(SectionFlags::Execute));
    text_sec.alignment = 4;

    std::size_t text_cursor = 0;
    std::unordered_map<std::string, std::uint64_t> func_addresses;

    for (const auto& mfn : mm.functions) {
        func_addresses[mfn->name] = text_cursor;
        obj.symbols[func_symbol_index[mfn->name]].value = text_cursor;

        for (const auto& bb : mfn->blocks) {
            for (const auto& inst : bb->instructions) {
                auto bytes = encode_instruction(*inst, text_cursor);
                text_sec.payload.insert(text_sec.payload.end(), bytes.begin(),
                                        bytes.end());
                text_cursor += bytes.size();
            }
        }
    }

    obj.relocations = std::move(pending_relocations_);

    // Set function sizes (approximate: distance to next function or end of text)
    std::size_t nfuncs = mm.functions.size();
    for (std::size_t i = 0; i < nfuncs; ++i) {
        auto it = func_symbol_index.find(mm.functions[i]->name);
        if (it != func_symbol_index.end()) {
            std::uint64_t start = obj.symbols[it->second].value;
            std::uint64_t end =
                (i + 1 < nfuncs)
                    ? func_addresses[mm.functions[i + 1]->name]
                    : static_cast<std::uint64_t>(text_cursor);
            obj.symbols[it->second].size = end - start;
        }
    }

    text_sec.payload.shrink_to_fit();
    obj.sections.push_back(std::move(text_sec));

    // ---- Fifth pass: create .rodata section for constants ----
    bool has_rodata = false;
    for (const auto& mg : mm.globals) {
        if (mg->is_constant && !mg->initializer.empty()) {
            has_rodata = true;
            break;
        }
    }
    if (has_rodata) {
        ObjectSection rodata_sec;
        rodata_sec.name = ".rodata";
        rodata_sec.type = SectionType::Rodata;
        rodata_sec.flags = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(SectionFlags::Alloc));
        rodata_sec.alignment = 4;

        std::uint64_t rodata_cursor = 0;
        for (const auto& mg : mm.globals) {
            if (!mg->is_constant || mg->initializer.empty())
                continue;
            // Align to global alignment.
            if (mg->alignment > 1) {
                rodata_cursor = (rodata_cursor + mg->alignment - 1) /
                                mg->alignment * mg->alignment;
            }
            // Update symbol value to offset within .rodata.
            auto gsi = global_symbol_index.find(mg->name);
            if (gsi != global_symbol_index.end()) {
                obj.symbols[gsi->second].value = rodata_cursor;
            }
            // Append initializer bytes.
            rodata_sec.payload.insert(rodata_sec.payload.end(),
                                      mg->initializer.begin(),
                                      mg->initializer.end());
            rodata_cursor += mg->initializer.size();
        }
        rodata_sec.payload.shrink_to_fit();
        obj.sections.push_back(std::move(rodata_sec));
    }

    // ---- Sixth pass: create .data section for mutable globals ----
    bool has_data = false;
    for (const auto& mg : mm.globals) {
        if (!mg->is_constant && !mg->initializer.empty()) {
            has_data = true;
            break;
        }
    }
    if (has_data) {
        ObjectSection data_sec;
        data_sec.name = ".data";
        data_sec.type = SectionType::Data;
        data_sec.flags = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(SectionFlags::Alloc)
            | static_cast<std::uint16_t>(SectionFlags::Write));
        data_sec.alignment = 4;

        std::uint64_t data_cursor = 0;
        for (const auto& mg : mm.globals) {
            if (mg->is_constant || mg->initializer.empty())
                continue;
            // Align to global alignment.
            if (mg->alignment > 1) {
                data_cursor = (data_cursor + mg->alignment - 1) /
                              mg->alignment * mg->alignment;
            }
            // Update symbol value to offset within .data.
            auto gsi = global_symbol_index.find(mg->name);
            if (gsi != global_symbol_index.end()) {
                obj.symbols[gsi->second].value = data_cursor;
            }
            // Append initializer bytes.
            data_sec.payload.insert(data_sec.payload.end(),
                                    mg->initializer.begin(),
                                    mg->initializer.end());
            data_cursor += mg->initializer.size();
        }
        data_sec.payload.shrink_to_fit();
        obj.sections.push_back(std::move(data_sec));
    }

    return true;
}

} // namespace krc
