#ifndef KRC_IR_H
#define KRC_IR_H

#include "source_manager.h"

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace krc {

class DiagnosticEngine;

//------------------------------------------------------------------------------
// IR types (plan.md §8.3)
//------------------------------------------------------------------------------

enum class IRTypeKind : std::uint8_t {
    Void,
    Bool,
    I8,
    I16,
    I32,
    I64,
    F32,
    F64,
    Ptr,        // address space 0 only in v1
    Array,
    Struct,
    Function,
    Opaque,     // incomplete types / forward declarations
};

// A type in the typed SSA IR. Types are interned: two structurally equal
// types are the same shared_ptr instance.
struct IRType {
    IRTypeKind kind = IRTypeKind::Void;
    std::shared_ptr<IRType> pointee;          // for Ptr
    std::shared_ptr<IRType> element_type;     // for Array
    std::uint64_t array_length = 0;           // for Array
    std::vector<std::shared_ptr<IRType>> members;  // for Struct
    std::shared_ptr<IRType> return_type;      // for Function
    std::vector<std::shared_ptr<IRType>> param_types; // for Function
    bool is_variadic = false;                 // for Function

    bool operator==(const IRType& other) const noexcept;
    bool operator!=(const IRType& other) const noexcept { return !(*this == other); }
};

// Builtin types (singleton accessors).
std::shared_ptr<IRType> ir_void_type();
std::shared_ptr<IRType> ir_bool_type();
std::shared_ptr<IRType> ir_i8_type();
std::shared_ptr<IRType> ir_i16_type();
std::shared_ptr<IRType> ir_i32_type();
std::shared_ptr<IRType> ir_i64_type();
std::shared_ptr<IRType> ir_f32_type();
std::shared_ptr<IRType> ir_f64_type();

// Type constructors.
std::shared_ptr<IRType> ir_ptr_type(const std::shared_ptr<IRType>& pointee);
std::shared_ptr<IRType> ir_array_type(const std::shared_ptr<IRType>& elem,
                                      std::uint64_t length);
std::shared_ptr<IRType> ir_struct_type(
    const std::vector<std::shared_ptr<IRType>>& members);
std::shared_ptr<IRType> ir_function_type(
    const std::shared_ptr<IRType>& return_type,
    const std::vector<std::shared_ptr<IRType>>& param_types,
    bool is_variadic = false);

//------------------------------------------------------------------------------
// Linkage and calling convention
//------------------------------------------------------------------------------

enum class IRLinkage : std::uint8_t {
    External,   // visible outside the module
    Internal,   // module-local
    Private,    // not accessible from other modules
};

enum class IRCallingConv : std::uint8_t {
    C,
};

//------------------------------------------------------------------------------
// Values (plan.md §8.2)
//------------------------------------------------------------------------------

// A value in the IR: an instruction result, a function argument, or a
// constant. Each SSA value has a type and a name (virtual register).
struct IRValue {
    std::shared_ptr<IRType> type;
    std::string name;        // e.g. "%1", "%x", "@foo"
    SourceLocation loc;      // source location (may be invalid)

    IRValue(std::shared_ptr<IRType> t, std::string n,
            SourceLocation l = SourceLocation{})
        : type(std::move(t)), name(std::move(n)), loc(std::move(l)) {}
    virtual ~IRValue() = default;
};

// A constant value.
struct IRConstant : IRValue {
    enum class Kind { Int, Float, NullPtr, Undef };
    Kind kind = Kind::Int;
    std::int64_t int_value = 0;
    double float_value = 0.0;

    IRConstant(std::shared_ptr<IRType> t, std::string n, Kind k,
               std::int64_t iv = 0, double fv = 0.0)
        : IRValue(std::move(t), std::move(n)), kind(k),
          int_value(iv), float_value(fv) {}
};

// A function argument.
struct IRArgument : IRValue {
    std::uint32_t index = 0;

    IRArgument(std::shared_ptr<IRType> t, std::string n, std::uint32_t idx)
        : IRValue(std::move(t), std::move(n)), index(idx) {}
};

//------------------------------------------------------------------------------
// Instructions (plan.md §8.2)
//------------------------------------------------------------------------------

enum class IROpcode : std::uint8_t {
    // Memory
    Alloca,   // allocate stack slot
    Load,     // load from pointer
    Store,    // store to pointer
    // Integer arithmetic
    IAdd,     // integer add
    ISub,     // integer subtract
    IMul,     // integer multiply
    IDiv,     // integer divide (signed)
    IRem,     // integer remainder (signed)
    IShl,     // shift left
    IShr,     // shift right (logical)
    ISra,     // shift right (arithmetic)
    IAnd,     // bitwise and
    IOr,      // bitwise or
    IXor,     // bitwise xor
    // Integer comparison
    ICmpEq,   // equal
    ICmpNe,   // not equal
    ICmpLtS,  // less than (signed)
    ICmpGtS,  // greater than (signed)
    ICmpLeS,  // less than or equal (signed)
    ICmpGeS,  // greater than or equal (signed)
    ICmpLtU,  // less than (unsigned)
    ICmpGtU,  // greater than (unsigned)
    ICmpLeU,  // less than or equal (unsigned)
    ICmpGeU,  // greater than or equal (unsigned)
    // Floating-point
    FAdd, FSub, FMul, FDiv, FRem,
    FCmpOeq,  // ordered equal
    FCmpOne,  // ordered not equal
    FCmpOlt,  // ordered less than
    FCmpOgt,  // ordered greater than
    FCmpOle,  // ordered less than or equal
    FCmpOge,  // ordered greater than or equal
    // Conversions
    ZExt,     // zero extend
    SExt,     // sign extend
    Trunc,    // truncate
    FToUI,    // float to unsigned int
    UIToF,    // unsigned int to float
    SIToF,    // signed int to float
    FToSI,    // float to signed int
    BitCast,  // bitcast (same size)
    PtrToInt, // pointer to integer
    IntToPtr, // integer to pointer
    // Control flow
    Br,          // unconditional branch
    CondBr,      // conditional branch
    Switch,      // switch (lowered by backend)
    Return,      // return
    Unreachable, // unreachable
    // Calls
    Call,        // direct or indirect call
    // Phi (SSA merge at block entry)
    Phi,
    // Atomic operations (v1: seq_cst)
    AtomicLoad,
    AtomicStore,
    AtomicCmpXchg,
    AtomicRMW,
    Fence,
    // Intrinsics
    Memcpy,
    Memset,
};

// Memory effect flags.
enum class IRMemEffect : std::uint8_t {
    None = 0,
    Read = 1,
    Write = 2,
    ReadWrite = Read | Write,
};

// An instruction in the IR. Each instruction defines a virtual register
// (its result) unless it is a terminator or void-producing.
struct IRInstruction : IRValue {
    IROpcode opcode = IROpcode::IAdd;
    bool is_volatile = false;
    IRMemEffect mem_effect = IRMemEffect::None;
    std::uint32_t alignment = 0;

    // Operands: values this instruction reads. For terminators, these are
    // the branch targets (BasicBlock*) stored as IRValue* with a null type.
    std::vector<IRValue*> operands;

    // For phi nodes: pairs of (value, predecessor block).
    std::vector<std::pair<IRValue*, std::string>> phi_incoming;

    IRInstruction(std::shared_ptr<IRType> ty, std::string name,
                  IROpcode op, SourceLocation loc = SourceLocation{})
        : IRValue(std::move(ty), std::move(name), std::move(loc)),
          opcode(op) {}
};

// A basic block: a sequence of instructions ending in a terminator.
struct IRBasicBlock {
    std::string name;
    std::vector<std::unique_ptr<IRInstruction>> instructions;
    std::unique_ptr<IRInstruction> terminator;  // must be set

    IRBasicBlock(std::string n) : name(std::move(n)) {}
    ~IRBasicBlock() = default;
};

//------------------------------------------------------------------------------
// Function
//------------------------------------------------------------------------------

struct IRFunction {
    std::string name;
    IRLinkage linkage = IRLinkage::External;
    IRCallingConv calling_conv = IRCallingConv::C;
    std::shared_ptr<IRType> return_type = ir_void_type();
    std::vector<std::shared_ptr<IRType>> param_types;
    std::vector<std::string> param_names;
    std::vector<std::unique_ptr<IRBasicBlock>> basic_blocks;
    IRBasicBlock* entry_block = nullptr;
    std::uint32_t frame_size = 0;   // set by backend frame layout

    IRFunction(std::string n) : name(std::move(n)) {}
    ~IRFunction() = default;
};

//------------------------------------------------------------------------------
// Global variable
//------------------------------------------------------------------------------

struct IRGlobal {
    std::string name;
    std::shared_ptr<IRType> type;
    IRLinkage linkage = IRLinkage::External;
    bool is_constant = false;
    std::shared_ptr<IRConstant> initializer;  // may be null
};

//------------------------------------------------------------------------------
// Module
//------------------------------------------------------------------------------

struct IRModule {
    std::string target_id;        // e.g. "kr32"
    std::vector<std::shared_ptr<IRType>> type_table;
    std::vector<IRGlobal> globals;
    std::vector<std::unique_ptr<IRFunction>> functions;

    IRModule(std::string tid) : target_id(std::move(tid)) {}
    ~IRModule() = default;
};

//------------------------------------------------------------------------------
// IR builder
//------------------------------------------------------------------------------

// Builds IR incrementally: creates functions, basic blocks, and emits
// instructions with automatic virtual register numbering.
class IRBuilder {
public:
    explicit IRBuilder(IRModule& module);

    // Start a new function.
    void begin_function(std::string name, IRLinkage linkage,
                        std::shared_ptr<IRType> return_type,
                        const std::vector<std::shared_ptr<IRType>>& param_types,
                        const std::vector<std::string>& param_names);

    // Finish the current function (validate terminators, etc.).
    void end_function();

    // Create a new basic block and make it the current insertion block.
    IRBasicBlock* create_block(std::string name);

    // Set the current insertion block.
    void set_insert_block(IRBasicBlock* block) { insert_block_ = block; }
    IRBasicBlock* insert_block() const { return insert_block_; }

    // Emit an instruction into the current block. Returns the instruction.
    IRInstruction* emit(std::unique_ptr<IRInstruction> inst);

    // Convenience emitters for common instructions.
    IRInstruction* emit_alloca(std::shared_ptr<IRType> type,
                               std::string name,
                               SourceLocation loc = SourceLocation{});
    IRInstruction* emit_load(std::shared_ptr<IRType> type, IRValue* ptr,
                             std::string name, bool volatile_ = false,
                             SourceLocation loc = SourceLocation{});
    IRInstruction* emit_store(IRValue* val, IRValue* ptr,
                              bool volatile_ = false,
                              SourceLocation loc = SourceLocation{});
    IRInstruction* emit_binop(IROpcode op, std::shared_ptr<IRType> type,
                              IRValue* lhs, IRValue* rhs,
                              std::string name,
                              SourceLocation loc = SourceLocation{});
    IRInstruction* emit_icmp(IROpcode op, IRValue* lhs, IRValue* rhs,
                             std::string name,
                             SourceLocation loc = SourceLocation{});
    IRInstruction* emit_fcmp(IROpcode op, IRValue* lhs, IRValue* rhs,
                             std::string name,
                             SourceLocation loc = SourceLocation{});
    IRInstruction* emit_zext(std::shared_ptr<IRType> type, IRValue* src,
                             std::string name,
                             SourceLocation loc = SourceLocation{});
    IRInstruction* emit_sext(std::shared_ptr<IRType> type, IRValue* src,
                             std::string name,
                             SourceLocation loc = SourceLocation{});
    IRInstruction* emit_trunc(std::shared_ptr<IRType> type, IRValue* src,
                              std::string name,
                              SourceLocation loc = SourceLocation{});
    IRInstruction* emit_bitcast(std::shared_ptr<IRType> type, IRValue* src,
                                std::string name,
                                SourceLocation loc = SourceLocation{});
    IRInstruction* emit_ret(IRValue* value = nullptr,
                            SourceLocation loc = SourceLocation{});
    IRInstruction* emit_br(IRBasicBlock* target,
                           SourceLocation loc = SourceLocation{});
    IRInstruction* emit_cond_br(IRValue* cond, IRBasicBlock* true_target,
                                IRBasicBlock* false_target,
                                SourceLocation loc = SourceLocation{});
    IRInstruction* emit_unreachable(
        SourceLocation loc = SourceLocation{});
    IRInstruction* emit_call(std::shared_ptr<IRType> result_type,
                             IRValue* callee,
                             const std::vector<IRValue*>& args,
                             std::string name,
                             SourceLocation loc = SourceLocation{});
    IRInstruction* emit_phi(std::shared_ptr<IRType> type, std::string name,
                            SourceLocation loc = SourceLocation{});

    // Add a phi incoming value (value comes from predecessor block).
    void add_phi_incoming(IRInstruction* phi, IRValue* value,
                          std::string_view pred_block_name);

    // Create a constant.
    IRConstant* create_int_const(std::shared_ptr<IRType> type, std::string name,
                                 std::int64_t value);
    IRConstant* create_float_const(std::shared_ptr<IRType> type,
                                   std::string name, double value);
    IRConstant* create_null_ptr(std::shared_ptr<IRType> type,
                                std::string name);

    // Create a function argument.
    IRArgument* create_argument(std::shared_ptr<IRType> type,
                                std::string name, std::uint32_t index);

    // Accessors.
    IRModule& module() { return module_; }
    IRFunction* current_function() const { return current_fn_; }
    IRBasicBlock* current_block() const { return insert_block_; }

    // Get the next virtual register number.
    std::string next_reg();

private:
    IRModule& module_;
    IRFunction* current_fn_ = nullptr;
    IRBasicBlock* insert_block_ = nullptr;
    std::uint32_t next_reg_num_ = 0;

    std::string make_reg_name();
};

//------------------------------------------------------------------------------
// IR verifier (plan.md §8.10)
//------------------------------------------------------------------------------

// Verify an IR module. Returns true if the module is well-formed.
// On failure, emits diagnostics via the provided diagnostic engine.
bool verify_module(IRModule& module, DiagnosticEngine& diagnostics);

//------------------------------------------------------------------------------
// IR textual printer (plan.md §8.9)
//------------------------------------------------------------------------------

// Print the module in a human-readable form (not stable for parsing).
void dump_module(const IRModule& module, std::ostream& out);

// Print a single function.
void dump_function(const IRFunction& fn, std::ostream& out);

} // namespace krc

#endif // KRC_IR_H
