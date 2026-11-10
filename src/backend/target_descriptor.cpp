#include "target_descriptor.h"

namespace krc {

namespace {

// KR32 register bit masks (R0-R15).
// R0-R3: integer arguments (caller-saved).
// R7: frame pointer (callee-saved).
// R11: thread pointer (reserved).
constexpr std::uint64_t kr32_gpr_caller_saved = 0x000FuLL;   // R0-R3
constexpr std::uint64_t kr32_gpr_callee_saved = 0x0080uLL;   // R7
constexpr std::uint64_t kr32_gpr_reserved = 0x0800uLL;       // R11

// KR16 register bit masks (R0-R4).
// R0: return value (caller-saved).
// R1-R3: arguments (caller-saved).
// R4: frame pointer (callee-saved).
constexpr std::uint64_t kr16_gpr_caller_saved = 0x001FuLL;   // R0-R4 (conservative: all caller-saved)
constexpr std::uint64_t kr16_gpr_callee_saved = 0x0010uLL;   // R4 (frame pointer)
constexpr std::uint64_t kr16_gpr_reserved = 0x0000uLL;

} // namespace

const TargetDescriptor& kr32_descriptor() {
    static const TargetDescriptor desc{
        // Identity
        TargetIdentity{
            .family = TargetFamily::Kr32,
            .id = "kr32",
            .name = "KR32",
            .version = 1,
            .register_family = RegisterFamily::Register,
            .pointer_width = 32,
            .endianness = Endianness::Little,
            .max_alignment = 16,
            .address_space_count = 1,
        },
        // Data model (plan.md §10.1 KR32 ILP32)
        DataModel{
            .char_size = 8, .short_size = 16, .int_size = 32, .long_size = 32,
            .long_long_size = 64, .pointer_size = 32,
            .float_size = 32, .double_size = 64, .long_double_size = 64,
            .wchar_t_size = 32, .char16_t_size = 16, .char32_t_size = 32,
            .size_t_size = 32, .ptrdiff_t_size = 32, .intmax_t_size = 64,
            .char_align = 1, .short_align = 2, .int_align = 4, .long_align = 4,
            .long_long_align = 8, .pointer_align = 4,
            .float_align = 4, .double_align = 8, .long_double_align = 8,
            .wchar_t_align = 4, .char16_t_align = 2, .char32_t_align = 4,
            .size_t_align = 4, .ptrdiff_t_align = 4, .intmax_t_align = 8,
            .char_is_unsigned = false,  // signed char
            .wchar_t_is_unsigned = false,
        },
        // Registers (plan.md §9.5)
        RegisterInfo{
            .sp_register = -1,
            .fp_register = 7,   // R7 is the frame pointer
            .tp_register = 11,  // R11 is the thread pointer
            .pc_register = -1,
            .flags_register = -1,
            .gpr_count = 16,
            .fpr_count = 16,
            .gpr_caller_saved = kr32_gpr_caller_saved,
            .gpr_callee_saved = kr32_gpr_callee_saved,
            .gpr_reserved = kr32_gpr_reserved,
        },
        // Capabilities (plan.md §4.1)
        TargetCapabilities{
            .has_fpu = true,
            .has_matrix = true,
            .has_atomics = true,
            .has_guest_threads = true,
            .has_eh = false,    // v1.1
            .has_rtti = false,  // v1.1
            .has_tls = true,    // static v1.0, dynamic v1.1
            .has_host_services = true,
            .has_file_io = true,
            .has_time = true,
            .volatile_exact_width = true,
        },
        // Feature macros
        FeatureMacros{
            .target_macro = "__KR32__",
            .char_unsigned_macro = "",
            .c_std_version_macro = "__STDC_VERSION__",
            .cxx_std_version_macro = "__cplusplus",
        },
        // Constraints
        TargetConstraints{
            .max_object_size = 0,
            .max_alignment = 16,
            .supports_section_gc = false,
            .supports_branch_relaxation = true,
        },
    };
    return desc;
}

const TargetDescriptor& kr16_descriptor() {
    static const TargetDescriptor desc{
        // Identity
        TargetIdentity{
            .family = TargetFamily::Kr16,
            .id = "kr16",
            .name = "KR16",
            .version = 1,
            .register_family = RegisterFamily::Register,
            .pointer_width = 16,
            .endianness = Endianness::Little,
            .max_alignment = 2,
            .address_space_count = 1,
        },
        // Data model (plan.md §10.1 KR16 IP16 embedded profile)
        DataModel{
            .char_size = 8, .short_size = 16, .int_size = 16, .long_size = 16,
            .long_long_size = 0,   // unsupported on KR16
            .pointer_size = 16,
            .float_size = 0,   // unsupported on KR16
            .double_size = 0,  // unsupported on KR16
            .long_double_size = 0,
            .wchar_t_size = 16, .char16_t_size = 16, .char32_t_size = 0,
            .size_t_size = 16, .ptrdiff_t_size = 16, .intmax_t_size = 16,
            .char_align = 1, .short_align = 2, .int_align = 2, .long_align = 2,
            .long_long_align = 0, .pointer_align = 2,
            .float_align = 0, .double_align = 0, .long_double_align = 0,
            .wchar_t_align = 2, .char16_t_align = 2, .char32_t_align = 0,
            .size_t_align = 2, .ptrdiff_t_align = 2, .intmax_t_align = 2,
            .char_is_unsigned = true,   // __CHAR_UNSIGNED__
            .wchar_t_is_unsigned = false,
        },
        // Registers (plan.md §9.6)
        RegisterInfo{
            .sp_register = -1,
            .fp_register = 4,   // R4 is the frame pointer
            .tp_register = -1,
            .pc_register = -1,
            .flags_register = -1,
            .gpr_count = 5,     // R0-R4
            .fpr_count = 0,
            .gpr_caller_saved = kr16_gpr_caller_saved,
            .gpr_callee_saved = kr16_gpr_callee_saved,
            .gpr_reserved = kr16_gpr_reserved,
        },
        // Capabilities (plan.md §4.1)
        TargetCapabilities{
            .has_fpu = false,
            .has_matrix = false,
            .has_atomics = false,
            .has_guest_threads = false,
            .has_eh = false,
            .has_rtti = false,
            .has_tls = false,
            .has_host_services = false,  // BIOS SYS, not host services
            .has_file_io = false,
            .has_time = false,
            .volatile_exact_width = true,
        },
        // Feature macros
        FeatureMacros{
            .target_macro = "__KR16__",
            .char_unsigned_macro = "__CHAR_UNSIGNED__",
            .c_std_version_macro = "__STDC_VERSION__",
            .cxx_std_version_macro = "__cplusplus",
        },
        // Constraints
        TargetConstraints{
            .max_object_size = 0,
            .max_alignment = 2,
            .supports_section_gc = true,   // code-size budget on KR16
            .supports_branch_relaxation = true,  // mandatory (ADR 0003)
        },
    };
    return desc;
}

} // namespace krc
