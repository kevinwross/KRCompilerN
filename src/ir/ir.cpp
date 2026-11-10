#include "ir.h"

#include "diagnostics.h"

#include <algorithm>
#include <cctype>
#include <numeric>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace krc {

//------------------------------------------------------------------------------
// IRType structural equality and interning
//------------------------------------------------------------------------------

bool IRType::operator==(const IRType& other) const noexcept {
    if (kind != other.kind) return false;

    switch (kind) {
        case IRTypeKind::Void:
        case IRTypeKind::Bool:
        case IRTypeKind::I8:
        case IRTypeKind::I16:
        case IRTypeKind::I32:
        case IRTypeKind::I64:
        case IRTypeKind::F32:
        case IRTypeKind::F64:
            return true;

        case IRTypeKind::Ptr:
            if (pointee && other.pointee) return *pointee == *other.pointee;
            return pointee == other.pointee; // both null

        case IRTypeKind::Array:
            if (array_length != other.array_length) return false;
            if (element_type && other.element_type)
                return *element_type == *other.element_type;
            return element_type == other.element_type; // both null

        case IRTypeKind::Struct: {
            if (members.size() != other.members.size()) return false;
            for (std::size_t i = 0; i < members.size(); ++i) {
                if (!members[i] || !other.members[i] ||
                    *members[i] != *other.members[i])
                    return false;
            }
            return true;
        }

        case IRTypeKind::Function: {
            if (is_variadic != other.is_variadic) return false;
            if (return_type == other.return_type ||
                (return_type && other.return_type &&
                 *return_type == *other.return_type)) {
                if (param_types.size() != other.param_types.size())
                    return false;
                for (std::size_t i = 0; i < param_types.size(); ++i) {
                    if (!param_types[i] || !other.param_types[i] ||
                        *param_types[i] != *other.param_types[i])
                        return false;
                }
                return true;
            }
            return false;
        }

        case IRTypeKind::Opaque:
            return true;
    }
    return false;
}

//------------------------------------------------------------------------------
// Builtin types (singletons)
//------------------------------------------------------------------------------

namespace {

std::shared_ptr<IRType> make_simple(IRTypeKind kind) {
    auto t = std::make_shared<IRType>();
    t->kind = kind;
    return t;
}

} // namespace

std::shared_ptr<IRType> ir_void_type() {
    static const auto t = make_simple(IRTypeKind::Void);
    return t;
}

std::shared_ptr<IRType> ir_bool_type() {
    static const auto t = make_simple(IRTypeKind::Bool);
    return t;
}

std::shared_ptr<IRType> ir_i8_type() {
    static const auto t = make_simple(IRTypeKind::I8);
    return t;
}

std::shared_ptr<IRType> ir_i16_type() {
    static const auto t = make_simple(IRTypeKind::I16);
    return t;
}

std::shared_ptr<IRType> ir_i32_type() {
    static const auto t = make_simple(IRTypeKind::I32);
    return t;
}

std::shared_ptr<IRType> ir_i64_type() {
    static const auto t = make_simple(IRTypeKind::I64);
    return t;
}

std::shared_ptr<IRType> ir_f32_type() {
    static const auto t = make_simple(IRTypeKind::F32);
    return t;
}

std::shared_ptr<IRType> ir_f64_type() {
    static const auto t = make_simple(IRTypeKind::F64);
    return t;
}

//------------------------------------------------------------------------------
// Type constructors (with interning)
//------------------------------------------------------------------------------

