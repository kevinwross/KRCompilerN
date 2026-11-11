#include "debug_info.h"
#include "machine.h"

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace krc;

TEST_CASE("MachineDebugInfoCollector: empty module") {
    MachineModule mm("kr32");
    MachineDebugInfoCollector collector(mm);
    bool ok = collector.collect();
    CHECK(ok);

    const auto& info = collector.info();
    CHECK(info.functions.empty());
    CHECK(info.variables.empty());
}

TEST_CASE("MachineDebugInfoCollector: function info") {
    MachineModule mm("kr32");
    auto fn = std::make_unique<MachineFunction>("main");
    fn->debug_start_address = 0x1000;
    fn->debug_size = 64;
    fn->debug_saved_fp_offset = 0;
    fn->debug_saved_ra_offset = 4;
    fn->debug_cfa_rule = 1;

    auto bb = std::make_unique<MachineBasicBlock>("entry");
    bb->instructions.push_back(
        std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Nop));
    fn->blocks.push_back(std::move(bb));
    fn->entry_block = fn->blocks[0].get();

    mm.functions.push_back(std::move(fn));

    MachineDebugInfoCollector collector(mm);
    bool ok = collector.collect();
    CHECK(ok);

    const auto& info = collector.info();
    CHECK(info.functions.size() == 1);
    CHECK(info.functions[0].name == "main");
    CHECK(info.functions[0].start_address == 0x1000);
    CHECK(info.functions[0].size == 64);
    CHECK(info.functions[0].saved_fp_offset == 0);
    CHECK(info.functions[0].saved_ra_offset == 4);
    CHECK(info.functions[0].cfa_rule == 1);
}

TEST_CASE("MachineDebugInfoCollector: variable from stack slot") {
    MachineModule mm("kr32");
    auto fn = std::make_unique<MachineFunction>("main");

    auto bb = std::make_unique<MachineBasicBlock>("entry");
    auto inst = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Load);
    inst->operands.push_back(MachineOperand::RegDef(0));
    inst->operands.push_back(MachineOperand::StackSlot(-8));
    bb->instructions.push_back(std::move(inst));

    fn->blocks.push_back(std::move(bb));
    fn->entry_block = fn->blocks[0].get();

    mm.functions.push_back(std::move(fn));

    MachineDebugInfoCollector collector(mm);
    bool ok = collector.collect();
    CHECK(ok);

    const auto& info = collector.info();
    CHECK(info.variables.size() == 1);
    CHECK(info.variables[0].name == "local_-8");
    CHECK(info.variables[0].stack_offset == -8);
    CHECK(info.variables[0].location_kind == 0);
}

TEST_CASE("MachineDebugInfoCollector: no duplicate variables") {
    MachineModule mm("kr32");
    auto fn = std::make_unique<MachineFunction>("main");

    auto bb = std::make_unique<MachineBasicBlock>("entry");

    // Two instructions referencing the same stack slot.
    auto inst1 = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Load);
    inst1->operands.push_back(MachineOperand::RegDef(0));
    inst1->operands.push_back(MachineOperand::StackSlot(-16));
    bb->instructions.push_back(std::move(inst1));

    auto inst2 = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Store);
    inst2->operands.push_back(MachineOperand::RegUse(0));
    inst2->operands.push_back(MachineOperand::StackSlot(-16));
    bb->instructions.push_back(std::move(inst2));

    fn->blocks.push_back(std::move(bb));
    fn->entry_block = fn->blocks[0].get();

    mm.functions.push_back(std::move(fn));

    MachineDebugInfoCollector collector(mm);
    bool ok = collector.collect();
    CHECK(ok);

    const auto& info = collector.info();
    CHECK(info.variables.size() == 1);
}

TEST_CASE("MachineDebugInfoCollector: multiple functions") {
    MachineModule mm("kr32");

    auto fn1 = std::make_unique<MachineFunction>("foo");
    auto bb1 = std::make_unique<MachineBasicBlock>("entry");
    bb1->instructions.push_back(
        std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Nop));
    fn1->blocks.push_back(std::move(bb1));
    fn1->entry_block = fn1->blocks[0].get();
    mm.functions.push_back(std::move(fn1));

    auto fn2 = std::make_unique<MachineFunction>("bar");
    auto bb2 = std::make_unique<MachineBasicBlock>("entry");
    bb2->instructions.push_back(
        std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Nop));
    fn2->blocks.push_back(std::move(bb2));
    fn2->entry_block = fn2->blocks[0].get();
    mm.functions.push_back(std::move(fn2));

    MachineDebugInfoCollector collector(mm);
    bool ok = collector.collect();
    CHECK(ok);

    const auto& info = collector.info();
    CHECK(info.functions.size() == 2);
    CHECK(info.functions[0].name == "foo");
    CHECK(info.functions[1].name == "bar");
}

TEST_CASE("MachineDebugInfoCollector: multiple variables") {
    MachineModule mm("kr32");
    auto fn = std::make_unique<MachineFunction>("main");

    auto bb = std::make_unique<MachineBasicBlock>("entry");

    auto inst1 = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Load);
    inst1->operands.push_back(MachineOperand::RegDef(0));
    inst1->operands.push_back(MachineOperand::StackSlot(-4));
    bb->instructions.push_back(std::move(inst1));

    auto inst2 = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Load);
    inst2->operands.push_back(MachineOperand::RegDef(1));
    inst2->operands.push_back(MachineOperand::StackSlot(-8));
    bb->instructions.push_back(std::move(inst2));

    auto inst3 = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Load);
    inst3->operands.push_back(MachineOperand::RegDef(2));
    inst3->operands.push_back(MachineOperand::StackSlot(-12));
    bb->instructions.push_back(std::move(inst3));

    fn->blocks.push_back(std::move(bb));
    fn->entry_block = fn->blocks[0].get();

    mm.functions.push_back(std::move(fn));

    MachineDebugInfoCollector collector(mm);
    bool ok = collector.collect();
    CHECK(ok);

    const auto& info = collector.info();
    CHECK(info.variables.size() == 3);
}
