#include "register_allocation.h"

#include <algorithm>

namespace krc {

RegisterAllocator::RegisterAllocator(
    const std::vector<std::uint32_t>& phys_regs)
    : phys_regs_(phys_regs) {}

bool RegisterAllocator::run(MachineFunction& fn) {
    // Simple linear-scan register allocation.
    // For each instruction, assign physical registers to virtual registers.
    // When all physical registers are in use, spill to a stack slot.

    // First pass: collect all virtual registers used in the function.
    std::unordered_set<std::uint32_t> virt_regs;
    for (auto& bb : fn.blocks) {
        for (auto& inst : bb->instructions) {
            for (const auto& op : inst->operands) {
                if (op.kind == MachineOperand::Kind::Reg) {
                    virt_regs.insert(op.reg);
                }
            }
        }
    }

    // Assign physical registers to virtual registers.
    // For v1: assign one physical register per virtual register, round-robin.
    // Spill when we run out.
    std::uint32_t phys_idx = 0;
    for (std::uint32_t vr : virt_regs) {
        if (phys_idx < phys_regs_.size()) {
            virt_to_phys_[vr] = phys_regs_[phys_idx];
            phys_to_virt_[phys_regs_[phys_idx]] = vr;
            ++phys_idx;
        } else {
            // Spill to stack slot.
            virt_to_stack_[vr] = next_stack_slot_;
            next_stack_slot_ -= 4;  // 4 bytes per slot
        }
    }

    // Second pass: rewrite instructions to use physical registers.
    for (auto& bb : fn.blocks) {
        for (auto& inst : bb->instructions) {
            for (auto& op : inst->operands) {
                if (op.kind != MachineOperand::Kind::Reg) continue;

                auto it = virt_to_phys_.find(op.reg);
                if (it != virt_to_phys_.end()) {
                    op.reg = it->second;
                } else {
                    auto sit = virt_to_stack_.find(op.reg);
                    if (sit != virt_to_stack_.end()) {
                        // Spilled: replace reg operand with stack slot.
                        op.kind = MachineOperand::Kind::StackSlot;
                        op.stack_offset = sit->second;
                    }
                }
            }
        }
    }

    return true;
}

std::uint32_t RegisterAllocator::phys_reg_for(std::uint32_t virt_reg) const {
    auto it = virt_to_phys_.find(virt_reg);
    if (it != virt_to_phys_.end()) return it->second;
    return 0;  // not assigned
}

std::int32_t RegisterAllocator::stack_slot_for(std::uint32_t virt_reg) const {
    auto it = virt_to_stack_.find(virt_reg);
    if (it != virt_to_stack_.end()) return it->second;
    return 0;
}

} // namespace krc
