#include "irgen.h"

#include "diagnostics.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace krc {

IRGen::IRGen(IRModule& module, DiagnosticEngine& diagnostics)
    : module_(module), diagnostics_(diagnostics), builder_(module) {}

bool IRGen::lower(const TranslationUnit& tu) {
    for (const auto& decl : tu.declarations) {
        if (!lower_declaration(decl)) {
            return false;
        }
    }
    return true;
}

std::shared_ptr<IRType> IRGen::lower_type(const std::shared_ptr<Type>& ast_type) {
    if (!ast_type) return ir_void_type();

    switch (ast_type->kind) {
        case TypeKind::Void:   return ir_void_type();
        case TypeKind::Bool:   return ir_bool_type();
        case TypeKind::Char:   return ir_i8_type();
        case TypeKind::Short:  return ir_i16_type();
        case TypeKind::Int:    return ir_i32_type();
        case TypeKind::Long:   return ir_i32_type();   // KR32: long is 32-bit
        case TypeKind::LongLong: return ir_i64_type();
        case TypeKind::Float:  return ir_f32_type();
        case TypeKind::Double: return ir_f64_type();
        case TypeKind::LongDouble: return ir_f64_type();
        case TypeKind::Pointer:
            return ir_ptr_type(lower_type(ast_type->pointee));
        case TypeKind::Reference:
            // References lower to pointers in the IR.
            return ir_ptr_type(lower_type(ast_type->pointee));
        case TypeKind::RvalueReference:
            // Rvalue references lower to pointers in the IR.
            return ir_ptr_type(lower_type(ast_type->pointee));
        case TypeKind::InitializerList:
            // initializer_list<T> lowers to a pointer to the element type
            // (as a begin/end pair abstraction).
            return ir_ptr_type(lower_type(ast_type->pointee));
        case TypeKind::Array:
            return ir_array_type(lower_type(ast_type->element_type),
                                 ast_type->array_length);
        case TypeKind::Function: {
            std::vector<std::shared_ptr<IRType>> param_ir_types;
            param_ir_types.reserve(ast_type->param_types.size());
            for (const auto& p : ast_type->param_types) {
                param_ir_types.push_back(lower_type(p));
            }
            return ir_function_type(lower_type(ast_type->return_type),
                                    param_ir_types,
                                    ast_type->is_variadic);
        }
        case TypeKind::Struct: {
            std::vector<std::shared_ptr<IRType>> member_ir_types;
            member_ir_types.reserve(ast_type->members.size());
            for (const auto& m : ast_type->members) {
                member_ir_types.push_back(lower_type(m->type));
            }
            return ir_struct_type(member_ir_types);
        }
        case TypeKind::Enum:
        case TypeKind::Typedef:
        case TypeKind::Incomplete:
        case TypeKind::TemplateParam:
        case TypeKind::Auto:
        case TypeKind::Decltype:
        case TypeKind::Union: {
            auto t = std::make_shared<IRType>();
            t->kind = IRTypeKind::Opaque;
            return t;
        }
    }
    return ir_void_type();
}

