#include "machine.h"

#include <doctest.h>

#include <cstdint>
#include <sstream>
#include <string>

using namespace krc;

TEST_CASE("MachineOperand: RegUse constructor") {
    MachineOperand op = MachineOperand::RegUse(3);
    CHECK(op.kind == MachineOperand::Kind::Reg);
    CHECK(op.reg == 3);
    CHECK(op.is_use);
    CHECK(!op.is_def);
}

TEST_CASE("MachineOperand: RegDef constructor") {
    MachineOperand op = MachineOperand::RegDef(7);
    CHECK(op.kind == MachineOperand::Kind::Reg);
    CHECK(op.reg == 7);
    CHECK(op.is_def);
    CHECK(!op.is_use);
}

TEST_CASE("MachineOperand: Imm constructor") {
    MachineOperand op = MachineOperand::Imm(-42);
    CHECK(op.kind == MachineOperand::Kind::Imm);
    CHECK(op.imm == -42);
}

TEST_CASE("MachineOperand: StackSlot constructor") {
    MachineOperand op = MachineOperand::StackSlot(-16);
    CHECK(op.kind == MachineOperand::Kind::StackSlot);
    CHECK(op.stack_offset == -16);
}

TEST_CASE("MachineOperand: Global constructor") {
    MachineOperand op = MachineOperand::Global("main");
    CHECK(op.kind == MachineOperand::Kind::Global);
    CHECK(op.name == "main");
}

TEST_CASE("MachineOperand: Label constructor") {
    MachineOperand op = MachineOperand::Label("bb_entry");
    CHECK(op.kind == MachineOperand::Kind::Label);
    CHECK(op.name == "bb_entry");
}

TEST_CASE("MachineOperand: default constructor") {
    MachineOperand op;
    CHECK(op.kind == MachineOperand::Kind::Invalid);
    CHECK(op.reg == 0);
    CHECK(op.imm == 0);
}

TEST_CASE("MachineInstr: creation with opcode") {
    MachineInstr inst(MachineInstr::Opcode::Kr16Add);
    CHECK(inst.opcode == MachineInstr::Opcode::Kr16Add);
    CHECK(inst.operands.empty());
    CHECK(inst.source_line == 0);
}

TEST_CASE("MachineInstr: with operands") {
    MachineInstr inst(MachineInstr::Opcode::Kr16Addi);
    inst.operands.push_back(MachineOperand::RegDef(0));
    inst.operands.push_back(MachineOperand::RegUse(1));
    inst.operands.push_back(MachineOperand::Imm(42));
    CHECK(inst.operands.size() == 3);
    CHECK(inst.operands[0].is_def);
    CHECK(inst.operands[1].is_use);
    CHECK(inst.operands[2].imm == 42);
}

TEST_CASE("MachineInstr: source location") {
    MachineInstr inst(MachineInstr::Opcode::Kr32Load);
    inst.source_file = "test.c";
    inst.source_line = 42;
    CHECK(inst.source_file == "test.c");
    CHECK(inst.source_line == 42);
}

TEST_CASE("MachineBasicBlock: creation") {
    MachineBasicBlock bb("entry");
    CHECK(bb.name == "entry");
    CHECK(bb.instructions.empty());
    CHECK(bb.successors.empty());
}

TEST_CASE("MachineBasicBlock: add instructions") {
    MachineBasicBlock bb("entry");
    bb.instructions.push_back(
        std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr16Nop));
    bb.instructions.push_back(
        std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr16Ret));
    CHECK(bb.instructions.size() == 2);
    CHECK(bb.instructions[0]->opcode == MachineInstr::Opcode::Kr16Nop);
    CHECK(bb.instructions[1]->opcode == MachineInstr::Opcode::Kr16Ret);
}

TEST_CASE("MachineFunction: creation") {
    MachineFunction fn("main");
    CHECK(fn.name == "main");
    CHECK(fn.entry_block == nullptr);
    CHECK(fn.blocks.empty());
    CHECK(fn.frame_size == 0);
}

TEST_CASE("MachineFunction: add blocks") {
    MachineFunction fn("main");
    auto bb1 = std::make_unique<MachineBasicBlock>("entry");
    auto bb2 = std::make_unique<MachineBasicBlock>("exit");
    fn.blocks.push_back(std::move(bb1));
    fn.blocks.push_back(std::move(bb2));
    fn.entry_block = fn.blocks[0].get();
    CHECK(fn.blocks.size() == 2);
    CHECK(fn.entry_block->name == "entry");
}

TEST_CASE("MachineFunction: frame size") {
    MachineFunction fn("main");
    fn.frame_size = 64;
    CHECK(fn.frame_size == 64);
}

TEST_CASE("MachineModule: creation") {
    MachineModule mm("kr16");
    CHECK(mm.target_id == "kr16");
    CHECK(mm.functions.empty());
    CHECK(mm.globals.empty());
}

TEST_CASE("MachineModule: add functions and globals") {
    MachineModule mm("kr32");
    auto fn = std::make_unique<MachineFunction>("main");
    mm.functions.push_back(std::move(fn));
    CHECK(mm.functions.size() == 1);
    CHECK(mm.functions[0]->name == "main");

    auto global = std::make_unique<MachineGlobal>();
    global->name = "x";
    global->size = 4;
    mm.globals.push_back(std::move(global));
    CHECK(mm.globals.size() == 1);
    CHECK(mm.globals[0]->name == "x");
    CHECK(mm.globals[0]->size == 4);
}

TEST_CASE("MachineGlobal: default values") {
    MachineGlobal g;
    g.name = "buf";
    g.size = 1024;
    CHECK(g.alignment == 4);
    CHECK(g.is_constant == false);
    CHECK(g.initializer.empty());
    CHECK(g.symbol_type == 1);
}

TEST_CASE("dump_machine_module: empty module") {
    MachineModule mm("kr16");
    std::ostringstream oss;
    dump_machine_module(mm, oss);
    CHECK(oss.str().empty());
}

TEST_CASE("dump_machine_module: module with function") {
    MachineModule mm("kr32");
    auto fn = std::make_unique<MachineFunction>("main");
    auto bb = std::make_unique<MachineBasicBlock>("entry");
    bb->instructions.push_back(
        std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr32Nop));
    fn->blocks.push_back(std::move(bb));
    fn->entry_block = fn->blocks[0].get();
    mm.functions.push_back(std::move(fn));

    std::ostringstream oss;
    dump_machine_module(mm, oss);
    std::string out = oss.str();
    CHECK(out.find("func @main") != std::string::npos);
    CHECK(out.find("bb entry") != std::string::npos);
    CHECK(out.find("kr32_nop") != std::string::npos);
}

TEST_CASE("dump_machine_module: module with global") {
    MachineModule mm("kr32");
    auto g = std::make_unique<MachineGlobal>();
    g->name = "x";
    g->size = 4;
    mm.globals.push_back(std::move(g));

    std::ostringstream oss;
    dump_machine_module(mm, oss);
    std::string out = oss.str();
    CHECK(out.find("global @x") != std::string::npos);
    CHECK(out.find("size=4") != std::string::npos);
}
