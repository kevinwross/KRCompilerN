#include "machine.h"

#include <ostream>
#include <string>

namespace krc {

void dump_machine_module(const MachineModule& mm, std::ostream& out) {
    for (const auto& mg : mm.globals) {
        out << "global @" << mg->name << " size=" << mg->size
            << " align=" << mg->alignment
            << " const=" << (mg->is_constant ? "yes" : "no")
            << " init=" << mg->initializer.size() << " bytes\n";
    }
    for (const auto& fn : mm.functions) {
        out << "func @" << fn->name << "\n";
        for (const auto& bb : fn->blocks) {
            out << "  bb " << bb->name << ":\n";
            for (const auto& inst : bb->instructions) {
                out << "    ";
                switch (inst->opcode) {
                    case MachineInstr::Opcode::PseudoAlloca:
                        out << "pseudo_alloca";
                        break;
                    case MachineInstr::Opcode::PseudoLoad:
                        out << "pseudo_load";
                        break;
                    case MachineInstr::Opcode::PseudoStore:
                        out << "pseudo_store";
                        break;
                    case MachineInstr::Opcode::PseudoRet:
                        out << "pseudo_ret";
                        break;
                    case MachineInstr::Opcode::PseudoBr:
                        out << "pseudo_br";
                        break;
                    case MachineInstr::Opcode::PseudoCondBr:
                        out << "pseudo_cond_br";
                        break;
                    case MachineInstr::Opcode::PseudoCall:
                        out << "pseudo_call";
                        break;
                    case MachineInstr::Opcode::PseudoPhi:
                        out << "pseudo_phi";
                        break;
                    case MachineInstr::Opcode::Kr32Add:
                        out << "kr32_add";
                        break;
                    case MachineInstr::Opcode::Kr32Sub:
                        out << "kr32_sub";
                        break;
                    case MachineInstr::Opcode::Kr32Mul:
                        out << "kr32_mul";
                        break;
                    case MachineInstr::Opcode::Kr32Div:
                        out << "kr32_div";
                        break;
                    case MachineInstr::Opcode::Kr32Rem:
                        out << "kr32_rem";
                        break;
                    case MachineInstr::Opcode::Kr32And:
                        out << "kr32_and";
                        break;
                    case MachineInstr::Opcode::Kr32Or:
                        out << "kr32_or";
                        break;
                    case MachineInstr::Opcode::Kr32Xor:
                        out << "kr32_xor";
                        break;
                    case MachineInstr::Opcode::Kr32Shl:
                        out << "kr32_shl";
                        break;
                    case MachineInstr::Opcode::Kr32Shr:
                        out << "kr32_shr";
                        break;
                    case MachineInstr::Opcode::Kr32Sra:
                        out << "kr32_sra";
                        break;
                    case MachineInstr::Opcode::Kr32Load:
                        out << "kr32_load";
                        break;
                    case MachineInstr::Opcode::Kr32Store:
                        out << "kr32_store";
                        break;
                    case MachineInstr::Opcode::Kr32Addi:
                        out << "kr32_addi";
                        break;
                    case MachineInstr::Opcode::Kr32Lw:
                        out << "kr32_lw";
                        break;
                    case MachineInstr::Opcode::Kr32Sw:
                        out << "kr32_sw";
                        break;
                    case MachineInstr::Opcode::Kr32Li:
                        out << "kr32_li";
                        break;
                    case MachineInstr::Opcode::Kr32Beq:
                        out << "kr32_beq";
                        break;
                    case MachineInstr::Opcode::Kr32Bne:
                        out << "kr32_bne";
                        break;
                    case MachineInstr::Opcode::Kr32Blt:
                        out << "kr32_blt";
                        break;
                    case MachineInstr::Opcode::Kr32Bgt:
                        out << "kr32_bgt";
                        break;
                    case MachineInstr::Opcode::Kr32Ble:
                        out << "kr32_ble";
                        break;
                    case MachineInstr::Opcode::Kr32Bge:
                        out << "kr32_bge";
                        break;
                    case MachineInstr::Opcode::Kr32J:
                        out << "kr32_j";
                        break;
                    case MachineInstr::Opcode::Kr32Jal:
                        out << "kr32_jal";
                        break;
                    case MachineInstr::Opcode::Kr32Ret:
                        out << "kr32_ret";
                        break;
                    case MachineInstr::Opcode::Kr32Nop:
                        out << "kr32_nop";
                        break;
                    default:
                        out << "invalid";
                        break;
                }
                for (const auto& op : inst->operands) {
                    out << " ";
                    switch (op.kind) {
                        case MachineOperand::Kind::Reg:
                            out << "r" << op.reg
                                << (op.is_def ? "[def]" : op.is_use ? "[use]" : "");
                            break;
                        case MachineOperand::Kind::Imm:
                            out << "#" << op.imm;
                            break;
                        case MachineOperand::Kind::StackSlot:
                            out << "sp" << op.stack_offset;
                            break;
                        case MachineOperand::Kind::Global:
                            out << "@" << op.name;
                            break;
                        case MachineOperand::Kind::Label:
                            out << "bb" << op.name;
                            break;
                        default:
                            out << "invalid";
                            break;
                    }
                }
                out << "\n";
            }
        }
    }
}

} // namespace krc
