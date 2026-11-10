#ifndef KRC_BACKEND_DEBUG_INFO_H
#define KRC_BACKEND_DEBUG_INFO_H

#include "machine.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace krc {

// Backend-native debug information collected from the machine module after
// instruction selection, register allocation, and frame layout. No dependency
// on the debug or frontend modules; the driver converts this to KDBG.
struct BackendFuncDebugInfo {
    std::string name;
    std::uint64_t start_address = 0;   // set by assembler/linker
    std::uint64_t size = 0;            // set by assembler/linker
    std::uint32_t saved_fp_offset = 0xFFFFFFFFu;
    std::uint32_t saved_ra_offset = 0xFFFFFFFFu;
    std::uint32_t cfa_rule = 0;
};

struct BackendVarDebugInfo {
    std::string name;
    std::uint32_t type_id = 0;
    std::int32_t stack_offset = 0;      // frame-relative offset (-1 if register)
    std::uint8_t location_kind = 0;     // 0=frame_offset, 1=register, 2=global_addr
    std::uint8_t location_reg = 0;
    std::uint8_t flags = 0;             // 0=local, 1=param, 2=static, 3=global, 4=TLS
    std::uint8_t reserved = 0;
};

struct BackendDebugInfo {
    std::vector<BackendFuncDebugInfo> functions;
    std::vector<BackendVarDebugInfo> variables;
};

// Collects backend debug information from a machine module.
class MachineDebugInfoCollector {
public:
    explicit MachineDebugInfoCollector(const MachineModule& module);

    bool collect();

    const BackendDebugInfo& info() const { return info_; }

private:
    const MachineModule& module_;
    BackendDebugInfo info_;

    // Collect variable debug info from a single machine function.
    void collect_function(const MachineFunction& fn);

    // Collect variable debug info from a single instruction.
    void collect_instruction(const MachineInstr& inst,
                             const MachineFunction& fn);
};

} // namespace krc

#endif // KRC_BACKEND_DEBUG_INFO_H