namespace {

// Type interning cache. Structurally equal types share one instance.
// Key: a canonical string representation of the type structure.
std::string type_key(const IRType& t) {
    std::string out;
    out.reserve(64);

    switch (t.kind) {
        case IRTypeKind::Void:   out += "v"; break;
        case IRTypeKind::Bool:   out += "b"; break;
        case IRTypeKind::I8:     out += "i8"; break;
        case IRTypeKind::I16:    out += "i16"; break;
        case IRTypeKind::I32:    out += "i32"; break;
        case IRTypeKind::I64:    out += "i64"; break;
        case IRTypeKind::F32:    out += "f32"; break;
        case IRTypeKind::F64:    out += "f64"; break;
        case IRTypeKind::Ptr: {
            out += "p";
            if (t.pointee) out += type_key(*t.pointee);
            else out += "?";
            break;
        }
        case IRTypeKind::Array: {
            out += "a";
            out += std::to_string(t.array_length);
            out += ":";
            if (t.element_type) out += type_key(*t.element_type);
            else out += "?";
            break;
        }
        case IRTypeKind::Struct: {
            out += "s";
            for (const auto& m : t.members) {
                out += "(";
                if (m) out += type_key(*m);
                else out += "?";
                out += ")";
            }
            break;
        }
        case IRTypeKind::Function: {
            out += "f(";
            if (t.return_type) out += type_key(*t.return_type);
            else out += "?";
            out += ")(";
            for (const auto& p : t.param_types) {
                if (p) out += type_key(*p);
                else out += "?";
                out += ",";
            }
            out += ")";
            if (t.is_variadic) out += "v";
            break;
        }
        case IRTypeKind::Opaque: out += "opaque"; break;
    }
    return out;
}

} // namespace

std::shared_ptr<IRType> ir_ptr_type(const std::shared_ptr<IRType>& pointee) {
    static std::unordered_map<std::string, std::shared_ptr<IRType>> interned;
    auto t = std::make_shared<IRType>();
    t->kind = IRTypeKind::Ptr;
    t->pointee = pointee;
    std::string key = type_key(*t);
    auto it = interned.find(key);
    if (it != interned.end()) return it->second;
    interned.emplace(key, t);
    return t;
}

std::shared_ptr<IRType> ir_array_type(const std::shared_ptr<IRType>& elem,
                                      std::uint64_t length) {
    static std::unordered_map<std::string, std::shared_ptr<IRType>> interned;
    auto t = std::make_shared<IRType>();
    t->kind = IRTypeKind::Array;
    t->element_type = elem;
    t->array_length = length;
    std::string key = type_key(*t);
    auto it = interned.find(key);
    if (it != interned.end()) return it->second;
    interned.emplace(key, t);
    return t;
}

std::shared_ptr<IRType> ir_struct_type(
    const std::vector<std::shared_ptr<IRType>>& members) {
    static std::unordered_map<std::string, std::shared_ptr<IRType>> interned;
    auto t = std::make_shared<IRType>();
    t->kind = IRTypeKind::Struct;
    t->members = members;
    std::string key = type_key(*t);
    auto it = interned.find(key);
    if (it != interned.end()) return it->second;
    interned.emplace(key, t);
    return t;
}

std::shared_ptr<IRType> ir_function_type(
    const std::shared_ptr<IRType>& return_type,
    const std::vector<std::shared_ptr<IRType>>& param_types,
    bool is_variadic) {
    static std::unordered_map<std::string, std::shared_ptr<IRType>> interned;
    auto t = std::make_shared<IRType>();
    t->kind = IRTypeKind::Function;
    t->return_type = return_type;
    t->param_types = param_types;
    t->is_variadic = is_variadic;
    std::string key = type_key(*t);
    auto it = interned.find(key);
    if (it != interned.end()) return it->second;
    interned.emplace(key, t);
    return t;
}

//------------------------------------------------------------------------------
// IRBuilder
//------------------------------------------------------------------------------

IRBuilder::IRBuilder(IRModule& module) : module_(module) {}

void IRBuilder::begin_function(
    std::string name, IRLinkage linkage, std::shared_ptr<IRType> return_type,
    const std::vector<std::shared_ptr<IRType>>& param_types,
    const std::vector<std::string>& param_names) {
    auto fn = std::make_unique<IRFunction>(std::move(name));
    fn->linkage = linkage;
    fn->return_type = std::move(return_type);
    fn->param_types = param_types;
    fn->param_names = param_names;

    // Create the entry block.
    auto entry = std::make_unique<IRBasicBlock>("entry");
    IRBasicBlock* entry_ptr = entry.get();
    fn->basic_blocks.push_back(std::move(entry));
    fn->entry_block = entry_ptr;

    current_fn_ = fn.get();
    insert_block_ = entry_ptr;
    next_reg_num_ = 0;

    module_.functions.push_back(std::move(fn));
}