bool IRGen::lower_declaration(const std::shared_ptr<Declaration>& decl) {
    if (decl->function_def) {
        return lower_function_def(decl->function_def);
    }

    // Non-function declarations: register the symbol, handle initializer.
    for (const auto& declarator : decl->declarators) {
        std::shared_ptr<IRType> ir_type = lower_type(decl->type);
        if (declarator->is_pointer) {
            ir_type = ir_ptr_type(ir_type);
        }
        for (auto& [size, is_vla] : declarator->array_sizes) {
            ir_type = ir_array_type(ir_type, size);
        }

        IRValue* value = nullptr;
        if (decl->storage == StorageClass::Static ||
            decl->storage == StorageClass::Extern) {
            // Global variable.
            IRGlobal global;
            global.name = declarator->name;
            global.type = ir_type;
            global.linkage = (decl->linkage == Linkage::External)
                ? IRLinkage::External : IRLinkage::Internal;
            global.is_constant = decl->is_const;
            module_.globals.push_back(global);
            value = new IRValue(ir_type, declarator->name);
            global_symbols_[declarator->name] = value;
        } else {
            // Local variable: allocate stack slot via alloca if inside a
            // function body, otherwise register as a placeholder.
            if (current_fn_) {
                // Create alloca for the local variable.
                IRInstruction* alloca = builder_.emit_alloca(ir_type,
                                                             declarator->name);
                symbol_map_[declarator->name] = alloca;

                // Emit initializer store if present.
                if (!decl->initializers.empty()) {
                    IRValue* init_val = lower_expression(
                        decl->initializers[0]->expr);
                    if (init_val) {
                        builder_.emit_store(init_val, alloca, false,
                                            decl->location);
                    }
                }

                // Don't also add to symbol_map_ as a plain IRValue below.
                continue;
            }
            // At file scope (non-static, non-extern): placeholder.
            value = new IRValue(ir_type, declarator->name);
        }

        symbol_map_[declarator->name] = value;

        // Handle initializer.
        if (!decl->initializers.empty()) {
            // Initializer will be emitted when the function body is lowered.
        }
    }

    // M6: Emit vtable global for structs with virtual functions.
    if (decl->type && decl->type->kind == TypeKind::Struct && decl->type->has_vtable) {
        emit_vtable_global(decl->type);
    }

    return true;
}

bool IRGen::emit_vtable_global(const std::shared_ptr<Type>& struct_type) {
    // Build vtable name from the struct name.
    std::string vtable_name = "__vtable_" + std::string(struct_type->name);

    // Don't emit duplicate vtables.
    if (vtable_emitted_.count(vtable_name)) {
        return true;
    }
    vtable_emitted_.insert(vtable_name);

    // vtable entry type = pointer to void (function pointers).
    auto ptr_type = ir_ptr_type(ir_void_type());

    // Build array type for the vtable.
    auto vtable_arr_type = ir_array_type(ptr_type, struct_type->vtable_size);

    // Create the vtable global.
    IRGlobal global;
    global.name = vtable_name;
    global.type = vtable_arr_type;
    global.linkage = IRLinkage::External;
    global.is_constant = true;
    module_.globals.push_back(global);

    // Register the vtable as a symbol so it can be referenced.
    global_symbols_[vtable_name] = new IRValue(vtable_arr_type, vtable_name);

    // Collect the virtual function names for the vtable initializer.
    for (std::size_t i = 0; i < struct_type->virtual_functions.size(); ++i) {
        auto& vf = struct_type->virtual_functions[i];
        std::string fn_name;
        if (!vf->declarators.empty()) {
            fn_name = vf->declarators[0]->name;
        }
        if (!fn_name.empty()) {
            vtable_entries_[vtable_name].push_back(fn_name);
        }
    }

    return true;
}

bool IRGen::lower_function_def(const std::shared_ptr<FunctionDef>& fdef) {
    const auto& decl = fdef->declaration;
    if (!decl) return false;

    // decl->type is the return type (Sema stores it as the base type,
    // not a Function type).  Parameter types default to i32 (int),
    // matching the Sema's treatment in check_function_definition.
    std::shared_ptr<IRType> return_type = lower_type(decl->type);

    std::vector<std::shared_ptr<IRType>> param_types;
    std::vector<std::string> param_names;

    // Collect parameter names and types from declarators.
    for (const auto& declarator : decl->declarators) {
        for (const auto& name : declarator->param_names) {
            param_names.push_back(name);
            param_types.push_back(ir_i32_type());
        }
    }

    std::string fn_name;
    if (!decl->declarators.empty()) {
        fn_name = decl->declarators[0]->name;
    }

    builder_.begin_function(fn_name, IRLinkage::External,
                            return_type, param_types, param_names);

    current_fn_ = builder_.current_function();

    // Create alloca for each parameter and register in symbol map.
    IRBasicBlock* entry = current_fn_->entry_block;
    builder_.set_insert_block(entry);

    for (std::size_t i = 0; i < param_types.size(); ++i) {
        std::string param_name = (i < param_names.size())
            ? param_names[i] : ("arg" + std::to_string(i));
        IRArgument* arg = builder_.create_argument(param_types[i],
                                                   param_name,
                                                   static_cast<std::uint32_t>(i));
        // Alloca space for the parameter value (not a pointer to it).
        IRInstruction* alloca = builder_.emit_alloca(param_types[i],
                                                     param_name);
        // Wrap the argument as an IRValue for the store operand.
        IRValue* arg_val = new IRValue(param_types[i], arg->name);
        builder_.emit_store(arg_val, alloca);
        symbol_map_[param_name] = alloca;
    }

    if (fdef->body) {
        if (!lower_statement(fdef->body)) {
            return false;
        }
    }

    builder_.end_function();

    // Register the function as a global symbol so other functions can
    // reference it.
    if (!fn_name.empty()) {
        auto fn_type = ir_function_type(return_type, param_types,
                                        /*is_variadic=*/false);
        global_symbols_[fn_name] = new IRValue(fn_type, fn_name);
    }

    current_fn_ = nullptr;
    symbol_map_.clear();
    return true;
}

