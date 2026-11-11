#include "register_allocation.h"

#include <doctest.h>

#include <cstdint>
#include <vector>

using namespace krc;

TEST_CASE("RegisterAllocator: basic allocation with enough registers") {
    std::vector<std::uint32_t> phys = {0, 1, 2, 3, 4};
    RegisterAllocator ra(phys);

    MachineFunction fn("test");
    auto bb = std::make_unique<MachineBasicBlock>("entry");
    bb->instructions.push_back(
        std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr16Add));
    bb->instructions.back()->operands.push_back(MachineOperand::RegDef(10));
    bb->instructions.back()->operands.push_back(MachineOperand::RegUse(11));
    bb->instructions.back()->operands.push_back(MachineOperand::RegUse(12));
    fn.blocks.push_back(std::move(bb));
    fn.entry_block = fn.blocks[0].get();

    bool ok = ra.run(fn);
    CHECK(ok);
    // With 5 phys regs and 3 virt regs, all should be assigned (not spilled).
    CHECK(ra.stack_slot_for(10) == 0);
    CHECK(ra.stack_slot_for(11) == 0);
    CHECK(ra.stack_slot_for(12) == 0);
}

TEST_CASE("RegisterAllocator: spill when registers exhausted") {
    std::vector<std::uint32_t> phys = {0};  // only 1 physical register
    RegisterAllocator ra(phys);

    MachineFunction fn("test");
    auto bb = std::make_unique<MachineBasicBlock>("entry");

    // Use 5 different virtual registers - only 1 can get a physical reg.
    for (int i = 0; i < 5; ++i) {
        auto inst = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr16Addi);
        inst->operands.push_back(MachineOperand::RegDef(100 + i));
        bb->instructions.push_back(std::move(inst));
    }

    fn.blocks.push_back(std::move(bb));
    fn.entry_block = fn.blocks[0].get();

    bool ok = ra.run(fn);
    CHECK(ok);

    // At least one should be spilled.
    int spilled = 0;
    for (int i = 0; i < 5; ++i) {
        if (ra.stack_slot_for(100 + i) != 0) ++spilled;
    }
    CHECK(spilled > 0);
}

TEST_CASE("RegisterAllocator: phys_reg_for returns 0 for unassigned") {
    RegisterAllocator ra({0, 1});
    CHECK(ra.phys_reg_for(999) == 0);
}

TEST_CASE("RegisterAllocator: stack_slot_for returns 0 for unassigned") {
    RegisterAllocator ra({0, 1});
    CHECK(ra.stack_slot_for(999) == 0);
}

TEST_CASE("RegisterAllocator: total_stack_slots after no spill") {
    std::vector<std::uint32_t> phys = {0};
    RegisterAllocator ra(phys);

    MachineFunction fn("test");
    auto bb = std::make_unique<MachineBasicBlock>("entry");

    auto inst = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr16Addi);
    inst->operands.push_back(MachineOperand::RegDef(42));
    bb->instructions.push_back(std::move(inst));
    fn.blocks.push_back(std::move(bb));
    fn.entry_block = fn.blocks[0].get();

    bool ok = ra.run(fn);
    CHECK(ok);

    // With 1 phys reg and 1 virt reg, it should get a phys reg, not spill.
    // So total_stack_slots should be 0.
    CHECK(ra.total_stack_slots() == 0);
}

TEST_CASE("RegisterAllocator: multiple functions independent") {
    std::vector<std::uint32_t> phys = {0, 1};
    RegisterAllocator ra(phys);

    MachineFunction fn1("fn1");
    auto bb1 = std::make_unique<MachineBasicBlock>("entry");
    bb1->instructions.push_back(
        std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr16Add));
    bb1->instructions.back()->operands.push_back(MachineOperand::RegDef(10));
    bb1->instructions.back()->operands.push_back(MachineOperand::RegUse(11));
    fn1.blocks.push_back(std::move(bb1));
    fn1.entry_block = fn1.blocks[0].get();

    MachineFunction fn2("fn2");
    auto bb2 = std::make_unique<MachineBasicBlock>("entry");
    bb2->instructions.push_back(
        std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr16Add));
    bb2->instructions.back()->operands.push_back(MachineOperand::RegDef(20));
    bb2->instructions.back()->operands.push_back(MachineOperand::RegUse(21));
    fn2.blocks.push_back(std::move(bb2));
    fn2.entry_block = fn2.blocks[0].get();

    bool ok1 = ra.run(fn1);
    bool ok2 = ra.run(fn2);
    CHECK(ok1);
    CHECK(ok2);

    // Both functions should have their registers assigned (not spilled).
    CHECK(ra.stack_slot_for(10) == 0);
    CHECK(ra.stack_slot_for(20) == 0);
}