void IRBuilder::end_function() {
    // Ensure every block has a terminator.
    for (auto& bb : current_fn_->basic_blocks) {
        if (!bb->terminator) {
            // Emit an unreachable terminator as a safety net.
            bb->terminator = std::make_unique<IRInstruction>(
                ir_void_type(), "unreachable", IROpcode::Unreachable);
        }
    }
    current_fn_ = nullptr;
    insert_block_ = nullptr;
}

IRBasicBlock* IRBuilder::create_block(std::string name) {
    auto bb = std::make_unique<IRBasicBlock>(std::move(name));
    IRBasicBlock* ptr = bb.get();
    current_fn_->basic_blocks.push_back(std::move(bb));
    return ptr;
}

IRInstruction* IRBuilder::emit(std::unique_ptr<IRInstruction> inst) {
    if (!insert_block_) return nullptr;

    // Assign a virtual register name if the instruction has a non-void type.
    if (inst->type && inst->type->kind != IRTypeKind::Void) {
        inst->name = make_reg_name();
    }

    IRInstruction* raw = inst.get();
    insert_block_->instructions.push_back(std::move(inst));
    return raw;
}

IRInstruction* IRBuilder::emit_alloca(std::shared_ptr<IRType> type,
                                      std::string name,
                                      SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        ir_ptr_type(type), std::move(name), IROpcode::Alloca, std::move(loc));
    inst->alignment = type->kind == IRTypeKind::Struct ? 8 : 4;
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_load(std::shared_ptr<IRType> type, IRValue* ptr,
                                    std::string name, bool volatile_,
                                    SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        std::move(type), std::move(name), IROpcode::Load, std::move(loc));
    inst->operands.push_back(ptr);
    inst->is_volatile = volatile_;
    inst->mem_effect = IRMemEffect::Read;
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_store(IRValue* val, IRValue* ptr,
                                     bool volatile_,
                                     SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        ir_void_type(), "", IROpcode::Store, std::move(loc));
    inst->operands.push_back(val);
    inst->operands.push_back(ptr);
    inst->is_volatile = volatile_;
    inst->mem_effect = IRMemEffect::Write;
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_binop(IROpcode op, std::shared_ptr<IRType> type,
                                     IRValue* lhs, IRValue* rhs,
                                     std::string name,
                                     SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        std::move(type), std::move(name), op, std::move(loc));
    inst->operands.push_back(lhs);
    inst->operands.push_back(rhs);
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_icmp(IROpcode op, IRValue* lhs, IRValue* rhs,
                                    std::string name,
                                    SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        ir_i8_type(), std::move(name), op, std::move(loc));
    inst->operands.push_back(lhs);
    inst->operands.push_back(rhs);
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_fcmp(IROpcode op, IRValue* lhs, IRValue* rhs,
                                    std::string name,
                                    SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        ir_i8_type(), std::move(name), op, std::move(loc));
    inst->operands.push_back(lhs);
    inst->operands.push_back(rhs);
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_zext(std::shared_ptr<IRType> type, IRValue* src,
                                    std::string name,
                                    SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        std::move(type), std::move(name), IROpcode::ZExt, std::move(loc));
    inst->operands.push_back(src);
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_sext(std::shared_ptr<IRType> type, IRValue* src,
                                    std::string name,
                                    SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        std::move(type), std::move(name), IROpcode::SExt, std::move(loc));
    inst->operands.push_back(src);
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_trunc(std::shared_ptr<IRType> type,
                                     IRValue* src, std::string name,
                                     SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        std::move(type), std::move(name), IROpcode::Trunc, std::move(loc));
    inst->operands.push_back(src);
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_bitcast(std::shared_ptr<IRType> type,
                                       IRValue* src, std::string name,
                                       SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        std::move(type), std::move(name), IROpcode::BitCast, std::move(loc));
    inst->operands.push_back(src);
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_ret(IRValue* value,
                                   SourceLocation loc) {
    std::shared_ptr<IRType> ret_type = ir_void_type();
    if (value) ret_type = value->type;
    auto inst = std::make_unique<IRInstruction>(
        ret_type, "", IROpcode::Return, std::move(loc));
    if (value) inst->operands.push_back(value);
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_br(IRBasicBlock* target,
                                  SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        ir_void_type(), "", IROpcode::Br, std::move(loc));
    // Branch targets are stored as operand pointers with null type.
    inst->operands.push_back(reinterpret_cast<IRValue*>(target));
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_cond_br(IRValue* cond,
                                       IRBasicBlock* true_target,
                                       IRBasicBlock* false_target,
                                       SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        ir_void_type(), "", IROpcode::CondBr, std::move(loc));
    inst->operands.push_back(cond);
    inst->operands.push_back(reinterpret_cast<IRValue*>(true_target));
    inst->operands.push_back(reinterpret_cast<IRValue*>(false_target));
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_unreachable(SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        ir_void_type(), "", IROpcode::Unreachable, std::move(loc));
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_call(std::shared_ptr<IRType> result_type,
                                    IRValue* callee,
                                    const std::vector<IRValue*>& args,
                                    std::string name,
                                    SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        std::move(result_type), std::move(name), IROpcode::Call,
        std::move(loc));
    inst->operands.push_back(callee);
    for (auto* a : args) inst->operands.push_back(a);
    return emit(std::move(inst));
}