bool IRGen::lower_statement(const std::shared_ptr<Statement>& stmt) {
    if (!stmt) return true;

    switch (stmt->kind) {
        case StatementKind::Compound:
            return lower_compound_statement(stmt);
        case StatementKind::If:
            return lower_if_statement(stmt);
        case StatementKind::While:
            return lower_while_statement(stmt);
        case StatementKind::Return:
            return lower_return_statement(stmt);
        case StatementKind::Expression:
            return lower_expression_statement(stmt);
        case StatementKind::Declaration:
            return lower_declaration_statement(stmt);
        case StatementKind::StructuredBinding:
            // Structured bindings lower to individual variable declarations
            // (deferred to backend/lower-level IR). For now, treat as no-op.
            return true;
        case StatementKind::Null:
            return lower_null_statement(stmt);
        case StatementKind::Label:
        case StatementKind::Case:
        case StatementKind::Default:
        case StatementKind::Switch:
        case StatementKind::DoWhile:
        case StatementKind::For:
        case StatementKind::Goto:
        case StatementKind::Continue:
        case StatementKind::Break:
        case StatementKind::Try:
        case StatementKind::Catch:
            diagnostics_.error(
                {0, "irgen-unimplemented"},
                "statement kind not yet supported in IRGen",
                stmt->location);
            return false;
    }
    return true;
}

bool IRGen::lower_compound_statement(const std::shared_ptr<Statement>& stmt) {
    for (const auto& item : stmt->block_items) {
        if (!lower_statement(item)) return false;
    }
    return true;
}

bool IRGen::lower_if_statement(const std::shared_ptr<Statement>& stmt) {
    IRValue* cond = lower_expression(stmt->condition);
    if (!cond) return false;

    IRBasicBlock* then_bb = builder_.create_block("then");
    IRBasicBlock* else_bb = builder_.create_block("else");
    IRBasicBlock* merge_bb = builder_.create_block("merge");

    builder_.emit_cond_br(cond, then_bb, else_bb);

    builder_.set_insert_block(then_bb);
    if (stmt->then_branch) {
        if (!lower_statement(stmt->then_branch)) return false;
    }
    if (!then_bb->terminator) {
        builder_.emit_br(merge_bb);
    }

    builder_.set_insert_block(else_bb);
    if (stmt->else_branch) {
        if (!lower_statement(stmt->else_branch)) return false;
    }
    if (!else_bb->terminator) {
        builder_.emit_br(merge_bb);
    }

    builder_.set_insert_block(merge_bb);
    return true;
}

bool IRGen::lower_while_statement(const std::shared_ptr<Statement>& stmt) {
    IRBasicBlock* cond_bb = builder_.create_block("while.cond");
    IRBasicBlock* body_bb = builder_.create_block("while.body");
    IRBasicBlock* exit_bb = builder_.create_block("while.exit");

    builder_.emit_br(cond_bb);

    builder_.set_insert_block(cond_bb);
    IRValue* cond = lower_expression(stmt->for_condition);
    if (!cond) return false;
    builder_.emit_cond_br(cond, body_bb, exit_bb);

    builder_.set_insert_block(body_bb);
    if (stmt->loop_body) {
        if (!lower_statement(stmt->loop_body)) return false;
    }
    if (!body_bb->terminator) {
        builder_.emit_br(cond_bb);
    }

    builder_.set_insert_block(exit_bb);
    return true;
}

