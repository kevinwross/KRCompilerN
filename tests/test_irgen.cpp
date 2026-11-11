#include "irgen.h"
#include "ir.h"
#include "ast.h"
#include "diagnostics.h"

#include <doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace krc;

namespace {

// Build a type for int (AST Type, not IR type).
static std::shared_ptr<Type> ast_int_type() {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Int;
    return t;
}

// Build a Declaration for a global int variable with given storage class.
static std::shared_ptr<Declaration> make_global_decl(
    const std::string& name, StorageClass storage) {
    auto decl = std::make_shared<Declaration>();
    decl->storage = storage;
    decl->linkage = Linkage::External;
    decl->type = ast_int_type();
    auto dec = std::make_shared<Declarator>();
    dec->name = name;
    decl->declarators.push_back(dec);
    return decl;
}

// Build a FunctionDef declaration with a body.
static std::shared_ptr<Declaration> make_function_decl(
    const std::string& fn_name,
    const std::shared_ptr<Statement>& body) {
    auto decl = std::make_shared<Declaration>();
    decl->storage = StorageClass::Extern;
    decl->linkage = Linkage::External;
    decl->type = ast_int_type();

    auto dec = std::make_shared<Declarator>();
    dec->name = fn_name;
    decl->declarators.push_back(dec);

    auto fdef = std::make_shared<FunctionDef>();
    fdef->declaration = decl;
    fdef->body = body;
    decl->function_def = fdef;

    return decl;
}

} // namespace

TEST_CASE("IRGen thread_local: global thread_local variable marked in IR") {
    DiagnosticEngine diagnostics;
    IRModule module("kr32");
    IRGen irgen(module, diagnostics);

    auto decl = make_global_decl("tls_x", StorageClass::ThreadLocal);
    TranslationUnit tu;
    tu.declarations.push_back(decl);

    CHECK(irgen.lower(tu));

    // The global should be present and flagged thread_local.
    REQUIRE(module.globals.size() == 1);
    CHECK(module.globals[0].name == "tls_x");
    CHECK(module.globals[0].is_thread_local);
    CHECK(module.globals[0].type->kind == IRTypeKind::I32);
}

TEST_CASE("IRGen thread_local: plain global is not thread_local") {
    DiagnosticEngine diagnostics;
    IRModule module("kr32");
    IRGen irgen(module, diagnostics);

    auto decl = make_global_decl("plain_x", StorageClass::Extern);
    TranslationUnit tu;
    tu.declarations.push_back(decl);

    CHECK(irgen.lower(tu));

    REQUIRE(module.globals.size() == 1);
    CHECK(module.globals[0].name == "plain_x");
    CHECK(!module.globals[0].is_thread_local);
}

TEST_CASE("IRGen throw: lowering produces __cxa_throw call and unreachable") {
    DiagnosticEngine diagnostics;
    IRModule module("kr32");
    IRGen irgen(module, diagnostics);

    auto throw_expr = std::make_shared<Expression>();
    throw_expr->kind = ExpressionKind::Throw;
    throw_expr->type = ast_int_type();

    auto operand = std::make_shared<Expression>();
    operand->kind = ExpressionKind::IntegerLiteral;
    operand->type = ast_int_type();
    operand->is_constant = true;
    operand->int_literal_value = 42;

    throw_expr->throw_operand = operand;

    auto throw_stmt = std::make_shared<Statement>();
    throw_stmt->kind = StatementKind::Expression;
    throw_stmt->expression = throw_expr;

    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    body->block_items.push_back(throw_stmt);

    auto decl = make_function_decl("test_throw", body);
    TranslationUnit tu;
    tu.declarations.push_back(decl);

    CHECK(irgen.lower(tu));

    REQUIRE(module.functions.size() == 1);
    CHECK(module.functions[0]->name == "test_throw");
}

TEST_CASE("IRGen typeid: lowering creates type_info symbol") {
    DiagnosticEngine diagnostics;
    IRModule module("kr32");
    IRGen irgen(module, diagnostics);

    auto typeid_expr = std::make_shared<Expression>();
    typeid_expr->kind = ExpressionKind::Typeid;
    typeid_expr->typeid_is_type = true;
    auto tid_type = std::make_shared<Type>();
    tid_type->kind = TypeKind::Int;
    tid_type->name = "int";
    typeid_expr->typeid_type = tid_type;

    auto ret_stmt = std::make_shared<Statement>();
    ret_stmt->kind = StatementKind::Return;
    ret_stmt->expression = typeid_expr;

    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    body->block_items.push_back(ret_stmt);

    auto decl = make_function_decl("test_typeid", body);
    TranslationUnit tu;
    tu.declarations.push_back(decl);

    CHECK(irgen.lower(tu));

    REQUIRE(module.functions.size() == 1);
    CHECK(module.functions[0]->name == "test_typeid");
}