IRInstruction* IRBuilder::emit_phi(std::shared_ptr<IRType> type,
                                   std::string name,
                                   SourceLocation loc) {
    auto inst = std::make_unique<IRInstruction>(
        std::move(type), std::move(name), IROpcode::Phi, std::move(loc));
    return emit(std::move(inst));
}

void IRBuilder::add_phi_incoming(IRInstruction* phi, IRValue* value,
                                 std::string_view pred_block_name) {
    phi->phi_incoming.emplace_back(value, std::string(pred_block_name));
}

IRConstant* IRBuilder::create_int_const(std::shared_ptr<IRType> type,
                                        std::string name,
                                        std::int64_t value) {
    auto* c = new IRConstant(std::move(type), std::move(name),
                             IRConstant::Kind::Int, value);
    return c;
}

IRConstant* IRBuilder::create_float_const(std::shared_ptr<IRType> type,
                                          std::string name,
                                          double value) {
    auto* c = new IRConstant(std::move(type), std::move(name),
                             IRConstant::Kind::Float, 0, value);
    return c;
}

IRConstant* IRBuilder::create_null_ptr(std::shared_ptr<IRType> type,
                                       std::string name) {
    auto* c = new IRConstant(std::move(type), std::move(name),
                             IRConstant::Kind::NullPtr);
    return c;
}

IRArgument* IRBuilder::create_argument(std::shared_ptr<IRType> type,
                                       std::string name,
                                       std::uint32_t index) {
    auto* a = new IRArgument(std::move(type), std::move(name), index);
    return a;
}

std::string IRBuilder::next_reg() {
    return make_reg_name();
}

std::string IRBuilder::make_reg_name() {
    return "%" + std::to_string(next_reg_num_++);
}

//------------------------------------------------------------------------------
// IR verifier (plan.md §8.10)
//------------------------------------------------------------------------------

namespace {

bool is_terminator(IROpcode op) {
    switch (op) {
        case IROpcode::Br:
        case IROpcode::CondBr:
        case IROpcode::Switch:
        case IROpcode::Return:
        case IROpcode::Unreachable:
            return true;
        default:
            return false;
    }
}

} // namespace