bool IRGen::lower_return_statement(const std::shared_ptr<Statement>& stmt) {
    if (stmt->return_value) {
        IRValue* ret_val = lower_expression(stmt->return_value);
        if (!ret_val) return false;
        builder_.emit_ret(ret_val, stmt->location);
    } else {
        builder_.emit_ret(nullptr, stmt->location);
    }
    return true;
}

bool IRGen::lower_expression_statement(const std::shared_ptr<Statement>& stmt) {
    if (stmt->expression) {
        IRValue* val = lower_expression(stmt->expression);
        (void)val;
    }
    return true;
}

bool IRGen::lower_declaration_statement(const std::shared_ptr<Statement>& stmt) {
    if (!stmt->declaration) return true;
    return lower_declaration(stmt->declaration);
}

bool IRGen::lower_null_statement(const std::shared_ptr<Statement>& stmt) {
    (void)stmt;
    return true;
}

IRValue* IRGen::lower_expression(const std::shared_ptr<Expression>& expr) {
    if (!expr) return nullptr;

    auto it = expr_to_value_.find(expr.get());
    if (it != expr_to_value_.end()) return it->second;

    IRValue* result = nullptr;

    switch (expr->kind) {
        case ExpressionKind::IntegerLiteral:
            result = lower_integer_literal(expr);
            break;
        case ExpressionKind::FloatingLiteral:
            result = lower_floating_literal(expr);
            break;
        case ExpressionKind::Identifier:
            result = lower_identifier(expr);
            break;
        case ExpressionKind::FunctionCall:
            result = lower_function_call(expr);
            break;
        case ExpressionKind::Unary:
            result = lower_unary(expr);
            break;
        case ExpressionKind::Binary:
            result = lower_binary(expr);
            break;
        case ExpressionKind::Conditional:
            result = lower_conditional(expr);
            break;
        case ExpressionKind::Assignment:
            result = lower_assignment(expr);
            break;
        case ExpressionKind::Cast:
            result = lower_cast(expr);
            break;
        case ExpressionKind::SizeofExpr:
        case ExpressionKind::SizeofType:
            result = lower_sizeof(expr);
            break;
        case ExpressionKind::MemberAccess:
            result = lower_member_access(expr);
            break;
        case ExpressionKind::ArraySubscript:
            result = lower_array_subscript(expr);
            break;
        case ExpressionKind::CharLiteral:
        case ExpressionKind::StringLiteral:
        case ExpressionKind::CompoundLiteral:
        case ExpressionKind::StatementExpr:
        case ExpressionKind::StringLiteralConcat:
        case ExpressionKind::This:
        case ExpressionKind::New:
        case ExpressionKind::Delete:
        case ExpressionKind::Lambda:
        case ExpressionKind::Move:
        case ExpressionKind::FoldExpression:
        case ExpressionKind::Throw:
        case ExpressionKind::Typeid:
            diagnostics_.error(
                {0, "irgen-unimplemented"},
                "expression kind not yet supported in IRGen",
                expr->location);
            result = nullptr;
            break;
    }

    if (result) {
        expr_to_value_[expr.get()] = result;
    }
    return result;
}

IRValue* IRGen::lower_integer_literal(const std::shared_ptr<Expression>& expr) {
    std::shared_ptr<IRType> type = lower_type(expr->type);
    IRConstant* c = builder_.create_int_const(type, "", expr->int_value);
    return new IRValue(type, c->name, expr->location);
}

IRValue* IRGen::lower_floating_literal(const std::shared_ptr<Expression>& expr) {
    std::shared_ptr<IRType> type = lower_type(expr->type);
    IRConstant* c = builder_.create_float_const(type, "", expr->float_literal_value);
    return new IRValue(type, c->name, expr->location);
}

