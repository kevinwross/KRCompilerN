#include "targets/kr16/kr16.h"

#include <doctest.h>

#include <cstdint>

using namespace krc;

TEST_CASE("KR16 instruction encoding: R-type ADD") {
    std::uint16_t inst = encode_kr16_2byte(Kr16Opcode::ADD, 1, 2, 3, 0);
    std::uint16_t expected = static_cast<std::uint16_t>(
        (0x1u << 12) | (1u << 10) | (2u << 8) | (3u << 6));
    CHECK(inst == expected);
}

TEST_CASE("KR16 instruction encoding: I-type ADDI") {
    std::uint16_t inst = encode_kr16_2byte(Kr16Opcode::ADDI, 2, 1, 0, 42);
    std::uint16_t expected = static_cast<std::uint16_t>(
        (0x10u << 12) | (2u << 10) | (1u << 8) | (0u << 6) | 42);
    CHECK(inst == expected);
}

TEST_CASE("KR16 instruction encoding: J-type JAL") {
    std::uint16_t inst = encode_kr16_2byte(Kr16Opcode::JAL, 1, 0, 0, 5);
    std::uint16_t expected = static_cast<std::uint16_t>(
        (0x51u << 12) | (1u << 10) | 5);
    CHECK(inst == expected);
}

TEST_CASE("KR16 instruction encoding: RET") {
    std::uint16_t inst = encode_kr16_2byte(Kr16Opcode::RET, 0, 0, 0, 0);
    std::uint16_t expected = static_cast<std::uint16_t>(0x60u << 12);
    CHECK(inst == expected);
}

TEST_CASE("KR16 instruction encoding: NOP") {
    std::uint16_t inst = encode_kr16_2byte(Kr16Opcode::NOP, 0, 0, 0, 0);
    std::uint16_t expected = static_cast<std::uint16_t>(0x7Fu << 12);
    CHECK(inst == expected);
}

TEST_CASE("KR16 instruction encoding: store SW") {
    std::uint16_t inst = encode_kr16_2byte(Kr16Opcode::SW, 0, 2, 1, 16);
    std::uint16_t expected = static_cast<std::uint16_t>(
        (0x32u << 12) | (0u << 10) | (2u << 8) | (1u << 6) | 16);
    CHECK(inst == expected);
}

TEST_CASE("KR16 instruction encoding: branch BEQ") {
    std::uint16_t inst = encode_kr16_2byte(Kr16Opcode::BEQ, 0, 3, 0, -1);
    std::uint16_t expected = static_cast<std::uint16_t>(
        (0x40u << 12) | (3u << 8) | 0xFFu);
    CHECK(inst == expected);
}

TEST_CASE("KR16 imm8_fits: boundary values") {
    CHECK(imm8_fits(-128));
    CHECK(imm8_fits(127));
    CHECK(!imm8_fits(-129));
    CHECK(!imm8_fits(128));
}

TEST_CASE("KR16 imm8_fits: zero and extremes") {
    CHECK(imm8_fits(0));
    CHECK(imm8_fits(1));
    CHECK(imm8_fits(-1));
}

TEST_CASE("KR16 register helpers") {
    CHECK(kr16_reg_name(0) == "r0");
    CHECK(kr16_reg_name(1) == "r1");
    CHECK(kr16_reg_name(4) == "r4");
    CHECK(kr16_reg_name(5) == "r5");
}

TEST_CASE("KR16 register classification") {
    // Caller-saved: R0-R3.
    CHECK(kr16_is_caller_saved(0));
    CHECK(kr16_is_caller_saved(1));
    CHECK(kr16_is_caller_saved(2));
    CHECK(kr16_is_caller_saved(3));
    CHECK(!kr16_is_caller_saved(4));

    // Callee-saved: R4.
    CHECK(!kr16_is_callee_saved(0));
    CHECK(!kr16_is_callee_saved(1));
    CHECK(!kr16_is_callee_saved(2));
    CHECK(!kr16_is_callee_saved(3));
    CHECK(kr16_is_callee_saved(4));

    // No reserved registers.
    CHECK(!kr16_is_reserved(0));
    CHECK(!kr16_is_reserved(4));
}

TEST_CASE("KR16 allocatable registers") {
    CHECK(kr16_num_allocatable_gprs() == 5);

    auto regs = kr16_allocatable_gprs();
    CHECK(regs.size() == 5);
    CHECK(regs[0] == 0);
    CHECK(regs[1] == 1);
    CHECK(regs[2] == 2);
    CHECK(regs[3] == 3);
    CHECK(regs[4] == 4);
}