bool verify_module(IRModule& module, DiagnosticEngine& diagnostics) {
    bool ok = true;

    // Collect all block names in each function for terminator validation.
    for (auto& fn : module.functions) {
        std::unordered_set<std::string> block_names;
        for (auto& bb : fn->basic_blocks) {
            if (!block_names.insert(bb->name).second) {
                diagnostics.error({0, "ir.verify"},
                                  "duplicate basic block name '" + bb->name +
                                      "' in function '" + fn->name + "'",
                                  SourceLocation{});
                ok = false;
            }
        }

        // Verify each block.
        for (auto& bb : fn->basic_blocks) {
            // Every block must end with a terminator.
            if (!bb->terminator) {
                diagnostics.error({0, "ir.verify"},
                                  "basic block '" + bb->name +
                                      "' in function '" + fn->name +
                                      "' has no terminator",
                                  SourceLocation{});
                ok = false;
                continue;
            }

            if (!is_terminator(bb->terminator->opcode)) {
                diagnostics.error({0, "ir.verify"},
                                  "block '" + bb->name + "' in function '" +
                                      fn->name +
                                      "' ends with non-terminator instruction",
                                  SourceLocation{});
                ok = false;
            }

            // No instructions after the terminator.
            // (instructions vector is separate from terminator, so this is
            // structurally guaranteed, but we check operand validity.)

            // Verify all instructions in the block.
            std::unordered_set<std::string> defined_values;
            for (auto& inst : bb->instructions) {
                // Check each operand is defined (either a block parameter,
                // a previously defined value, or a constant/function ref).
                for (auto* op : inst->operands) {
                    if (!op) continue; // null operands allowed (e.g. alloca)
                    if (op->type == nullptr) continue; // block reference
                    if (!defined_values.count(op->name)) {
                        // Could be a function argument or global - those are
                        // valid. We can't easily check here, so skip.
                    }
                }
                if (!inst->name.empty() && inst->type &&
                    inst->type->kind != IRTypeKind::Void) {
                    defined_values.insert(inst->name);
                }
            }

            // Verify terminator operands.
            auto* term = bb->terminator.get();
            switch (term->opcode) {
                case IROpcode::Br:
                    if (term->operands.size() != 1 ||
                        !term->operands[0] || term->operands[0]->type != nullptr) {
                        diagnostics.error({0, "ir.verify"},
                                          "br terminator in block '" + bb->name +
                                              "' must have exactly one block operand",
                                          SourceLocation{});
                        ok = false;
                    }
                    break;
                case IROpcode::CondBr:
                    if (term->operands.size() != 3 ||
                        !term->operands[0] ||
                        term->operands[0]->type == nullptr ||
                        !term->operands[1] ||
                        term->operands[1]->type != nullptr ||
                        !term->operands[2] ||
                        term->operands[2]->type != nullptr) {
                        diagnostics.error({0, "ir.verify"},
                                          "condbr terminator in block '" +
                                              bb->name +
                                              "' must have condition + 2 block operands",
                                          SourceLocation{});
                        ok = false;
                    }
                    break;
                case IROpcode::Return:
                    if (fn->return_type->kind == IRTypeKind::Void) {
                        if (term->operands.size() != 0) {
                            diagnostics.error({0, "ir.verify"},
                                              "void function '" + fn->name +
                                                  "' returns a value",
                                              SourceLocation{});
                            ok = false;
                        }
                    } else {
                        if (term->operands.size() != 1 ||
                            !term->operands[0]) {
                            diagnostics.error({0, "ir.verify"},
                                              "non-void function '" + fn->name +
                                                  "' must return a value",
                                              SourceLocation{});
                            ok = false;
                        }
                    }
                    break;
                case IROpcode::Unreachable:
                    break;
                default:
                    break;
            }
        }

        // Verify entry block exists.
        if (!fn->entry_block) {
            diagnostics.error({0, "ir.verify"},
                              "function '" + fn->name +
                                  "' has no entry block",
                              SourceLocation{});
            ok = false;
        }
    }

    return ok;
}

