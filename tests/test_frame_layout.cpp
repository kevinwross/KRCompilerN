#include "frame_layout.h"
#include "target_descriptor.h"

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace krc;

TEST_CASE("FrameLayout: empty function has zero frame size") {
    const TargetDescriptor& td = kr32_descriptor();
    FrameLayout fl(td);

    MachineFunction fn("main");
    auto bb = std::make_unique<MachineBasicBlock>("entry");
    bb->instructions.push_back(
        std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Nop));
    fn.blocks.push_back(std::move(bb));
    fn.entry_block = fn.blocks[0].get();

    bool ok = fl.run(fn);
    CHECK(ok);
    CHECK(fn.frame_size == 0);
}

TEST_CASE("FrameLayout: stack slots compute frame size") {
    const TargetDescriptor& td = kr32_descriptor();
    FrameLayout fl(td);

    MachineFunction fn("main");
    auto bb = std::make_unique<MachineBasicBlock>("entry");

    // Stack slots are negative offsets from frame pointer.
    auto inst1 = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Load);
    inst1->operands.push_back(MachineOperand::RegDef(0));
    inst1->operands.push_back(MachineOperand::StackSlot(-4));
    bb->instructions.push_back(std::move(inst1));

    auto inst2 = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Store);
    inst2->operands.push_back(MachineOperand::RegUse(0));
    inst2->operands.push_back(MachineOperand::StackSlot(-16));
    bb->instructions.push_back(std::move(inst2));

    fn.blocks.push_back(std::move(bb));
    fn.entry_block = fn.blocks[0].get();

    bool ok = fl.run(fn);
    CHECK(ok);

    // min_offset = -16, max_offset = 0, frame_size = 16
    CHECK(fn.frame_size == 16);
}

TEST_CASE("FrameLayout: aligns frame size to pointer alignment") {
    const TargetDescriptor& td = kr32_descriptor();
    FrameLayout fl(td);

    MachineFunction fn("main");
    auto bb = std::make_unique<MachineBasicBlock>("entry");

    // Stack slot at -13 (not aligned to 4).
    auto inst = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Load);
    inst->operands.push_back(MachineOperand::RegDef(0));
    inst->operands.push_back(MachineOperand::StackSlot(-13));
    bb->instructions.push_back(std::move(inst));

    fn.blocks.push_back(std::move(bb));
    fn.entry_block = fn.blocks[0].get();

    bool ok = fl.run(fn);
    CHECK(ok);

    // min_offset = -13, frame_size = 13, aligned to 4 → 16
    CHECK(fn.frame_size == 16);
}

TEST_CASE("FrameLayout: no stack slots means zero frame size") {
    const TargetDescriptor& td = kr32_descriptor();
    FrameLayout fl(td);

    MachineFunction fn("main");
    auto bb = std::make_unique<MachineBasicBlock>("entry");

    // Only register operands, no stack slots.
    auto inst = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Add);
    inst->operands.push_back(MachineOperand::RegDef(0));
    inst->operands.push_back(MachineOperand::RegUse(1));
    inst->operands.push_back(MachineOperand::RegUse(2));
    bb->instructions.push_back(std::move(inst));

    fn.blocks.push_back(std::move(bb));
    fn.entry_block = fn.blocks[0].get();

    bool ok = fl.run(fn);
    CHECK(ok);
    CHECK(fn.frame_size == 0);
}

TEST_CASE("FrameLayout: positive stack offsets don't increase size") {
    const TargetDescriptor& td = kr32_descriptor();
    FrameLayout fl(td);

    MachineFunction fn("main");
    auto bb = std::make_unique<MachineBasicBlock>("entry");

    // Only positive stack offsets.
    auto inst = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Load);
    inst->operands.push_back(MachineOperand::RegDef(0));
    inst->operands.push_back(MachineOperand::StackSlot(8));
    bb->instructions.push_back(std::move(inst));

    fn.blocks.push_back(std::move(bb));
    fn.entry_block = fn.blocks[0].get();

    bool ok = fl.run(fn);
    CHECK(ok);

    // min_offset = 0, frame_size = 0
    CHECK(fn.frame_size == 0);
}
