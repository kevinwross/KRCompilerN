#ifndef KRC_FRAME_LAYOUT_H
#define KRC_FRAME_LAYOUT_H

#include "machine.h"

#include <cstdint>

namespace krc {

// Frame layout: computes stack frame size, assigns stack slots to spilled
// registers and local variables, and rewrites stack slot operands into
// frame-pointer-relative offsets.
class FrameLayout {
public:
    // Construct with the target descriptor.
    explicit FrameLayout(const TargetDescriptor& target);

    // Run frame layout on a machine function.
    // Sets fn->frame_size and rewrites StackSlot operands.
    bool run(MachineFunction& fn);

    // Get the frame size for a function.
    std::int32_t frame_size(const MachineFunction& fn) const;

private:
    const TargetDescriptor& target_;
};

} // namespace krc

#endif // KRC_FRAME_LAYOUT_H