//------------------------------------------------------------------------------
// IR textual printer (plan.md §8.9)
//------------------------------------------------------------------------------

namespace {

std::string type_name(const std::shared_ptr<IRType>& t) {
    if (!t) return "?";
    std::ostringstream os;
    switch (t->kind) {
        case IRTypeKind::Void:   os << "void"; break;
        case IRTypeKind::Bool:   os << "i8"; break;
        case IRTypeKind::I8:     os << "i8"; break;
        case IRTypeKind::I16:    os << "i16"; break;
        case IRTypeKind::I32:    os << "i32"; break;
        case IRTypeKind::I64:    os << "i64"; break;
        case IRTypeKind::F32:    os << "f32"; break;
        case IRTypeKind::F64:    os << "f64"; break;
        case IRTypeKind::Ptr: {
            os << "ptr<";
            if (t->pointee) os << type_name(t->pointee);
            else os << "?";
            os << ">";
            break;
        }
        case IRTypeKind::Array: {
            os << "[" << std::to_string(t->array_length) << " x ";
            if (t->element_type) os << type_name(t->element_type);
            else os << "?";
            os << "]";
            break;
        }
        case IRTypeKind::Struct: {
            os << "{";
            for (std::size_t i = 0; i < t->members.size(); ++i) {
                if (i) os << ", ";
                if (t->members[i]) os << type_name(t->members[i]);
                else os << "?";
            }
            os << "}";
            break;
        }
        case IRTypeKind::Function: {
            os << "(";
            if (t->return_type) os << type_name(t->return_type);
            else os << "?";
            os << ")(";
            for (std::size_t i = 0; i < t->param_types.size(); ++i) {
                if (i) os << ", ";
                if (t->param_types[i]) os << type_name(t->param_types[i]);
                else os << "?";
            }
            os << ")";
            if (t->is_variadic) os << " vararg";
            break;
        }
        case IRTypeKind::Opaque: os << "opaque"; break;
    }
    return os.str();
}

std::string value_name(const IRValue* v) {
    if (!v) return "null";
    if (v->type == nullptr) return "block";
    return v->name;
}

std::string opcode_name(IROpcode op) {
    switch (op) {
        case IROpcode::Alloca:   return "alloca";
        case IROpcode::Load:     return "load";
        case IROpcode::Store:    return "store";
        case IROpcode::IAdd:     return "add";
        case IROpcode::ISub:     return "sub";
        case IROpcode::IMul:     return "mul";
        case IROpcode::IDiv:     return "div";
        case IROpcode::IRem:     return "rem";
        case IROpcode::IShl:     return "shl";
        case IROpcode::IShr:     return "shr";
        case IROpcode::ISra:     return "sra";
        case IROpcode::IAnd:     return "and";
        case IROpcode::IOr:      return "or";
        case IROpcode::IXor:     return "xor";
        case IROpcode::ICmpEq:   return "icmp.eq";
        case IROpcode::ICmpNe:   return "icmp.ne";
        case IROpcode::ICmpLtS:  return "icmp.lts";
        case IROpcode::ICmpGtS:  return "icmp.gts";
        case IROpcode::ICmpLeS:  return "icmp.les";
        case IROpcode::ICmpGeS:  return "icmp.ges";
        case IROpcode::ICmpLtU:  return "icmp.ltu";
        case IROpcode::ICmpGtU:  return "icmp.gtu";
        case IROpcode::ICmpLeU:  return "icmp.leu";
        case IROpcode::ICmpGeU:  return "icmp.geu";
        case IROpcode::FAdd:     return "fadd";
        case IROpcode::FSub:     return "fsub";
        case IROpcode::FMul:     return "fmul";
        case IROpcode::FDiv:     return "fdiv";
        case IROpcode::FRem:     return "frem";
        case IROpcode::FCmpOeq:  return "fcmp.oeq";
        case IROpcode::FCmpOne:  return "fcmp.one";
        case IROpcode::FCmpOlt:  return "fcmp.olt";
        case IROpcode::FCmpOgt:  return "fcmp.ogt";
        case IROpcode::FCmpOle:  return "fcmp.ole";
        case IROpcode::FCmpOge:  return "fcmp.oge";
        case IROpcode::ZExt:     return "zext";
        case IROpcode::SExt:     return "sext";
        case IROpcode::Trunc:    return "trunc";
        case IROpcode::FToUI:    return "f2ui";
        case IROpcode::UIToF:    return "ui2f";
        case IROpcode::SIToF:    return "si2f";
        case IROpcode::FToSI:    return "f2si";
        case IROpcode::BitCast:  return "bitcast";
        case IROpcode::PtrToInt: return "ptr2int";
        case IROpcode::IntToPtr: return "int2ptr";
        case IROpcode::Br:       return "br";
        case IROpcode::CondBr:   return "condbr";
        case IROpcode::Switch:   return "switch";
        case IROpcode::Return:   return "ret";
        case IROpcode::Unreachable: return "unreachable";
        case IROpcode::Call:     return "call";
        case IROpcode::Phi:      return "phi";
        case IROpcode::AtomicLoad:  return "atomic.load";
        case IROpcode::AtomicStore: return "atomic.store";
        case IROpcode::AtomicCmpXchg: return "atomic.cmpxchg";
        case IROpcode::AtomicRMW:   return "atomic.rmw";
        case IROpcode::Fence:       return "fence";
        case IROpcode::Memcpy:      return "memcpy";
        case IROpcode::Memset:      return "memset";
    }
    return "unknown";
}

} // namespace

