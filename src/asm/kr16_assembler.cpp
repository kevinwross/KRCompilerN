#include "kr16_assembler.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace krc {

Kr16Assembler::Kr16Assembler(const TargetDescriptor& target) : target_(target) {
    (void)target_;
}

std::uint32_t Kr16Assembler::map_vreg(std::uint32_t vreg) {
    auto it = vreg_map_.find(vreg);
    if (it != vreg_map_.end())
        return it->second;
    auto allocatable = kr16_allocatable_gprs();
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

std::uint32_t Kr16Assembler::symbol_index_for(std::string_view name,
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

void Kr16Assembler::add_relocation(std::uint32_t section_index,
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

std::vector<std::uint8_t> Kr16Assembler::encode_kr16_2byte(
    Kr16Opcode op, std::uint32_t rd, std::uint32_t rs1, std::uint32_t rs2,
    std::int32_t imm8) {
    std::uint16_t inst = krc::encode_kr16_2byte(op, rd, rs1, rs2, imm8);
    std::vector<std::uint8_t> bytes(2);
    bytes[0] = static_cast<std::uint8_t>(inst & 0xFF);
    bytes[1] = static_cast<std::uint8_t>((inst >> 8) & 0xFF);
    return bytes;
}

std::vector<std::uint8_t> Kr16Assembler::encode_instruction(
    const MachineInstr& inst, std::uint64_t current_offset) {
    (void)current_offset;
    std::vector<std::uint8_t> bytes;

    auto append = [&](const std::vector<std::uint8_t>& b) {
        bytes.insert(bytes.end(), b.begin(), b.end());
    };

    switch (inst.opcode) {
        //-----------------------------------------------------------------------------
        // Pseudo instructions — expand into real KR16 instructions
        //-----------------------------------------------------------------------------

        case MachineInstr::Opcode::PseudoAlloca:
            // Stack allocation is handled by frame layout; no code needed.
            return {};

        case MachineInstr::Opcode::PseudoLoad: {
            // PseudoLoad: operand[0] = pointer, operand[1] = def reg
            if (inst.operands.size() < 2)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rd = map_vreg(inst.operands[1].reg);
            if (inst.operands[0].kind == MachineOperand::Kind::StackSlot) {
                // Load from stack slot: LW rd, offset(fp)
                std::int32_t offset = inst.operands[0].stack_offset;
                return encode_kr16_2byte(Kr16Opcode::LW, rd, KR16_FP_REG, 0,
                                         offset);
            } else if (inst.operands[0].kind == MachineOperand::Kind::Global) {
                // Load from global variable: LW rd, (base) with R_Abs16 relocation
                std::string target_name = inst.operands[0].name;
                std::uint32_t sym_idx = symbol_index_for(target_name,
                                                         SymbolBinding::Global,
                                                         SymbolType::Object);
                add_relocation(text_section_index(),
                               current_offset,
                               RelocType::R_Abs16,
                               sym_idx, 0, 16);
                // Emit a placeholder LW using R0 as base (will be patched)
                return encode_kr16_2byte(Kr16Opcode::LW, rd, KR16_R0, 0,
                                         0);
            } else {
                // Load from register-indirect: LW rd, (rs)
                std::uint32_t rs = map_vreg(inst.operands[0].reg);
                return encode_kr16_2byte(Kr16Opcode::LW, rd, rs, 0, 0);
            }
        }

        case MachineInstr::Opcode::PseudoStore: {
            // PseudoStore: operand[0] = value (reg), operand[1] = pointer
            if (inst.operands.size() < 2)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            if (inst.operands[1].kind == MachineOperand::Kind::StackSlot) {
                // Store to stack slot: SW rs, offset(fp)
                std::uint32_t rs = map_vreg(inst.operands[0].reg);
                std::int32_t offset = inst.operands[1].stack_offset;
                return encode_kr16_2byte(Kr16Opcode::SW, KR16_FP_REG, rs, 0,
                                         offset);
            } else if (inst.operands[1].kind == MachineOperand::Kind::Global) {
                // Store to global variable: SW rs, (base) with R_Abs16 relocation
                std::string target_name = inst.operands[1].name;
                std::uint32_t sym_idx = symbol_index_for(target_name,
                                                         SymbolBinding::Global,
                                                         SymbolType::Object);
                add_relocation(text_section_index(),
                               current_offset,
                               RelocType::R_Abs16,
                               sym_idx, 0, 16);
                // Emit a placeholder SW using R0 as base (will be patched)
                return encode_kr16_2byte(Kr16Opcode::SW, KR16_R0,
                                         map_vreg(inst.operands[0].reg), 0, 0);
            } else {
                // Store to register-indirect: SW base, rs
                std::uint32_t rs = map_vreg(inst.operands[0].reg);
                std::uint32_t base = map_vreg(inst.operands[1].reg);
                return encode_kr16_2byte(Kr16Opcode::SW, base, rs, 0, 0);
            }
        }

        case MachineInstr::Opcode::PseudoRet:
            return encode_kr16_2byte(Kr16Opcode::RET, 0, 0, 0, 0);

        case MachineInstr::Opcode::PseudoBr: {
            // Unconditional branch to a label.
            std::string target_name;
            if (!inst.operands.empty() &&
                inst.operands[0].kind == MachineOperand::Kind::Label)
                target_name = inst.operands[0].name;

            std::uint32_t sym_idx = symbol_index_for(target_name,
                                                     SymbolBinding::Global,
                                                     SymbolType::Function);
            add_relocation(text_section_index(),
                           current_offset,
                           RelocType::R_KR16_Branch10,
                           sym_idx, 0, 10);
            return encode_kr16_2byte(Kr16Opcode::J, 0, 0, 0, 0);
        }

        case MachineInstr::Opcode::PseudoCondBr: {
            // Conditional branch: operand[0] = condition reg, operand[1] = true label, operand[2] = false label
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t cond = map_vreg(inst.operands[0].reg);
            std::string target_name;
            if (inst.operands[1].kind == MachineOperand::Kind::Label)
                target_name = inst.operands[1].name;

            std::uint32_t sym_idx = symbol_index_for(target_name,
                                                     SymbolBinding::Global,
                                                     SymbolType::Function);
            add_relocation(text_section_index(),
                           current_offset,
                           RelocType::R_KR16_Branch8,
                           sym_idx, 0, 8);
            return encode_kr16_2byte(Kr16Opcode::BEQ, cond, KR16_R0, 0, 0);
        }

        case MachineInstr::Opcode::PseudoCall: {
            // PseudoCall: operand[0] = Global(func name), def = return value reg
            if (inst.operands.empty())
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::string func_name;
            if (inst.operands[0].kind == MachineOperand::Kind::Global)
                func_name = inst.operands[0].name;

            std::uint32_t sym_idx = symbol_index_for(func_name,
                                                     SymbolBinding::Global,
                                                     SymbolType::Function);
            add_relocation(text_section_index(),
                           current_offset,
                           RelocType::R_KR16_Branch10,
                           sym_idx, 0, 10);

            std::vector<std::uint8_t> result;
            // JAL ra, func
            append(encode_kr16_2byte(Kr16Opcode::JAL, KR16_RA_REG, 0, 0, 0));
            return result;
        }

        case MachineInstr::Opcode::PseudoPhi:
            // Phi nodes are SSA bookkeeping; no code needed after regalloc.
            return {};

        //-----------------------------------------------------------------------------
        // Real KR16 instructions — encode directly
        //-----------------------------------------------------------------------------

        case MachineInstr::Opcode::Kr16Nop:
            return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);

        case MachineInstr::Opcode::Kr16Add: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::ADD, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr16Sub: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::SUB, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr16Mul: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::MUL, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr16Div: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::DIV, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr16Rem: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::REM, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr16And: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::AND, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr16Or: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::OR, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr16Xor: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::XOR, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr16Shl: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::SHL, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr16Shr: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::SHR, rd, rs1, rs2, 0);
        }
        case MachineInstr::Opcode::Kr16Addi: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::int32_t imm = inst.operands[1].imm;
            std::uint32_t rd = map_vreg(inst.operands[2].reg);
            return encode_kr16_2byte(Kr16Opcode::ADDI, rd, rs1, 0, imm);
        }
        case MachineInstr::Opcode::Kr16Lw: {
            if (inst.operands.size() < 2)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rd = map_vreg(inst.operands[0].reg);
            std::uint32_t base = map_vreg(inst.operands[1].reg);
            std::int32_t offset = 0;
            if (inst.operands.size() > 2)
                offset = inst.operands[2].imm;
            return encode_kr16_2byte(Kr16Opcode::LW, rd, base, 0, offset);
        }
        case MachineInstr::Opcode::Kr16Sw: {
            if (inst.operands.size() < 2)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs = map_vreg(inst.operands[0].reg);
            std::uint32_t base = map_vreg(inst.operands[1].reg);
            std::int32_t offset = 0;
            if (inst.operands.size() > 2)
                offset = inst.operands[2].imm;
            return encode_kr16_2byte(Kr16Opcode::SW, base, rs, 0, offset);
        }
        case MachineInstr::Opcode::Kr16Li: {
            std::uint32_t rd = 0;
            std::int32_t imm = 0;
            if (!inst.operands.empty()) {
                rd = map_vreg(inst.operands[0].reg);
                if (inst.operands.size() > 1)
                    imm = inst.operands[1].imm;
            }
            if (imm8_fits(imm)) {
                return encode_kr16_2byte(Kr16Opcode::ADDI, rd, KR16_R0, 0,
                                         imm);
            } else {
                // Immediate doesn't fit in 8 bits; emit NOP placeholder.
                // Branch relaxation would expand this, but for v1 we just NOP.
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            }
        }
        case MachineInstr::Opcode::Kr16Beq: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            return encode_kr16_2byte(Kr16Opcode::BEQ, rs1, rs2, 0, 0);
        }
        case MachineInstr::Opcode::Kr16Bne: {
            if (inst.operands.size() < 3)
                return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
            std::uint32_t rs1 = map_vreg(inst.operands[0].reg);
            std::uint32_t rs2 = map_vreg(inst.operands[1].reg);
            return encode_kr16_2byte(Kr16Opcode::BNE, rs1, rs2, 0, 0);
        }
        case MachineInstr::Opcode::Kr16J: {
            return encode_kr16_2byte(Kr16Opcode::J, 0, 0, 0, 0);
        }
        case MachineInstr::Opcode::Kr16Jal: {
            return encode_kr16_2byte(Kr16Opcode::JAL, KR16_RA_REG, 0, 0, 0);
        }
        case MachineInstr::Opcode::Kr16Ret:
            return encode_kr16_2byte(Kr16Opcode::RET, 0, 0, 0, 0);

        default:
            return encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
    }
}