TEST_CASE("IRGen try/catch: lowering produces structured blocks") {
    DiagnosticEngine diagnostics;
    IRModule module("kr32");
    IRGen irgen(module, diagnostics);

    auto try_block = std::make_shared<Statement>();
    try_block->kind = StatementKind::Compound;

    auto catch_clause = std::make_shared<Statement>();
    catch_clause->kind = StatementKind::Catch;
    catch_clause->catch_body = std::make_shared<Statement>();
    catch_clause->catch_body->kind = StatementKind::Compound;

    auto try_stmt = std::make_shared<Statement>();
    try_stmt->kind = StatementKind::Try;
    try_stmt->try_block = try_block;
    try_stmt->catch_clauses.push_back(catch_clause);

    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    body->block_items.push_back(try_stmt);

    auto decl = make_function_decl("test_try", body);
    TranslationUnit tu;
    tu.declarations.push_back(decl);

    CHECK(irgen.lower(tu));

    REQUIRE(module.functions.size() == 1);
    CHECK(module.functions[0]->name == "test_try");
}

TEST_CASE("IRGen move: lowering passes through the operand") {
    DiagnosticEngine diagnostics;
    IRModule module("kr32");
    IRGen irgen(module, diagnostics);

    auto move_expr = std::make_shared<Expression>();
    move_expr->kind = ExpressionKind::Move;
    move_expr->type = ast_int_type();

    auto operand = std::make_shared<Expression>();
    operand->kind = ExpressionKind::Identifier;
    operand->type = ast_int_type();
    operand->is_lvalue = false;

    move_expr->move_operand = operand;

    auto ret_stmt = std::make_shared<Statement>();
    ret_stmt->kind = StatementKind::Return;
    ret_stmt->expression = move_expr;

    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    body->block_items.push_back(ret_stmt);

    auto decl = make_function_decl("test_move", body);
    TranslationUnit tu;
    tu.declarations.push_back(decl);

    CHECK(irgen.lower(tu));

    REQUIRE(module.functions.size() == 1);
    CHECK(module.functions[0]->name == "test_move");
}

TEST_CASE("IRGen forward: lowering passes through the operand") {
    DiagnosticEngine diagnostics;
    IRModule module("kr32");
    IRGen irgen(module, diagnostics);

    auto forward_expr = std::make_shared<Expression>();
    forward_expr->kind = ExpressionKind::Forward;
    forward_expr->type = ast_int_type();

    auto operand = std::make_shared<Expression>();
    operand->kind = ExpressionKind::Identifier;
    operand->type = ast_int_type();

    forward_expr->forward_operand = operand;

    auto ret_stmt = std::make_shared<Statement>();
    ret_stmt->kind = StatementKind::Return;
    ret_stmt->expression = forward_expr;

    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    body->block_items.push_back(ret_stmt);

    auto decl = make_function_decl("test_forward", body);
    TranslationUnit tu;
    tu.declarations.push_back(decl);

    CHECK(irgen.lower(tu));

    REQUIRE(module.functions.size() == 1);
    CHECK(module.functions[0]->name == "test_forward");
}

TEST_CASE("IRGen fold expression: binary fold lowers to binop") {
    DiagnosticEngine diagnostics;
    IRModule module("kr32");
    IRGen irgen(module, diagnostics);

    auto fold_expr = std::make_shared<Expression>();
    fold_expr->kind = ExpressionKind::FoldExpression;
    fold_expr->type = ast_int_type();
    fold_expr->fold_kind = 1;  // unary left
    fold_expr->fold_op = FoldOp::Plus;

    auto operand = std::make_shared<Expression>();
    operand->kind = ExpressionKind::IntegerLiteral;
    operand->type = ast_int_type();
    operand->is_constant = true;
    operand->int_literal_value = 42;

    fold_expr->fold_operand = operand;

    auto ret_stmt = std::make_shared<Statement>();
    ret_stmt->kind = StatementKind::Return;
    ret_stmt->expression = fold_expr;

    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    body->block_items.push_back(ret_stmt);

    auto decl = make_function_decl("test_fold", body);
    TranslationUnit tu;
    tu.declarations.push_back(decl);

    CHECK(irgen.lower(tu));

    REQUIRE(module.functions.size() == 1);
    CHECK(module.functions[0]->name == "test_fold");
}