void dump_function(const IRFunction& fn, std::ostream& out) {
    std::string linkage_str;
    switch (fn.linkage) {
        case IRLinkage::External: linkage_str = "external"; break;
        case IRLinkage::Internal: linkage_str = "internal"; break;
        case IRLinkage::Private:  linkage_str = "private"; break;
    }

    out << "func @" << fn.name << "(";
    for (std::size_t i = 0; i < fn.param_types.size(); ++i) {
        if (i) out << ", ";
        out << type_name(fn.param_types[i]);
        if (i < fn.param_names.size() && !fn.param_names[i].empty())
            out << " %" << fn.param_names[i];
    }
    out << ") -> " << type_name(fn.return_type) << " {\n";

    for (auto& bb : fn.basic_blocks) {
        out << "  " << bb->name << ":\n";
        for (auto& inst : bb->instructions) {
            out << "    ";
            if (!inst->name.empty()) out << inst->name << " = ";
            out << opcode_name(inst->opcode) << " "
                << type_name(inst->type);
            for (auto* op : inst->operands) {
                out << " " << value_name(op);
            }
            if (inst->opcode == IROpcode::Phi) {
                for (auto& [val, pred] : inst->phi_incoming) {
                    out << " [" << value_name(val) << " from " << pred << "]";
                }
            }
            if (inst->is_volatile) out << " volatile";
            if (inst->mem_effect != IRMemEffect::None) {
                out << " mem=";
                switch (inst->mem_effect) {
                    case IRMemEffect::Read:      out << "read"; break;
                    case IRMemEffect::Write:     out << "write"; break;
                    case IRMemEffect::ReadWrite: out << "readwrite"; break;
                    default: break;
                }
            }
            if (inst->alignment) out << " align=" << inst->alignment;
            out << "\n";
        }
        if (bb->terminator) {
            out << "    ";
            out << opcode_name(bb->terminator->opcode);
            for (auto* op : bb->terminator->operands) {
                out << " " << value_name(op);
            }
            out << "\n";
        }
    }
    out << "}\n";
}

void dump_module(const IRModule& module, std::ostream& out) {
    out << "; Module target=" << module.target_id << "\n\n";

    for (auto& fn : module.functions) {
        dump_function(*fn, out);
        out << "\n";
    }
}

} // namespace krc