bool Kr16Assembler::assemble(const MachineModule& mm, ObjectFile& obj) {
    obj.target_id = mm.target_id;
    obj.kind = ObjectKind::Object;
    obj.flags = static_cast<std::uint16_t>(ObjectFlags::HasSymbols);
    obj.pointer_width = 16;
    obj.endianness = 0; // little-endian
    obj.target_numeric_id = 2; // KR16

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
    text_sec.alignment = 2; // KR16: 2-byte alignment

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
        rodata_sec.flags = static_cast<std::uint16_t>(SectionFlags::Alloc);
        rodata_sec.alignment = 2;

        std::size_t rodata_cursor = 0;
        for (const auto& mg : mm.globals) {
            if (!mg->is_constant || mg->initializer.empty())
                continue;
            auto it = global_symbol_index.find(mg->name);
            if (it != global_symbol_index.end()) {
                obj.symbols[it->second].value = rodata_cursor;
            }
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
        if (!mg->is_constant) {
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
        data_sec.alignment = 2;

        std::size_t data_cursor = 0;
        for (const auto& mg : mm.globals) {
            if (mg->is_constant)
                continue;
            auto it = global_symbol_index.find(mg->name);
            if (it != global_symbol_index.end()) {
                obj.symbols[it->second].value = data_cursor;
            }
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
