#ifndef KRC_IRGEN_H
#define KRC_IRGEN_H

#include "ast.h"
#include "ir.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace krc {

// Lowers the typed AST (produced by Sema) to typed SSA IR.
// IRGen depends on the front end AST and the IR; it is the bridge in the
// front end → IR → backend dependency chain.
class IRGen {
public:
    IRGen(IRModule& module, DiagnosticEngine& diagnostics);

    // Lower a translation unit to IR. Returns true on success.
    bool lower(const TranslationUnit& tu);

    // Access the resulting module.
    IRModule& module() { return module_; }

private:
    IRModule& module_;
    DiagnosticEngine& diagnostics_;
    IRBuilder builder_;

    // Current function being lowered.
    IRFunction* current_fn_ = nullptr;

    // AST expression → IR value mapping (for SSA values within a function).
    std::unordered_map<const Expression*, IRValue*> expr_to_value_;

    // AST statement → basic block mapping (for control flow).
    std::unordered_map<const Statement*, IRBasicBlock*> stmt_to_block_;

    // Symbol name → IR value (for variables in scope).
    std::unordered_map<std::string, IRValue*> symbol_map_;

    // Global symbols (functions and globals) that persist across functions.
    std::unordered_map<std::string, IRValue*> global_symbols_;

    //------------------------------------------------------------------------------
    // Type lowering
    //------------------------------------------------------------------------------

    std::shared_ptr<IRType> lower_type(const std::shared_ptr<Type>& ast_type);

    //------------------------------------------------------------------------------
    // Declaration lowering
    //------------------------------------------------------------------------------

    bool lower_declaration(const std::shared_ptr<Declaration>& decl);
    bool lower_function_def(const std::shared_ptr<FunctionDef>& fdef);

    //------------------------------------------------------------------------------
    // Statement lowering
    //------------------------------------------------------------------------------

    bool lower_statement(const std::shared_ptr<Statement>& stmt);
    bool lower_compound_statement(const std::shared_ptr<Statement>& stmt);
    bool lower_if_statement(const std::shared_ptr<Statement>& stmt);
    bool lower_while_statement(const std::shared_ptr<Statement>& stmt);
    bool lower_return_statement(const std::shared_ptr<Statement>& stmt);
    bool lower_expression_statement(const std::shared_ptr<Statement>& stmt);
    bool lower_declaration_statement(const std::shared_ptr<Statement>& stmt);
    bool lower_null_statement(const std::shared_ptr<Statement>& stmt);

    //------------------------------------------------------------------------------
    // Expression lowering
    //------------------------------------------------------------------------------

    IRValue* lower_expression(const std::shared_ptr<Expression>& expr);
    IRValue* lower_integer_literal(const std::shared_ptr<Expression>& expr);
    IRValue* lower_floating_literal(const std::shared_ptr<Expression>& expr);
    IRValue* lower_identifier(const std::shared_ptr<Expression>& expr);
    IRValue* lower_function_call(const std::shared_ptr<Expression>& expr);
    IRValue* lower_unary(const std::shared_ptr<Expression>& expr);
    IRValue* lower_binary(const std::shared_ptr<Expression>& expr);
    IRValue* lower_conditional(const std::shared_ptr<Expression>& expr);
    IRValue* lower_assignment(const std::shared_ptr<Expression>& expr);
    IRValue* lower_cast(const std::shared_ptr<Expression>& expr);
    IRValue* lower_sizeof(const std::shared_ptr<Expression>& expr);
    IRValue* lower_member_access(const std::shared_ptr<Expression>& expr);
    IRValue* lower_array_subscript(const std::shared_ptr<Expression>& expr);
};

} // namespace krc

#endif // KRC_IRGEN_H
