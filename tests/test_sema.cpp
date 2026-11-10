#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "sema.h"
#include "ast.h"
#include "diagnostics.h"
#include "source_manager.h"

#include <doctest.h>

#include <memory>
#include <string>
#include <string_view>

using namespace krc;

TEST_CASE("Sema builtin types") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    CHECK(sema.void_type()->kind == TypeKind::Void);
    CHECK(sema.bool_type()->kind == TypeKind::Bool);
    CHECK(sema.char_type()->kind == TypeKind::Char);
    CHECK(sema.short_type()->kind == TypeKind::Short);
    CHECK(sema.int_type()->kind == TypeKind::Int);
    CHECK(sema.long_type()->kind == TypeKind::Long);
    CHECK(sema.long_long_type()->kind == TypeKind::LongLong);
    CHECK(sema.float_type()->kind == TypeKind::Float);
    CHECK(sema.double_type()->kind == TypeKind::Double);

    // KR32 is ILP32: int and long are both 32-bit.
    CHECK(sema.int_type()->size == 4);
    CHECK(sema.long_type()->size == 4);
    CHECK(sema.long_long_type()->size == 8);
    CHECK(sema.float_type()->size == 4);
    CHECK(sema.double_type()->size == 8);
}

TEST_CASE("Scope lookup and insert") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Insert into file scope.
    auto sym = std::make_shared<Symbol>();
    sym->name = "x";
    sym->type = sema.int_type();
    CHECK(sema.insert_symbol("x", sym));
    CHECK(sema.lookup("x") == sym);

    // Cannot insert duplicate in same scope.
    auto sym2 = std::make_shared<Symbol>();
    sym2->name = "x";
    sym2->type = sema.int_type();
    CHECK(!sema.insert_symbol("x", sym2));

    // Lookup in nested scope finds parent scope symbols.
    sema.push_scope();
    CHECK(sema.lookup("x") == sym);
    sema.pop_scope();
}

TEST_CASE("Shadowing in nested scopes") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto sym_x = std::make_shared<Symbol>();
    sym_x->name = "x";
    sym_x->type = sema.int_type();
    CHECK(sema.insert_symbol("x", sym_x));

    sema.push_scope();
    auto sym_y = std::make_shared<Symbol>();
    sym_y->name = "x";
    sym_y->type = sema.char_type();
    CHECK(sema.insert_symbol("x", sym_y));
    // Inner scope lookup finds inner symbol.
    CHECK(sema.lookup("x") == sym_y);
    sema.pop_scope();
    // Back in outer scope, finds outer symbol.
    CHECK(sema.lookup("x") == sym_x);
}

TEST_CASE("Type classification") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    CHECK(sema.is_integer_type(sema.int_type()));
    CHECK(sema.is_integer_type(sema.char_type()));
    CHECK(sema.is_integer_type(sema.long_long_type()));
    CHECK(!sema.is_integer_type(sema.float_type()));
    CHECK(!sema.is_integer_type(sema.void_type()));

    CHECK(sema.is_floating_type(sema.float_type()));
    CHECK(sema.is_floating_type(sema.double_type()));
    CHECK(!sema.is_floating_type(sema.int_type()));

    CHECK(sema.is_arithmetic_type(sema.int_type()));
    CHECK(sema.is_arithmetic_type(sema.double_type()));
    CHECK(!sema.is_arithmetic_type(sema.void_type()));

    auto ptr = sema.pointer_to(sema.int_type());
    CHECK(sema.is_pointer_type(ptr));
    CHECK(sema.is_scalar_type(ptr));

    CHECK(sema.is_void_type(sema.void_type()));
    CHECK(!sema.is_void_type(sema.int_type()));
}

TEST_CASE("Type compatibility") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Same canonical types are compatible.
    CHECK(sema.types_compatible(sema.int_type(), sema.int_type()));
    CHECK(sema.types_compatible(sema.char_type(), sema.char_type()));
    CHECK(sema.types_compatible(sema.void_type(), sema.void_type()));

    // Different kinds are not compatible.
    CHECK(!sema.types_compatible(sema.int_type(), sema.char_type()));
    CHECK(!sema.types_compatible(sema.int_type(), sema.void_type()));

    // Pointer compatibility depends on pointee.
    auto int_ptr = sema.pointer_to(sema.int_type());
    auto char_ptr = sema.pointer_to(sema.char_type());
    auto int_ptr2 = sema.pointer_to(sema.int_type());
    CHECK(sema.types_compatible(int_ptr, int_ptr2));
    CHECK(!sema.types_compatible(int_ptr, char_ptr));

    // Array compatibility depends on element type and length.
    auto arr10 = sema.array_of(sema.int_type(), 10);
    auto arr10b = sema.array_of(sema.int_type(), 10);
    auto arr20 = sema.array_of(sema.int_type(), 20);
    CHECK(sema.types_compatible(arr10, arr10b));
    CHECK(!sema.types_compatible(arr10, arr20));
}

