#include "machine.h"
#include "targets/kr16/kr16.h"
#include "kr16_assembler.h"
#include "linker.h"
#include "object.h"

#include <doctest.h>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

using namespace krc;

// Build a simple KR16 machine module with one function containing
// a few instructions, used for determinism testing.
static MachineModule make_test_module() {
    MachineModule mm("kr16");

    auto func = std::make_unique<MachineFunction>("main");
    auto bb = std::make_unique<MachineBasicBlock>("entry");

    // Add a few KR16 instructions: add r0, r1, r2; nop; ret
    auto add_inst = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr16Add);
    add_inst->operands.push_back(MachineOperand::RegDef(0));  // rd = r0
    add_inst->operands.push_back(MachineOperand::RegUse(1));  // rs1 = r1
    add_inst->operands.push_back(MachineOperand::RegUse(2));  // rs2 = r2
    bb->instructions.push_back(std::move(add_inst));

    auto nop_inst = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr16Nop);
    bb->instructions.push_back(std::move(nop_inst));

    auto ret_inst = std::make_unique<MachineInstr>(MachineInstr::Opcode::Kr16Ret);
    bb->instructions.push_back(std::move(ret_inst));

    func->blocks.push_back(std::move(bb));
    func->entry_block = func->blocks[0].get();
    mm.functions.push_back(std::move(func));

    return mm;
}

// Assemble a machine module and return the serialized KRO bytes.
static std::vector<std::uint8_t> assemble_module(const MachineModule& mm) {
    const TargetDescriptor& td = kr16_descriptor();
    Kr16Assembler assembler(td);
    ObjectFile obj("kr16");
    obj.target_numeric_id = 2;
    obj.pointer_width = 16;
    obj.endianness = 0;
    obj.kind = ObjectKind::Object;

    bool ok = assembler.assemble(mm, obj);
    REQUIRE(ok);

    std::ostringstream oss;
    bool written = write_kro_object(oss, obj);
    REQUIRE(written);

    std::string str = oss.str();
    return std::vector<std::uint8_t>(str.begin(), str.end());
}

// Assemble a machine module and return the ObjectFile.
static ObjectFile assemble_object(const MachineModule& mm) {
    const TargetDescriptor& td = kr16_descriptor();
    Kr16Assembler assembler(td);
    ObjectFile obj("kr16");
    obj.target_numeric_id = 2;
    obj.pointer_width = 16;
    obj.endianness = 0;
    obj.kind = ObjectKind::Object;

    bool ok = assembler.assemble(mm, obj);
    REQUIRE(ok);

    return obj;
}

// Link an object file and return the serialized KRO output bytes.
static std::vector<std::uint8_t> link_object(const ObjectFile& obj) {
    LinkerOptions opts;
    opts.target_id = "kr16";
    opts.abi_version = 1;
    opts.pointer_width = 16;
    opts.endianness = 0;

    Linker linker(opts);
    bool added = linker.add_object(obj);
    REQUIRE(added);

    bool linked = linker.link();
    REQUIRE(linked);

    std::ostringstream oss;
    bool written = write_kro_object(oss, linker.output());
    REQUIRE(written);

    std::string str = oss.str();
    return std::vector<std::uint8_t>(str.begin(), str.end());
}

TEST_CASE("KR16 assembler determinism: same module produces identical KRO bytes") {
    MachineModule mm1 = make_test_module();
    MachineModule mm2 = make_test_module();

    auto bytes1 = assemble_module(mm1);
    auto bytes2 = assemble_module(mm2);

    REQUIRE(bytes1.size() == bytes2.size());
    CHECK(std::memcmp(bytes1.data(), bytes2.data(), bytes1.size()) == 0);
}

TEST_CASE("KR16 assembler determinism: triple assembly produces identical KRO bytes") {
    MachineModule mm1 = make_test_module();
    MachineModule mm2 = make_test_module();
    MachineModule mm3 = make_test_module();

    auto bytes1 = assemble_module(mm1);
    auto bytes2 = assemble_module(mm2);
    auto bytes3 = assemble_module(mm3);

    REQUIRE(bytes1.size() == bytes2.size());
    REQUIRE(bytes1.size() == bytes3.size());
    CHECK(std::memcmp(bytes1.data(), bytes2.data(), bytes1.size()) == 0);
    CHECK(std::memcmp(bytes1.data(), bytes3.data(), bytes1.size()) == 0);
}

