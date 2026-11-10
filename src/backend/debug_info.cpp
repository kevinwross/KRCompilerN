#include "debug_info.h"

#include <algorithm>

namespace krc {

MachineDebugInfoCollector::MachineDebugInfoCollector(
    const MachineModule& module)
    : module_(module) {}

bool MachineDebugInfoCollector::collect() {
    for (const auto& fn : module_.functions) {
        if (!fn)
            continue;

        collect_function(*fn);
    }

    return true;
}

void MachineDebugInfoCollector::collect_function(const MachineFunction& fn) {
    BackendFuncDebugInfo func_info{};
    func_info.name = fn.name;
    func_info.start_address = fn.debug_start_address;
    func_info.size = fn.debug_size;
    func_info.saved_fp_offset = fn.debug_saved_fp_offset;
    func_info.saved_ra_offset = fn.debug_saved_ra_offset;
    func_info.cfa_rule = fn.debug_cfa_rule;
    info_.functions.push_back(std::move(func_info));

    // Collect variable debug info from all instructions in this function.
    for (const auto& bb : fn.blocks) {
        if (!bb)
            continue;
        for (const auto& inst : bb->instructions) {
            if (inst)
                collect_instruction(*inst, fn);
        }
    }
}

void MachineDebugInfoCollector::collect_instruction(
    const MachineInstr& inst, const MachineFunction& /*fn*/) {
    for (const auto& op : inst.operands) {
        if (op.kind != MachineOperand::Kind::StackSlot)
            continue;

        // Check if we already have a variable for this stack offset.
        auto it = std::find_if(info_.variables.begin(),
                               info_.variables.end(),
                               [&](const BackendVarDebugInfo& v) {
                                   return v.stack_offset == op.stack_offset &&
                                          v.location_kind == 0;
                               });
        if (it != info_.variables.end())
            continue;

        BackendVarDebugInfo var_info{};
        // Generate a name based on the stack offset.
        var_info.name = "local_" + std::to_string(op.stack_offset);
        var_info.type_id = 0; // unknown type
        var_info.stack_offset = op.stack_offset;
        var_info.location_kind = 0; // frame_offset
        var_info.location_reg = 0;
        var_info.flags = 0; // local
        var_info.reserved = 0;
        info_.variables.push_back(std::move(var_info));
    }
}

} // namespace krc