IRValue* IRGen::lower_identifier(const std::shared_ptr<Expression>& expr) {
    // Check local symbol map first, then global symbols.
    auto it = symbol_map_.find(expr->identifier_name);
    if (it != symbol_map_.end()) {
        IRValue* ptr = it->second;
        std::shared_ptr<IRType> value_type = lower_type(expr->type);
        return builder_.emit_load(value_type, ptr,
                                  expr->identifier_name, false, expr->location);
    }
    auto git = global_symbols_.find(expr->identifier_name);
    if (git != global_symbols_.end()) {
        IRValue* val = git->second;
        // For function references and pointers, return directly (no load).
        if (val->type->kind == IRTypeKind::Function ||
            val->type->kind == IRTypeKind::Ptr) {
            return val;
        }
        // For global variables, load the value.
        std::shared_ptr<IRType> value_type = lower_type(expr->type);
        return builder_.emit_load(value_type, val,
                                  expr->identifier_name, false, expr->location);
    }
    diagnostics_.error(
        {0, "irgen-undeclared"},
        "undeclared identifier: " + expr->identifier_name,
        expr->location);
    return nullptr;
}

IRValue* IRGen::lower_function_call(const std::shared_ptr<Expression>& expr) {
    IRValue* callee = lower_expression(expr->callee);
    if (!callee) return nullptr;

    std::vector<IRValue*> args;
    args.reserve(expr->call_args.size());
    for (const auto& arg : expr->call_args) {
        IRValue* arg_val = lower_expression(arg);
        if (!arg_val) return nullptr;
        args.push_back(arg_val);
    }

    std::shared_ptr<IRType> result_type = ir_void_type();
    if (expr->type) {
        result_type = lower_type(expr->type);
    }

    return builder_.emit_call(result_type, callee, args, "",
                              expr->location);
}

IRValue* IRGen::lower_unary(const std::shared_ptr<Expression>& expr) {
    IRValue* operand = lower_expression(expr->unary_operand);
    if (!operand) return nullptr;

    std::shared_ptr<IRType> result_type = lower_type(expr->type);

    switch (expr->unary_op) {
        case UnaryOp::UnaryMinus:
        {
            IRConstant* zero = builder_.create_int_const(
                result_type, "", 0);
            IRValue* zero_val = new IRValue(result_type, zero->name);
            return builder_.emit_binop(IROpcode::ISub, result_type,
                                       zero_val, operand, "",
                                       expr->location);
        }
        case UnaryOp::BitwiseNot:
        {
            IRConstant* neg_one = builder_.create_int_const(
                result_type, "", -1);
            IRValue* neg_one_val = new IRValue(result_type, neg_one->name);
            return builder_.emit_binop(IROpcode::IXor, result_type,
                                       neg_one_val, operand, "",
                                       expr->location);
        }
        case UnaryOp::LogicalNot:
        {
            IRConstant* zero = builder_.create_int_const(
                result_type, "", 0);
            IRValue* zero_val = new IRValue(result_type, zero->name);
            return builder_.emit_icmp(IROpcode::ICmpEq, operand, zero_val,
                                      "", expr->location);
        }
        case UnaryOp::AddressOf:
            return operand;
        case UnaryOp::Deref:
            return builder_.emit_load(result_type, operand, "", false,
                                      expr->location);
        case UnaryOp::PreInc:
        case UnaryOp::PreDec:
        case UnaryOp::PostInc:
        case UnaryOp::PostDec:
        case UnaryOp::UnaryPlus:
        case UnaryOp::Sizeof:
        default:
            diagnostics_.error(
                {0, "irgen-unimplemented"},
                "unary operator not yet supported in IRGen",
                expr->location);
            return nullptr;
    }
}