TEST_CASE("KR16 assembler determinism: symbol table is stable across assemblies") {
    MachineModule mm1 = make_test_module();
    MachineModule mm2 = make_test_module();

    const TargetDescriptor& td = kr16_descriptor();
    Kr16Assembler assembler1(td);
    Kr16Assembler assembler2(td);

    ObjectFile obj1("kr16");
    obj1.target_numeric_id = 2;
    obj1.pointer_width = 16;
    obj1.endianness = 0;
    obj1.kind = ObjectKind::Object;
    bool ok1 = assembler1.assemble(mm1, obj1);
    REQUIRE(ok1);

    ObjectFile obj2("kr16");
    obj2.target_numeric_id = 2;
    obj2.pointer_width = 16;
    obj2.endianness = 0;
    obj2.kind = ObjectKind::Object;
    bool ok2 = assembler2.assemble(mm2, obj2);
    REQUIRE(ok2);

    // Same number of sections, symbols, relocations
    CHECK(obj1.sections.size() == obj2.sections.size());
    CHECK(obj1.symbols.size() == obj2.symbols.size());
    CHECK(obj1.relocations.size() == obj2.relocations.size());

    // Same section payloads
    for (size_t i = 0; i < obj1.sections.size(); i++) {
        CHECK(obj1.sections[i].payload == obj2.sections[i].payload);
        CHECK(obj1.sections[i].name == obj2.sections[i].name);
    }
}

TEST_CASE("KR16 linker determinism: same object produces identical linked output") {
    ObjectFile obj1 = assemble_object(make_test_module());
    ObjectFile obj2 = assemble_object(make_test_module());

    auto bytes1 = link_object(obj1);
    auto bytes2 = link_object(obj2);

    REQUIRE(bytes1.size() == bytes2.size());
    CHECK(std::memcmp(bytes1.data(), bytes2.data(), bytes1.size()) == 0);
}

TEST_CASE("KR16 linker determinism: triple link produces identical linked output") {
    ObjectFile obj1 = assemble_object(make_test_module());
    ObjectFile obj2 = assemble_object(make_test_module());
    ObjectFile obj3 = assemble_object(make_test_module());

    auto bytes1 = link_object(obj1);
    auto bytes2 = link_object(obj2);
    auto bytes3 = link_object(obj3);

    REQUIRE(bytes1.size() == bytes2.size());
    REQUIRE(bytes1.size() == bytes3.size());
    CHECK(std::memcmp(bytes1.data(), bytes2.data(), bytes1.size()) == 0);
    CHECK(std::memcmp(bytes1.data(), bytes3.data(), bytes1.size()) == 0);
}

TEST_CASE("KR16 linker output is a valid executable image") {
    ObjectFile obj = assemble_object(make_test_module());

    LinkerOptions opts;
    opts.target_id = "kr16";
    opts.abi_version = 1;
    opts.pointer_width = 16;
    opts.endianness = 0;

    Linker linker(opts);
    bool added = linker.add_object(obj);
    REQUIRE(added);

    bool linked = linker.link();
    REQUIRE(linked);

    const ObjectFile& out = linker.output();
    CHECK(out.kind == ObjectKind::Executable);
    CHECK(out.pointer_width == 16);
    CHECK(out.target_numeric_id == 2);
    CHECK(!out.sections.empty());
    CHECK(out.target_id == "kr16");
}

TEST_CASE("KR16 linker symbol resolution: defined symbols are resolved") {
    ObjectFile obj = assemble_object(make_test_module());

    LinkerOptions opts;
    opts.target_id = "kr16";
    opts.abi_version = 1;
    opts.pointer_width = 16;
    opts.endianness = 0;

    Linker linker(opts);
    bool added = linker.add_object(obj);
    REQUIRE(added);

    bool linked = linker.link();
    REQUIRE(linked);

    const auto& syms = linker.symbols();
    // Should have at least the "main" symbol defined.
    bool found_main = false;
    for (const auto& s : syms) {
        if (s.name == "main") {
            found_main = true;
            CHECK(s.defined);
            break;
        }
    }
    CHECK(found_main);
}
