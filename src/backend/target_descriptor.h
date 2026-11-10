#ifndef KRC_TARGET_DESCRIPTOR_H
#define KRC_TARGET_DESCRIPTOR_H

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace krc {

// The register family of a target.
enum class RegisterFamily : std::uint8_t {
    Register,   // general-purpose register file (KR32, KR16)
    Accumulator, // accumulator-based (out of scope v1)
    Stack,      // stack machine (out of scope v1)
};

// Endianness of the target.
enum class Endianness : std::uint8_t {
    Little = 0,
    Big = 1,
};

// The target family.
enum class TargetFamily : std::uint8_t {
    Kr32,
    Kr16,
    Kr64,
};

//------------------------------------------------------------------------------
// Identity section (plan.md §4.2)
//------------------------------------------------------------------------------
struct TargetIdentity {
    TargetFamily family = TargetFamily::Kr32;
    std::string_view id = "kr32";          // stable target ID
    std::string_view name = "KR32";
    std::uint32_t version = 1;
    RegisterFamily register_family = RegisterFamily::Register;
    std::uint32_t pointer_width = 32;      // bits
    Endianness endianness = Endianness::Little;
    std::uint32_t max_alignment = 16;      // bytes
    std::uint32_t address_space_count = 1; // v1: only address space 0
};

//------------------------------------------------------------------------------
// Data model section (plan.md §4.2)
//------------------------------------------------------------------------------
struct DataModel {
    // Sizes in bits.
    std::uint32_t char_size = 8;
    std::uint32_t short_size = 16;
    std::uint32_t int_size = 32;
    std::uint32_t long_size = 32;
    std::uint32_t long_long_size = 64;
    std::uint32_t pointer_size = 32;
    std::uint32_t float_size = 32;
    std::uint32_t double_size = 64;
    std::uint32_t long_double_size = 64;
    std::uint32_t wchar_t_size = 32;
    std::uint32_t char16_t_size = 16;
    std::uint32_t char32_t_size = 32;
    std::uint32_t size_t_size = 32;
    std::uint32_t ptrdiff_t_size = 32;
    std::uint32_t intmax_t_size = 64;

    // Alignments in bytes.
    std::uint32_t char_align = 1;
    std::uint32_t short_align = 2;
    std::uint32_t int_align = 4;
    std::uint32_t long_align = 4;
    std::uint32_t long_long_align = 8;
    std::uint32_t pointer_align = 4;
    std::uint32_t float_align = 4;
    std::uint32_t double_align = 8;
    std::uint32_t long_double_align = 8;
    std::uint32_t wchar_t_align = 4;
    std::uint32_t char16_t_align = 2;
    std::uint32_t char32_t_align = 4;
    std::uint32_t size_t_align = 4;
    std::uint32_t ptrdiff_t_align = 4;
    std::uint32_t intmax_t_align = 8;

    // Signedness.
    bool char_is_unsigned = false;  // __CHAR_UNSIGNED__
    bool wchar_t_is_unsigned = false;
};

//------------------------------------------------------------------------------
// Registers section (plan.md §4.2)
//------------------------------------------------------------------------------
struct RegisterInfo {
    // Fixed architectural roles (register index or -1 if none).
    int sp_register = -1;   // stack pointer
    int fp_register = -1;   // frame pointer
    int tp_register = -1;   // thread pointer
    int pc_register = -1;   // program counter
    int flags_register = -1; // condition/flags register

    std::uint32_t gpr_count = 0;
    std::uint32_t fpr_count = 0;

    // Bit masks: bit i corresponds to register i.
    std::uint64_t gpr_caller_saved = 0;
    std::uint64_t fpr_caller_saved = 0;
    std::uint64_t gpr_callee_saved = 0;
    std::uint64_t fpr_callee_saved = 0;
    std::uint64_t gpr_reserved = 0;
    std::uint64_t fpr_reserved = 0;
};

//------------------------------------------------------------------------------
// Capabilities section (plan.md §4.2)
//------------------------------------------------------------------------------
struct TargetCapabilities {
    bool has_fpu = false;
    bool has_matrix = false;
    bool has_atomics = false;
    bool has_guest_threads = false;
    bool has_eh = false;        // exception handling
    bool has_rtti = false;      // run-time type information
    bool has_tls = false;       // thread-local storage
    bool has_host_services = false;
    bool has_file_io = false;
    bool has_time = false;
    bool volatile_exact_width = false; // volatile accesses exact-width, non-folded
};

//------------------------------------------------------------------------------
// Feature macros section (plan.md §4.2)
//------------------------------------------------------------------------------
struct FeatureMacros {
    std::string_view target_macro = "__KR32__";
    std::string_view char_unsigned_macro = "";     // __CHAR_UNSIGNED__ if set
    std::string_view c_std_version_macro = "__STDC_VERSION__";
    std::string_view cxx_std_version_macro = "__cplusplus";
};

//------------------------------------------------------------------------------
// Constraints section (plan.md §4.2)
//------------------------------------------------------------------------------
struct TargetConstraints {
    std::uint64_t max_object_size = 0;     // 0 = unlimited
    std::uint32_t max_alignment = 16;
    bool supports_section_gc = false;
    bool supports_branch_relaxation = false;
};

//------------------------------------------------------------------------------
// Target descriptor: the complete description of a target.
// All sections are public data; the descriptor is a value type.
//------------------------------------------------------------------------------
class TargetDescriptor {
public:
    TargetDescriptor() = default;

    TargetDescriptor(TargetIdentity id, DataModel dm, RegisterInfo reg,
                     TargetCapabilities cap, FeatureMacros fm,
                     TargetConstraints con)
        : identity(std::move(id)),
          data_model(std::move(dm)),
          registers(std::move(reg)),
          capabilities(std::move(cap)),
          feature_macros(std::move(fm)),
          constraints(std::move(con)) {}

    TargetIdentity identity;
    DataModel data_model;
    RegisterInfo registers;
    TargetCapabilities capabilities;
    FeatureMacros feature_macros;
    TargetConstraints constraints;
};

// Return the KR32 target descriptor (plan.md §4.1, §9.5).
const TargetDescriptor& kr32_descriptor();

// Return the KR16 target descriptor (plan.md §4.1, §9.6).
const TargetDescriptor& kr16_descriptor();

} // namespace krc

#endif // KRC_TARGET_DESCRIPTOR_H
