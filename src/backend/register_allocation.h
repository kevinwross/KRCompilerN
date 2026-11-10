#ifndef KRC_REGISTER_ALLOCATION_H
#define KRC_REGISTER_ALLOCATION_H

#include "machine.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace krc {

// Simple graph-coloring register allocator.
// Maps virtual registers to physical registers, spilling to stack slots
// when registers run out.
// For v1, uses a linear-scan-style allocator with a fixed pool of
// allocatable physical registers.
class RegisterAllocator {
public:
    // Construct with the target's allocatable physical registers.
    explicit RegisterAllocator(const std::vector<std::uint32_t>& phys_regs);

    // Run register allocation on a machine function.
    // Returns true on success.
    bool run(MachineFunction& fn);

    // Get the physical register assigned to a virtual register.
    std::uint32_t phys_reg_for(std::uint32_t virt_reg) const;

    // Get the stack slot offset for a spilled virtual register.
    std::int32_t stack_slot_for(std::uint32_t virt_reg) const;

    // Total number of stack slots used for spilling.
    std::int32_t total_stack_slots() const { return next_stack_slot_; }

private:
    std::vector<std::uint32_t> phys_regs_;

    // virt_reg -> phys_reg mapping.
    std::unordered_map<std::uint32_t, std::uint32_t> virt_to_phys_;
    // virt_reg -> stack slot offset (for spilled regs).
    std::unordered_map<std::uint32_t, std::int32_t> virt_to_stack_;
    // phys_reg -> virt_reg currently occupying it (for tracking liveness).
    std::unordered_map<std::uint32_t, std::uint32_t> phys_to_virt_;

    std::int32_t next_stack_slot_ = 0;  // negative offsets from frame pointer
};

} // namespace krc

#endif // KRC_REGISTER_ALLOCATION_H
