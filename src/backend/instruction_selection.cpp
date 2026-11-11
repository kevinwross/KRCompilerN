#include "instruction_selection.h"

#include "diagnostics.h"

#include <cassert>
#include <unordered_map>

namespace krc {

InstructionSelection::InstructionSelection(IRModule& ir_module)
    : ir_module_(ir_module), mm_(ir_module.target_id),
      is_kr16_(ir_module.target_id == "kr16") {}

MachineModule InstructionSelection::run() {
    for (auto& fn : ir_module_.functions) {
        lower_function(*fn);
    }
    return std::move(mm_);
}

void InstructionSelection::lower_function(IRFunction& fn) {
    auto mfn = std::make_unique<MachineFunction>(fn.name);

    // Create machine basic blocks for each IR basic block.
    for (auto& bb : fn.basic_blocks) {
        auto mbb = std::make_unique<MachineBasicBlock>(bb->name);
        if (!mfn->entry_block) {
            mfn->entry_block = mbb.get();
        }
        mfn->blocks.push_back(std::move(mbb));
    }

    // Map from IR value name → virtual register number.
    std::unordered_map<std::string, std::uint32_t> value_to_vreg;
    std::uint32_t next_vreg = 0;

    // Assign virtual registers to function arguments.
    for (std::size_t i = 0; i < fn.param_types.size(); ++i) {
        std::string arg_name = (i < fn.param_names.size())
            ? fn.param_names[i] : ("arg" + std::to_string(i));
        value_to_vreg[arg_name] = next_vreg++;
    }

    // Lower each basic block.
    for (auto& bb : fn.basic_blocks) {
        // Find the corresponding machine basic block.
        MachineBasicBlock* mbb = nullptr;
        for (auto& mb : mfn->blocks) {
            if (mb->name == bb->name) {
                mbb = mb.get();
                break;
            }
        }
        if (!mbb) continue;

        // Lower each instruction.
        for (auto& inst : bb->instructions) {
            lower_instruction(inst.get(), mbb, value_to_vreg, next_vreg);
        }

        // Lower the terminator.
        if (bb->terminator) {
            lower_instruction(bb->terminator.get(), mbb, value_to_vreg,
                              next_vreg);
        }
    }

    mm_.functions.push_back(std::move(mfn));
}

void InstructionSelection::lower_basic_block(IRBasicBlock* /*bb*/,
                                              MachineBasicBlock* /*mbb*/) {
    // Handled inline in lower_function.
}

static std::uint32_t vreg_for(std::unordered_map<std::string, std::uint32_t>& map,
                              std::uint32_t& next_vreg,
                              const std::string& name) {
    auto it = map.find(name);
    if (it != map.end()) return it->second;
    map[name] = next_vreg;
    return next_vreg++;
}

// Target-aware opcode selection: pick KR32 or KR16 variant based on target.
static MachineInstr::Opcode pick_kr_opcode(bool is_kr16,
                                           MachineInstr::Opcode kr32_op,
                                           MachineInstr::Opcode kr16_op) {
    return is_kr16 ? kr16_op : kr32_op;
}

void InstructionSelection::lower_instruction(IRInstruction* inst,
                                              MachineBasicBlock* mbb,
                                              std::unordered_map<std::string,
                                                                 std::uint32_t>&
                                                  value_to_vreg,
                                              std::uint32_t& next_vreg) {
    auto mi = std::make_unique<MachineInstr>();

    auto add_use = [&](IRValue* val) {
        if (val) mi->operands.push_back(
            MachineOperand::RegUse(vreg_for(value_to_vreg, next_vreg,
                                            val->name)));
    };
    auto add_def = [&]() {
        if (!inst->name.empty()) {
            mi->operands.push_back(MachineOperand::RegDef(
                vreg_for(value_to_vreg, next_vreg, inst->name)));
        }
    };

    switch (inst->opcode) {
        case IROpcode::Alloca:
            mi->opcode = MachineInstr::Opcode::PseudoAlloca;
            add_def();
            break;

        case IROpcode::Load:
            mi->opcode = MachineInstr::Opcode::PseudoLoad;
            if (!inst->operands.empty()) add_use(inst->operands[0]);
            add_def();
            break;

        case IROpcode::Store:
            mi->opcode = MachineInstr::Opcode::PseudoStore;
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);  // value to store
                add_use(inst->operands[1]);  // pointer
            }
            break;

        case IROpcode::IAdd:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Add,
                                        MachineInstr::Opcode::Kr16Add);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::ISub:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Sub,
                                        MachineInstr::Opcode::Kr16Sub);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::IMul:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Mul,
                                        MachineInstr::Opcode::Kr16Mul);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::IDiv:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Div,
                                        MachineInstr::Opcode::Kr16Div);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::IRem:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Rem,
                                        MachineInstr::Opcode::Kr16Rem);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::IAnd:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32And,
                                        MachineInstr::Opcode::Kr16And);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::IOr:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Or,
                                        MachineInstr::Opcode::Kr16Or);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::IXor:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Xor,
                                        MachineInstr::Opcode::Kr16Xor);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::IShl:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Shl,
                                        MachineInstr::Opcode::Kr16Shl);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::IShr:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Shr,
                                        MachineInstr::Opcode::Kr16Shr);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::ISra:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Sra,
                                        MachineInstr::Opcode::Kr16Shr);
            if (inst->operands.size() >= 2) {
                add_use(inst->operands[0]);
                add_use(inst->operands[1]);
            }
            add_def();
            break;

        case IROpcode::Return:
            mi->opcode = MachineInstr::Opcode::PseudoRet;
            if (!inst->operands.empty()) add_use(inst->operands[0]);
            break;

        case IROpcode::Br:
            mi->opcode = MachineInstr::Opcode::PseudoBr;
            if (!inst->operands.empty()) {
                mi->operands.push_back(MachineOperand::Label(
                    inst->operands[0]->name));
            }
            break;

        case IROpcode::CondBr:
            mi->opcode = MachineInstr::Opcode::PseudoCondBr;
            if (inst->operands.size() >= 3) {
                add_use(inst->operands[0]);
                mi->operands.push_back(MachineOperand::Label(
                    inst->operands[1]->name));
                mi->operands.push_back(MachineOperand::Label(
                    inst->operands[2]->name));
            }
            break;

        case IROpcode::Call:
            mi->opcode = MachineInstr::Opcode::PseudoCall;
            if (!inst->operands.empty()) {
                mi->operands.push_back(MachineOperand::Global(
                    inst->operands[0]->name));
            }
            add_def();
            break;

        case IROpcode::Unreachable:
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Nop,
                                        MachineInstr::Opcode::Kr16Nop);
            break;

        default:
            // Unimplemented opcodes become target-specific NOPs for now.
            mi->opcode = pick_kr_opcode(is_kr16_,
                                        MachineInstr::Opcode::Kr32Nop,
                                        MachineInstr::Opcode::Kr16Nop);
            break;
    }

    mbb->instructions.push_back(std::move(mi));
}

} // namespace krc