TEST_CASE("Integer promotion") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Narrow types promote to int.
    CHECK(sema.integer_promotion(sema.char_type()) == sema.int_type());
    CHECK(sema.integer_promotion(sema.short_type()) == sema.int_type());
    CHECK(sema.integer_promotion(sema.bool_type()) == sema.int_type());

    // int and wider stay the same.
    CHECK(sema.integer_promotion(sema.int_type()) == sema.int_type());
    CHECK(sema.integer_promotion(sema.long_type()) == sema.long_type());
    CHECK(sema.integer_promotion(sema.long_long_type()) ==
          sema.long_long_type());
}

TEST_CASE("Usual arithmetic conversions") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // int + int -> int
    CHECK(sema.usual_arithmetic_conversions(sema.int_type(), sema.int_type()) ==
          sema.int_type());

    // float + float -> float
    CHECK(sema.usual_arithmetic_conversions(sema.float_type(), sema.float_type()) ==
          sema.float_type());

    // double + double -> double
    CHECK(sema.usual_arithmetic_conversions(sema.double_type(), sema.double_type()) ==
          sema.double_type());

    // int + double -> double
    CHECK(sema.usual_arithmetic_conversions(sema.int_type(), sema.double_type()) ==
          sema.double_type());
    CHECK(sema.usual_arithmetic_conversions(sema.double_type(), sema.int_type()) ==
          sema.double_type());

    // int + float -> float
    CHECK(sema.usual_arithmetic_conversions(sema.int_type(), sema.float_type()) ==
          sema.float_type());

    // long long + int -> long long
    CHECK(
        sema.usual_arithmetic_conversions(sema.long_long_type(), sema.int_type()) ==
        sema.long_long_type());
}

TEST_CASE("Array-to-pointer decay") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto arr = sema.array_of(sema.int_type(), 10);
    auto ptr = sema.array_to_pointer_decay(arr);

    CHECK(sema.is_pointer_type(ptr));
    CHECK(ptr->pointee == sema.int_type());
    CHECK(sema.array_to_pointer_decay(sema.int_type()) == sema.int_type());
}

TEST_CASE("Function-to-pointer decay") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto fntype = sema.function_type(sema.int_type(), {}, false);
    auto ptr = sema.function_to_pointer_decay(fntype);

    CHECK(sema.is_pointer_type(ptr));
    CHECK(ptr->pointee == fntype);
    CHECK(sema.function_to_pointer_decay(sema.int_type()) == sema.int_type());
}

TEST_CASE("Sema end-to-end: simple translation unit") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Sema sema(diagnostics);

    // Build a minimal AST: int main(void) { return 0; }
    auto tu = std::make_shared<TranslationUnit>();

    auto main_type = std::make_shared<Type>();
    main_type->kind = TypeKind::Function;
    main_type->return_type = sema.int_type();

    auto main_decl = std::make_shared<Declaration>();
    main_decl->type = main_type;
    main_decl->linkage = Linkage::External;

    auto main_dec = std::make_shared<Declarator>();
    main_dec->name = "main";
    main_dec->is_function = true;

    auto ret_expr = std::make_shared<Expression>();
    ret_expr->kind = ExpressionKind::IntegerLiteral;
    ret_expr->int_value = 0;
    ret_expr->type = sema.int_type();
    ret_expr->is_constant = true;
    ret_expr->is_lvalue = false;

    auto ret_stmt = std::make_shared<Statement>();
    ret_stmt->kind = StatementKind::Return;
    ret_stmt->return_value = ret_expr;

    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    body->block_items.push_back(ret_stmt);

    auto fdef = std::make_shared<FunctionDef>();
    fdef->declaration = main_decl;
    fdef->body = body;

    main_decl->function_def = fdef;
    main_decl->declarators.push_back(main_dec);

    tu->declarations.push_back(main_decl);

    CHECK(sema.analyze(*tu));
    CHECK_FALSE(diagnostics.has_errors());
}

TEST_CASE("Sema end-to-end: undeclared identifier") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Sema sema(diagnostics);

    auto tu = std::make_shared<TranslationUnit>();

    auto main_type = std::make_shared<Type>();
    main_type->kind = TypeKind::Function;
    main_type->return_type = sema.int_type();

    auto main_decl = std::make_shared<Declaration>();
    main_decl->type = main_type;
    main_decl->linkage = Linkage::External;

    auto main_dec = std::make_shared<Declarator>();
    main_dec->name = "main";
    main_dec->is_function = true;

    auto id_expr = std::make_shared<Expression>();
    id_expr->kind = ExpressionKind::Identifier;
    id_expr->identifier_name = "undefined_var";
    id_expr->location = SourceLocation{};

    auto ret_stmt = std::make_shared<Statement>();
    ret_stmt->kind = StatementKind::Return;
    ret_stmt->return_value = id_expr;

    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    body->block_items.push_back(ret_stmt);

    auto fdef = std::make_shared<FunctionDef>();
    fdef->declaration = main_decl;
    fdef->body = body;

    main_decl->function_def = fdef;
    main_decl->declarators.push_back(main_dec);

    tu->declarations.push_back(main_decl);

    CHECK_FALSE(sema.analyze(*tu));
    CHECK(diagnostics.has_errors());
}
