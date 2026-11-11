#include "target_descriptor.h"

#include <doctest.h>

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

using namespace krc;

TEST_CASE("KR32 target descriptor identity") {
    const TargetDescriptor& d = kr32_descriptor();

    CHECK(d.identity.family == TargetFamily::Kr32);
    CHECK(d.identity.id == "kr32");
    CHECK(d.identity.name == "KR32");
    CHECK(d.identity.version == 1);
    CHECK(d.identity.register_family == RegisterFamily::Register);
    CHECK(d.identity.pointer_width == 32);
    CHECK(d.identity.endianness == Endianness::Little);
    CHECK(d.identity.max_alignment == 16);
    CHECK(d.identity.address_space_count == 1);
}

TEST_CASE("KR32 target descriptor data model") {
    const TargetDescriptor& d = kr32_descriptor();

    // ILP32: int, long are 32-bit; pointer is 32-bit.
    CHECK(d.data_model.char_size == 8);
    CHECK(d.data_model.short_size == 16);
    CHECK(d.data_model.int_size == 32);
    CHECK(d.data_model.long_size == 32);
    CHECK(d.data_model.long_long_size == 64);
    CHECK(d.data_model.pointer_size == 32);
    CHECK(d.data_model.float_size == 32);
    CHECK(d.data_model.double_size == 64);
    CHECK(d.data_model.size_t_size == 32);
    CHECK(d.data_model.ptrdiff_t_size == 32);
    CHECK(d.data_model.intmax_t_size == 64);
    CHECK(d.data_model.char_is_unsigned == false);
}

TEST_CASE("KR32 target descriptor registers") {
    const TargetDescriptor& d = kr32_descriptor();

    CHECK(d.registers.gpr_count == 16);
    CHECK(d.registers.fpr_count == 16);
    CHECK(d.registers.fp_register == 7);
    CHECK(d.registers.tp_register == 11);
    CHECK(d.registers.sp_register == -1);
    CHECK(d.registers.pc_register == -1);

    // Caller-saved: R0-R3 (bits 0-3).
    CHECK((d.registers.gpr_caller_saved & 0x000FuLL) != 0);
    // Callee-saved: R7 (bit 7).
    CHECK((d.registers.gpr_callee_saved & 0x0080uLL) != 0);
    // Reserved: R11 (bit 11).
    CHECK((d.registers.gpr_reserved & 0x0800uLL) != 0);
}

TEST_CASE("KR32 target descriptor capabilities") {
    const TargetDescriptor& d = kr32_descriptor();

    CHECK(d.capabilities.has_fpu == true);
    CHECK(d.capabilities.has_matrix == true);
    CHECK(d.capabilities.has_atomics == true);
    CHECK(d.capabilities.has_guest_threads == true);
    CHECK(d.capabilities.has_eh == false);
    CHECK(d.capabilities.has_rtti == false);
    CHECK(d.capabilities.has_tls == true);
    CHECK(d.capabilities.has_host_services == true);
    CHECK(d.capabilities.has_file_io == true);
    CHECK(d.capabilities.has_time == true);
    CHECK(d.capabilities.volatile_exact_width == true);
}

TEST_CASE("KR32 target descriptor feature macros") {
    const TargetDescriptor& d = kr32_descriptor();

    CHECK(d.feature_macros.target_macro == "__KR32__");
    CHECK(d.feature_macros.char_unsigned_macro == "");
    CHECK(d.feature_macros.c_std_version_macro == "__STDC_VERSION__");
    CHECK(d.feature_macros.cxx_std_version_macro == "__cplusplus");
}