IRValue* IRGen::lower_binary(const std::shared_ptr<Expression>& expr) {
    IRValue* left = lower_expression(expr->binary_left);
    IRValue* right = lower_expression(expr->binary_right);
    if (!left || !right) return nullptr;

    std::shared_ptr<IRType> result_type = lower_type(expr->type);
    bool is_float = (expr->type &&
                     (expr->type->kind == TypeKind::Float ||
                      expr->type->kind == TypeKind::Double));

    switch (expr->binary_op) {
        case BinaryOp::Add:
            return builder_.emit_binop(is_float ? IROpcode::FAdd : IROpcode::IAdd,
                                       result_type, left, right, "",
                                       expr->location);
        case BinaryOp::Sub:
            return builder_.emit_binop(is_float ? IROpcode::FSub : IROpcode::ISub,
                                       result_type, left, right, "",
                                       expr->location);
        case BinaryOp::Mul:
            return builder_.emit_binop(is_float ? IROpcode::FMul : IROpcode::IMul,
                                       result_type, left, right, "",
                                       expr->location);
        case BinaryOp::Div:
            return builder_.emit_binop(is_float ? IROpcode::FDiv : IROpcode::IDiv,
                                       result_type, left, right, "",
                                       expr->location);
        case BinaryOp::Rem:
            return builder_.emit_binop(IROpcode::IRem, result_type,
                                       left, right, "", expr->location);
        case BinaryOp::Eq:
            return builder_.emit_icmp(IROpcode::ICmpEq, left, right, "",
                                      expr->location);
        case BinaryOp::Ne:
            return builder_.emit_icmp(IROpcode::ICmpNe, left, right, "",
                                      expr->location);
        case BinaryOp::Lt:
            return builder_.emit_icmp(is_float ? IROpcode::FCmpOlt : IROpcode::ICmpLtS,
                                      left, right, "", expr->location);
        case BinaryOp::Gt:
            return builder_.emit_icmp(is_float ? IROpcode::FCmpOgt : IROpcode::ICmpGtS,
                                      left, right, "", expr->location);
        case BinaryOp::Le:
            return builder_.emit_icmp(is_float ? IROpcode::FCmpOle : IROpcode::ICmpLeS,
                                      left, right, "", expr->location);
        case BinaryOp::Ge:
            return builder_.emit_icmp(is_float ? IROpcode::FCmpOge : IROpcode::ICmpGeS,
                                      left, right, "", expr->location);
        case BinaryOp::BitAnd:
            return builder_.emit_binop(IROpcode::IAnd, result_type,
                                       left, right, "", expr->location);
        case BinaryOp::BitOr:
            return builder_.emit_binop(IROpcode::IOr, result_type,
                                       left, right, "", expr->location);
        case BinaryOp::BitXor:
            return builder_.emit_binop(IROpcode::IXor, result_type,
                                       left, right, "", expr->location);
        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr:
        case BinaryOp::Shl:
        case BinaryOp::Shr:
        case BinaryOp::Assign:
        case BinaryOp::AddAssign:
        case BinaryOp::SubAssign:
        case BinaryOp::MulAssign:
        case BinaryOp::DivAssign:
        case BinaryOp::RemAssign:
        case BinaryOp::ShlAssign:
        case BinaryOp::ShrAssign:
        case BinaryOp::AndAssign:
        case BinaryOp::XorAssign:
        case BinaryOp::OrAssign:
        case BinaryOp::Comma:
        default:
            diagnostics_.error(
                {0, "irgen-unimplemented"},
                "binary operator not yet supported in IRGen",
                expr->location);
            return nullptr;
    }
}

IRValue* IRGen::lower_conditional(const std::shared_ptr<Expression>& expr) {
    IRValue* cond = lower_expression(expr->cond_condition);
    if (!cond) return nullptr;

    std::shared_ptr<IRType> result_type = lower_type(expr->type);

    IRBasicBlock* true_bb = builder_.create_block("cond.true");
    IRBasicBlock* false_bb = builder_.create_block("cond.false");
    IRBasicBlock* merge_bb = builder_.create_block("cond.merge");

    IRInstruction* phi = builder_.emit_phi(result_type, "cond.result",
                                           expr->location);

    builder_.emit_cond_br(cond, true_bb, false_bb);

    builder_.set_insert_block(true_bb);
    IRValue* true_val = lower_expression(expr->cond_true);
    if (!true_val) return nullptr;
    builder_.add_phi_incoming(phi, true_val, "cond.true");
    if (!true_bb->terminator) {
        builder_.emit_br(merge_bb);
    }

    builder_.set_insert_block(false_bb);
    IRValue* false_val = lower_expression(expr->cond_false);
    if (!false_val) return nullptr;
    builder_.add_phi_incoming(phi, false_val, "cond.false");
    if (!false_bb->terminator) {
        builder_.emit_br(merge_bb);
    }

    builder_.set_insert_block(merge_bb);
    return phi;
}

