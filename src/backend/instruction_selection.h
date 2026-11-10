#ifndef KRC_INSTRUCTION_SELECTION_H
#define KRC_INSTRUCTION_SELECTION_H

#include "ir.h"
#include "machine.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace krc {

// Lowers a typed SSA IR module to target-independent machine instructions.
// The result is a MachineModule with pseudo instructions that the backend
// (register allocation, frame layout) will expand into target-specific
// instructions.
class InstructionSelection {
public:
    explicit InstructionSelection(IRModule& ir_module);

    // Run instruction selection on the whole module.
    // Returns a MachineModule (target-independent pseudo instructions).
    MachineModule run();

private:
    IRModule& ir_module_;
    MachineModule mm_;

    // Lower a single function.
    void lower_function(IRFunction& fn);

    // Lower a basic block.
    void lower_basic_block(IRBasicBlock* bb, MachineBasicBlock* mbb);

    // Lower an instruction to one or more machine instructions.
    void lower_instruction(IRInstruction* inst, MachineBasicBlock* mbb,
                           std::unordered_map<std::string, std::uint32_t>&
                               value_to_vreg,
                           std::uint32_t& next_vreg);

    // Map from IR value name to machine operand (virtual register).
    std::vector<MachineOperand> map_operands(IRInstruction* inst);
};

} // namespace krc

#endif // KRC_INSTRUCTION_SELECTION_H