TEST_CASE("KR16 target descriptor identity") {
    const TargetDescriptor& d = kr16_descriptor();

    CHECK(d.identity.family == TargetFamily::Kr16);
    CHECK(d.identity.id == "kr16");
    CHECK(d.identity.name == "KR16");
    CHECK(d.identity.version == 1);
    CHECK(d.identity.register_family == RegisterFamily::Register);
    CHECK(d.identity.pointer_width == 16);
    CHECK(d.identity.endianness == Endianness::Little);
    CHECK(d.identity.max_alignment == 2);
    CHECK(d.identity.address_space_count == 1);
}

TEST_CASE("KR16 target descriptor data model") {
    const TargetDescriptor& d = kr16_descriptor();

    // IP16: int, long, pointer all 16-bit; no long long, no float/double.
    CHECK(d.data_model.char_size == 8);
    CHECK(d.data_model.short_size == 16);
    CHECK(d.data_model.int_size == 16);
    CHECK(d.data_model.long_size == 16);
    CHECK(d.data_model.long_long_size == 0);
    CHECK(d.data_model.pointer_size == 16);
    CHECK(d.data_model.float_size == 0);
    CHECK(d.data_model.double_size == 0);
    CHECK(d.data_model.size_t_size == 16);
    CHECK(d.data_model.ptrdiff_t_size == 16);
    CHECK(d.data_model.intmax_t_size == 16);
    CHECK(d.data_model.char_is_unsigned == true);
}

TEST_CASE("KR16 target descriptor registers") {
    const TargetDescriptor& d = kr16_descriptor();

    CHECK(d.registers.gpr_count == 5);
    CHECK(d.registers.fpr_count == 0);
    CHECK(d.registers.fp_register == 4);
    CHECK(d.registers.tp_register == -1);
    CHECK(d.registers.sp_register == -1);
    CHECK(d.registers.pc_register == -1);

    // Caller-saved: R0-R4 (bits 0-4).
    CHECK((d.registers.gpr_caller_saved & 0x001FuLL) != 0);
    // Callee-saved: R4 (bit 4).
    CHECK((d.registers.gpr_callee_saved & 0x0010uLL) != 0);
    // No reserved registers.
    CHECK(d.registers.gpr_reserved == 0);
}

TEST_CASE("KR16 target descriptor capabilities") {
    const TargetDescriptor& d = kr16_descriptor();

    CHECK(d.capabilities.has_fpu == false);
    CHECK(d.capabilities.has_matrix == false);
    CHECK(d.capabilities.has_atomics == false);
    CHECK(d.capabilities.has_guest_threads == false);
    CHECK(d.capabilities.has_eh == false);
    CHECK(d.capabilities.has_rtti == false);
    CHECK(d.capabilities.has_tls == false);
    CHECK(d.capabilities.has_host_services == false);
    CHECK(d.capabilities.has_file_io == false);
    CHECK(d.capabilities.has_time == false);
    CHECK(d.capabilities.volatile_exact_width == true);
}

TEST_CASE("KR16 target descriptor feature macros") {
    const TargetDescriptor& d = kr16_descriptor();

    CHECK(d.feature_macros.target_macro == "__KR16__");
    CHECK(d.feature_macros.char_unsigned_macro == "__CHAR_UNSIGNED__");
    CHECK(d.feature_macros.c_std_version_macro == "__STDC_VERSION__");
    CHECK(d.feature_macros.cxx_std_version_macro == "__cplusplus");
}

TEST_CASE("KR16 target descriptor constraints") {
    const TargetDescriptor& d = kr16_descriptor();

    CHECK(d.constraints.max_object_size == 0);
    CHECK(d.constraints.max_alignment == 2);
    CHECK(d.constraints.supports_section_gc == true);
    CHECK(d.constraints.supports_branch_relaxation == true);
}

TEST_CASE("KR32 target descriptor constraints") {
    const TargetDescriptor& d = kr32_descriptor();

    CHECK(d.constraints.max_object_size == 0);
    CHECK(d.constraints.max_alignment == 16);
    CHECK(d.constraints.supports_section_gc == false);
    CHECK(d.constraints.supports_branch_relaxation == true);
}