IRValue* IRGen::lower_assignment(const std::shared_ptr<Expression>& expr) {
    IRValue* right = lower_expression(expr->assign_right);
    if (!right) return nullptr;

    if (expr->assign_left->kind != ExpressionKind::Identifier) {
        diagnostics_.error(
            {0, "irgen-error"},
            "assignment left-hand side must be an identifier",
            expr->location);
        return nullptr;
    }

    auto it = symbol_map_.find(expr->assign_left->identifier_name);
    if (it == symbol_map_.end()) {
        diagnostics_.error(
            {0, "irgen-undeclared"},
            "undeclared identifier: " + expr->assign_left->identifier_name,
            expr->location);
        return nullptr;
    }

    IRValue* ptr = it->second;
    builder_.emit_store(right, ptr, false, expr->location);
    return right;
}

IRValue* IRGen::lower_cast(const std::shared_ptr<Expression>& expr) {
    IRValue* operand = lower_expression(expr->cast_operand);
    if (!operand) return nullptr;

    std::shared_ptr<IRType> target_type = lower_type(expr->cast_type);

    bool to_float = (target_type->kind == IRTypeKind::F32 ||
                     target_type->kind == IRTypeKind::F64);

    if (to_float) {
        // int → float: use signed int to float conversion
        return builder_.emit_sext(target_type, operand, "", expr->location);
    } else {
        // float → int or int to int: bitcast
        return builder_.emit_bitcast(target_type, operand, "", expr->location);
    }
}

IRValue* IRGen::lower_sizeof(const std::shared_ptr<Expression>& expr) {
    std::shared_ptr<IRType> type = ir_i32_type();
    std::uint64_t size = 4; // default

    if (expr->sizeof_type) {
        std::shared_ptr<IRType> ir_t = lower_type(expr->sizeof_type);
        switch (ir_t->kind) {
            case IRTypeKind::Void:   size = 0; break;
            case IRTypeKind::Bool:   size = 1; break;
            case IRTypeKind::I8:     size = 1; break;
            case IRTypeKind::I16:    size = 2; break;
            case IRTypeKind::I32:    size = 4; break;
            case IRTypeKind::I64:    size = 8; break;
            case IRTypeKind::F32:    size = 4; break;
            case IRTypeKind::F64:    size = 8; break;
            case IRTypeKind::Ptr:    size = 4; break;
            default:                 size = 4; break;
        }
    } else if (expr->sizeof_operand) {
        std::shared_ptr<IRType> ir_t = lower_type(expr->sizeof_operand->type);
        switch (ir_t->kind) {
            case IRTypeKind::Void:   size = 0; break;
            case IRTypeKind::Bool:   size = 1; break;
            case IRTypeKind::I8:     size = 1; break;
            case IRTypeKind::I16:    size = 2; break;
            case IRTypeKind::I32:    size = 4; break;
            case IRTypeKind::I64:    size = 8; break;
            case IRTypeKind::F32:    size = 4; break;
            case IRTypeKind::F64:    size = 8; break;
            case IRTypeKind::Ptr:    size = 4; break;
            default:                 size = 4; break;
        }
    }

    IRConstant* c = builder_.create_int_const(type, "", static_cast<std::int64_t>(size));
    return new IRValue(type, c->name, expr->location);
}

IRValue* IRGen::lower_member_access(const std::shared_ptr<Expression>& expr) {
    IRValue* base = lower_expression(expr->member_base);
    if (!base) return nullptr;

    std::shared_ptr<IRType> result_type = lower_type(expr->type);
    return builder_.emit_load(result_type, base, expr->member_name, false,
                              expr->location);
}

IRValue* IRGen::lower_array_subscript(const std::shared_ptr<Expression>& expr) {
    IRValue* base = lower_expression(expr->array_base);
    IRValue* index = lower_expression(expr->array_index);
    if (!base || !index) return nullptr;

    std::shared_ptr<IRType> result_type = lower_type(expr->type);
    return builder_.emit_load(result_type, base, "", false, expr->location);
}

} // namespace krc
