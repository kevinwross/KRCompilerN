#include "frame_layout.h"

#include <algorithm>

namespace krc {

FrameLayout::FrameLayout(const TargetDescriptor& target) : target_(target) {}

bool FrameLayout::run(MachineFunction& fn) {
    // Collect all stack slot references used in the function.
    std::int32_t min_offset = 0;
    std::int32_t max_offset = 0;

    for (auto& bb : fn.blocks) {
        for (auto& inst : bb->instructions) {
            for (const auto& op : inst->operands) {
                if (op.kind == MachineOperand::Kind::StackSlot) {
                    min_offset = std::min(min_offset, op.stack_offset);
                    max_offset = std::max(max_offset, op.stack_offset);
                }
            }
        }
    }

    // Frame size = range of stack slots + alignment padding.
    // Stack slots are negative offsets from the frame pointer.
    std::int32_t frame_size = 0;
    if (min_offset < 0) {
        frame_size = -min_offset;  // size in bytes
    }

    // Align frame size to the target's pointer alignment.
    std::uint32_t ptr_align = target_.data_model.pointer_align;
    if (ptr_align > 0) {
        frame_size = (frame_size + ptr_align - 1) / ptr_align * ptr_align;
    }

    fn.frame_size = frame_size;
    return true;
}

std::int32_t FrameLayout::frame_size(const MachineFunction& /*fn*/) const {
    return 0;
}

} // namespace krc
