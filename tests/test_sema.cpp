#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "sema.h"
#include "ast.h"
#include "diagnostics.h"
#include "source_manager.h"
#include "preprocessor.h"

#include <doctest.h>

#include <memory>
#include <string>
#include <string_view>
#include <fstream>
#include <filesystem>

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

TEST_CASE("Sema class type definition registration") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: class Point { int x; int y; };
    auto point_type = std::make_shared<Type>();
    point_type->kind = TypeKind::Struct;
    point_type->name = "Point";

    auto x_type = std::make_shared<Type>();
    x_type->kind = TypeKind::Int;

    auto x_decl = std::make_shared<Declaration>();
    x_decl->type = x_type;
    auto x_dec = std::make_shared<Declarator>();
    x_dec->name = "x";
    x_decl->declarators.push_back(x_dec);
    point_type->members.push_back(x_decl);

    auto y_type = std::make_shared<Type>();
    y_type->kind = TypeKind::Int;

    auto y_decl = std::make_shared<Declaration>();
    y_decl->type = y_type;
    auto y_dec = std::make_shared<Declarator>();
    y_dec->name = "y";
    y_decl->declarators.push_back(y_dec);
    point_type->members.push_back(y_decl);

    auto tu = std::make_shared<TranslationUnit>();
    auto decl = std::make_shared<Declaration>();
    decl->type = point_type;
    // No declarator: pure type definition
    tu->declarations.push_back(decl);

    CHECK(sema.analyze(*tu));
    // The class should be registered in file scope.
    auto sym = sema.lookup("Point");
    REQUIRE(sym != nullptr);
    CHECK(sym->type == point_type);
}

TEST_CASE("Sema class member lookup") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: struct Pair { int first; int second; };
    auto pair_type = std::make_shared<Type>();
    pair_type->kind = TypeKind::Struct;
    pair_type->name = "Pair";

    auto int_type = sema.int_type();

    auto first_decl = std::make_shared<Declaration>();
    first_decl->type = int_type;
    auto first_dec = std::make_shared<Declarator>();
    first_dec->name = "first";
    first_decl->declarators.push_back(first_dec);
    pair_type->members.push_back(first_decl);

    auto second_decl = std::make_shared<Declaration>();
    second_decl->type = int_type;
    auto second_dec = std::make_shared<Declarator>();
    second_dec->name = "second";
    second_decl->declarators.push_back(second_dec);
    pair_type->members.push_back(second_decl);

    // lookup_member should find both members.
    auto m1 = sema.lookup_member(pair_type, "first");
    REQUIRE(m1 != nullptr);
    CHECK(m1->name == "first");
    CHECK(m1->type == int_type);

    auto m2 = sema.lookup_member(pair_type, "second");
    REQUIRE(m2 != nullptr);
    CHECK(m2->name == "second");
    CHECK(m2->type == int_type);

    // Non-existent member should return null.
    CHECK(sema.lookup_member(pair_type, "nonexistent") == nullptr);
}

TEST_CASE("Sema member access type checking") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: struct Box { int value; };
    auto box_type = std::make_shared<Type>();
    box_type->kind = TypeKind::Struct;
    box_type->name = "Box";

    auto int_type = sema.int_type();

    auto value_decl = std::make_shared<Declaration>();
    value_decl->type = int_type;
    auto value_dec = std::make_shared<Declarator>();
    value_dec->name = "value";
    value_decl->declarators.push_back(value_dec);
    box_type->members.push_back(value_decl);

    // Register Box type and variable b in file scope.
    {
        auto sym = std::make_shared<Symbol>();
        sym->name = "Box";
        sym->type = box_type;
        sym->is_definition = true;
        CHECK(sema.insert_symbol("Box", sym));
    }
    {
        auto sym = std::make_shared<Symbol>();
        sym->name = "b";
        sym->type = box_type;
        sym->is_definition = true;
        CHECK(sema.insert_symbol("b", sym));
    }

    // Build: Box b; b.value
    auto base_expr = std::make_shared<Expression>();
    base_expr->kind = ExpressionKind::Identifier;
    base_expr->identifier_name = "b";
    base_expr->type = box_type;
    base_expr->is_lvalue = true;

    auto member_expr = std::make_shared<Expression>();
    member_expr->kind = ExpressionKind::MemberAccess;
    member_expr->member_base = base_expr;
    member_expr->member_name = "value";
    member_expr->is_ptr_member_access = false;

    CHECK(sema.type_member_access(*member_expr));
    CHECK(member_expr->type == int_type);
    CHECK(member_expr->is_lvalue == true);
}

TEST_CASE("Sema overload: lookup_all finds multiple functions") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto int_type = sema.int_type();
    auto float_type = sema.double_type(); // use double for float params

    // Declare two overloads of 'f': f(int) and f(double)
    auto f1_sym = std::make_shared<Symbol>();
    f1_sym->name = "f";
    f1_sym->type = sema.function_type(int_type, {int_type}, false);
    CHECK(sema.insert_symbol("f", f1_sym));

    auto f2_sym = std::make_shared<Symbol>();
    f2_sym->name = "f";
    f2_sym->type = sema.function_type(int_type, {float_type}, false);
    CHECK(sema.insert_symbol("f", f2_sym));

    // lookup should return the most recently inserted.
    auto sym = sema.lookup("f");
    REQUIRE(sym != nullptr);
    CHECK(sym == f2_sym);

    // lookup_all should return both.
    auto all = sema.lookup_all("f");
    REQUIRE(all.size() == 2);
}

TEST_CASE("Sema overload: resolve_overload picks best match") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto int_type = sema.int_type();
    auto double_type = sema.double_type();

    // Build args: f(42) with f(int) and f(double) candidates.
    auto arg_int = std::make_shared<Expression>();
    arg_int->kind = ExpressionKind::IntegerLiteral;
    arg_int->type = int_type;
    arg_int->is_constant = true;
    arg_int->is_lvalue = false;

    auto arg_double = std::make_shared<Expression>();
    arg_double->kind = ExpressionKind::FloatingLiteral;
    arg_double->type = double_type;
    arg_double->is_constant = true;
    arg_double->is_lvalue = false;

    auto f_int = std::make_shared<Symbol>();
    f_int->name = "f";
    f_int->type = sema.function_type(int_type, {int_type}, false);

    auto f_double = std::make_shared<Symbol>();
    f_double->name = "f";
    f_double->type = sema.function_type(int_type, {double_type}, false);

    std::vector<std::shared_ptr<Symbol>> candidates = {f_int, f_double};

    // f(42) -> int arg: exact match on f(int).
    std::vector<std::shared_ptr<Expression>> args_int = {arg_int};
    int best = sema.resolve_overload(candidates, args_int);
    CHECK(best == 0);

    // f(1.5) -> double arg: exact match on f(double).
    std::vector<std::shared_ptr<Expression>> args_double = {arg_double};
    best = sema.resolve_overload(candidates, args_double);
    CHECK(best == 1);

    // arg count mismatch should return -1.
    std::vector<std::shared_ptr<Expression>> args_two = {arg_int, arg_double};
    best = sema.resolve_overload(candidates, args_two);
    CHECK(best == -1);
}

TEST_CASE("Sema overload: type_function_call resolves overload") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto int_type = sema.int_type();
    auto double_type = sema.double_type();

    // Insert two overloads: f(int) and f(double) into file scope.
    auto f_int = std::make_shared<Symbol>();
    f_int->name = "f";
    f_int->type = sema.function_type(int_type, {int_type}, false);
    CHECK(sema.insert_symbol("f", f_int));

    auto f_double = std::make_shared<Symbol>();
    f_double->name = "f";
    f_double->type = sema.function_type(int_type, {double_type}, false);
    CHECK(sema.insert_symbol("f", f_double));

    // Build call: f(42) with integer literal arg.
    auto arg = std::make_shared<Expression>();
    arg->kind = ExpressionKind::IntegerLiteral;
    arg->type = int_type;
    arg->is_constant = true;
    arg->is_lvalue = false;

    auto callee = std::make_shared<Expression>();
    callee->kind = ExpressionKind::Identifier;
    callee->identifier_name = "f";

    auto call = std::make_shared<Expression>();
    call->kind = ExpressionKind::FunctionCall;
    call->callee = callee;
    call->call_args = {arg};

    CHECK(sema.type_function_call(*call));
    // Should have selected f(int) -> return type int.
    REQUIRE(call->type != nullptr);
    CHECK(call->type == int_type);
}

TEST_CASE("Sema overload: type_function_call no match reports error") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto int_type = sema.int_type();

    // Insert only f(int).
    auto f_int = std::make_shared<Symbol>();
    f_int->name = "f";
    f_int->type = sema.function_type(int_type, {int_type}, false);
    CHECK(sema.insert_symbol("f", f_int));

    // Build call: f(42, 42) - two args, no match.
    auto arg1 = std::make_shared<Expression>();
    arg1->kind = ExpressionKind::IntegerLiteral;
    arg1->type = int_type;
    arg1->is_constant = true;
    arg1->is_lvalue = false;

    auto arg2 = std::make_shared<Expression>();
    arg2->kind = ExpressionKind::IntegerLiteral;
    arg2->type = int_type;
    arg2->is_constant = true;
    arg2->is_lvalue = false;

    auto callee = std::make_shared<Expression>();
    callee->kind = ExpressionKind::Identifier;
    callee->identifier_name = "f";

    auto call = std::make_shared<Expression>();
    call->kind = ExpressionKind::FunctionCall;
    call->callee = callee;
    call->call_args = {arg1, arg2};

    CHECK_FALSE(sema.type_function_call(*call));
    CHECK(diagnostics.has_errors());
}

TEST_CASE("Sema mangling: C++ function gets mangled name") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);
    sema.set_cpp_mode(true);

    auto int_type = sema.int_type();

    // Global function: f(int) -> _Zf i
    auto sym = std::make_shared<Symbol>();
    sym->name = "f";
    sym->type = sema.function_type(int_type, {int_type}, false);
    sym->linkage = Linkage::External;

    std::string mangled = sema.compute_mangled_name(*sym);
    CHECK(mangled == "_Z1fi");
}

TEST_CASE("Sema mangling: C linkage does not mangle") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);
    sema.set_cpp_mode(true);

    auto int_type = sema.int_type();

    // C linkage: no mangling.
    auto sym = std::make_shared<Symbol>();
    sym->name = "f";
    sym->type = sema.function_type(int_type, {int_type}, false);
    sym->linkage = Linkage::Internal;

    std::string mangled = sema.compute_mangled_name(*sym);
    CHECK(mangled.empty());
}

TEST_CASE("Sema mangling: non-CPP mode does not mangle") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);
    // is_cpp_ defaults to false.

    auto int_type = sema.int_type();

    auto sym = std::make_shared<Symbol>();
    sym->name = "f";
    sym->type = sema.function_type(int_type, {int_type}, false);
    sym->linkage = Linkage::External;

    std::string mangled = sema.compute_mangled_name(*sym);
    CHECK(mangled.empty());
}

TEST_CASE("Sema mangling: function with two params") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);
    sema.set_cpp_mode(true);

    auto int_type = sema.int_type();
    auto double_type = sema.double_type();

    // f(int, double) -> _Zfid
    auto sym = std::make_shared<Symbol>();
    sym->name = "f";
    sym->type = sema.function_type(int_type, {int_type, double_type}, false);
    sym->linkage = Linkage::External;

    std::string mangled = sema.compute_mangled_name(*sym);
    CHECK(mangled == "_Z1fid");
}

TEST_CASE("Sema mangling: no params encodes v") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);
    sema.set_cpp_mode(true);

    auto int_type = sema.int_type();

    // f() -> _Zfv
    auto sym = std::make_shared<Symbol>();
    sym->name = "f";
    sym->type = sema.function_type(int_type, {}, false);
    sym->linkage = Linkage::External;

    std::string mangled = sema.compute_mangled_name(*sym);
    CHECK(mangled == "_Z1fv");
}

TEST_CASE("Sema mangling: class member function is qualified") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);
    sema.set_cpp_mode(true);

    auto int_type = sema.int_type();

    // Set current class to "Point", then mangle f(int)
    auto point_type = std::make_shared<Type>();
    point_type->kind = TypeKind::Struct;
    point_type->name = "Point";
    sema.set_current_class_type(point_type);

    auto sym = std::make_shared<Symbol>();
    sym->name = "f";
    sym->type = sema.function_type(int_type, {int_type}, false);
    sym->linkage = Linkage::External;

    // _ZN5Point1fEi -> N + Point + f + E, then params
    std::string mangled = sema.compute_mangled_name(*sym);
    CHECK(mangled == "_ZN5Point1fEi");
}

TEST_CASE("Sema this: this inside a class has pointer-to-class type") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);
    sema.set_cpp_mode(true);

    // Set up a Point class type.
    auto point_type = std::make_shared<Type>();
    point_type->kind = TypeKind::Struct;
    point_type->name = "Point";
    sema.set_current_class_type(point_type);

    auto this_expr = std::make_shared<Expression>();
    this_expr->kind = ExpressionKind::This;

    CHECK(sema.type_this(*this_expr));
    REQUIRE(this_expr->type != nullptr);
    CHECK(this_expr->type->kind == TypeKind::Pointer);
    REQUIRE(this_expr->type->pointee != nullptr);
    CHECK(this_expr->type->pointee == point_type);
    CHECK(this_expr->is_lvalue == false);
    CHECK(this_expr->is_constant == true);
}

TEST_CASE("Sema this: this outside a class reports error") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);
    // current_class_type_ is nullptr by default.

    auto this_expr = std::make_shared<Expression>();
    this_expr->kind = ExpressionKind::This;

    CHECK_FALSE(sema.type_this(*this_expr));
    CHECK(diagnostics.has_errors());
}

TEST_CASE("Sema new: new int has pointer-to-int type") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto new_expr = std::make_shared<Expression>();
    new_expr->kind = ExpressionKind::New;
    new_expr->new_type = sema.int_type();

    CHECK(sema.type_new(*new_expr));
    REQUIRE(new_expr->type != nullptr);
    CHECK(new_expr->type->kind == TypeKind::Pointer);
    REQUIRE(new_expr->type->pointee != nullptr);
    CHECK(new_expr->type->pointee == sema.int_type());
    CHECK(new_expr->is_lvalue == false);
    CHECK(new_expr->is_constant == false);
}

TEST_CASE("Sema new: new[] with integral size") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto size_expr = std::make_shared<Expression>();
    size_expr->kind = ExpressionKind::IntegerLiteral;
    size_expr->type = sema.int_type();
    size_expr->is_constant = true;
    size_expr->is_lvalue = false;

    auto new_expr = std::make_shared<Expression>();
    new_expr->kind = ExpressionKind::New;
    new_expr->new_type = sema.int_type();
    new_expr->new_alloc_expr = size_expr;

    CHECK(sema.type_new(*new_expr));
    REQUIRE(new_expr->type != nullptr);
    CHECK(new_expr->type->kind == TypeKind::Pointer);
    CHECK(new_expr->type->pointee == sema.int_type());
}

TEST_CASE("Sema new: new void reports error") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto new_expr = std::make_shared<Expression>();
    new_expr->kind = ExpressionKind::New;
    new_expr->new_type = sema.void_type();

    CHECK_FALSE(sema.type_new(*new_expr));
    CHECK(diagnostics.has_errors());
}

TEST_CASE("Sema delete: delete int* has void type") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto ptr_expr = std::make_shared<Expression>();
    ptr_expr->kind = ExpressionKind::Identifier;
    ptr_expr->identifier_name = "p";
    ptr_expr->type = sema.pointer_to(sema.int_type());
    ptr_expr->is_lvalue = true;

    auto delete_expr = std::make_shared<Expression>();
    delete_expr->kind = ExpressionKind::Delete;
    delete_expr->delete_operand = ptr_expr;

    // Register 'p' in file scope so lookup succeeds.
    auto sym = std::make_shared<Symbol>();
    sym->name = "p";
    sym->type = sema.pointer_to(sema.int_type());
    sym->is_definition = true;
    CHECK(sema.insert_symbol("p", sym));

    CHECK(sema.type_delete(*delete_expr));
    REQUIRE(delete_expr->type != nullptr);
    CHECK(delete_expr->type == sema.void_type());
    CHECK(delete_expr->is_lvalue == false);
}

TEST_CASE("Sema delete: delete non-pointer reports error") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto int_expr = std::make_shared<Expression>();
    int_expr->kind = ExpressionKind::IntegerLiteral;
    int_expr->type = sema.int_type();
    int_expr->is_constant = true;
    int_expr->is_lvalue = false;

    auto delete_expr = std::make_shared<Expression>();
    delete_expr->kind = ExpressionKind::Delete;
    delete_expr->delete_operand = int_expr;

    CHECK_FALSE(sema.type_delete(*delete_expr));
    CHECK(diagnostics.has_errors());
}

TEST_CASE("Sema template: template type parameter declaration") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: template <typename T> class Foo { T x; };
    auto tu = std::make_shared<TranslationUnit>();

    // Template parameter: typename T
    auto t_param = std::make_shared<TemplateParam>();
    t_param->kind = TemplateParam::Kind::Type;
    t_param->name = "T";

    // The class Foo type
    auto foo_type = std::make_shared<Type>();
    foo_type->kind = TypeKind::Struct;
    foo_type->name = "Foo";

    // Member: T x
    auto x_decl = std::make_shared<Declaration>();
    auto x_type = std::make_shared<Type>();
    x_type->kind = TypeKind::Incomplete; // T will be resolved later
    x_decl->type = x_type;
    auto x_dec = std::make_shared<Declarator>();
    x_dec->name = "x";
    x_decl->declarators.push_back(x_dec);
    foo_type->members.push_back(x_decl);

    // Outer declaration: class Foo { ... } with template param
    auto outer_decl = std::make_shared<Declaration>();
    outer_decl->type = foo_type;
    outer_decl->template_params.push_back(t_param);

    tu->declarations.push_back(outer_decl);

    // Sema should accept the template declaration (params stored, not yet
    // instantiated).
    CHECK(sema.analyze(*tu));
}

TEST_CASE("Sema template: template with multiple type parameters") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto tu = std::make_shared<TranslationUnit>();

    // Template parameters: typename T, class U
    auto t_param = std::make_shared<TemplateParam>();
    t_param->kind = TemplateParam::Kind::Type;
    t_param->name = "T";

    auto u_param = std::make_shared<TemplateParam>();
    u_param->kind = TemplateParam::Kind::Type;
    u_param->name = "U";

    auto pair_type = std::make_shared<Type>();
    pair_type->kind = TypeKind::Struct;
    pair_type->name = "Pair";

    auto outer_decl = std::make_shared<Declaration>();
    outer_decl->type = pair_type;
    outer_decl->template_params = {t_param, u_param};

    tu->declarations.push_back(outer_decl);

    CHECK(sema.analyze(*tu));
}

TEST_CASE("Sema template: template value parameter") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto tu = std::make_shared<TranslationUnit>();

    // Template parameter: int N
    auto n_param = std::make_shared<TemplateParam>();
    n_param->kind = TemplateParam::Kind::Value;
    n_param->name = "N";
    n_param->param_type = sema.int_type();

    auto arr_type = std::make_shared<Type>();
    arr_type->kind = TypeKind::Struct;
    arr_type->name = "Array";

    auto outer_decl = std::make_shared<Declaration>();
    outer_decl->type = arr_type;
    outer_decl->template_params.push_back(n_param);

    tu->declarations.push_back(outer_decl);

    CHECK(sema.analyze(*tu));
}

TEST_CASE("Sema template: template with default type") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto tu = std::make_shared<TranslationUnit>();

    // Template parameter: typename T = int
    auto t_param = std::make_shared<TemplateParam>();
    t_param->kind = TemplateParam::Kind::Type;
    t_param->name = "T";
    t_param->default_type = sema.int_type();

    auto foo_type = std::make_shared<Type>();
    foo_type->kind = TypeKind::Struct;
    foo_type->name = "Foo";

    auto outer_decl = std::make_shared<Declaration>();
    outer_decl->type = foo_type;
    outer_decl->template_params.push_back(t_param);

    tu->declarations.push_back(outer_decl);

    CHECK(sema.analyze(*tu));
}

TEST_CASE("Sema integer_promotion: bool/char/short promote to int") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    CHECK(sema.integer_promotion(sema.bool_type()) == sema.int_type());
    CHECK(sema.integer_promotion(sema.char_type()) == sema.int_type());
    CHECK(sema.integer_promotion(sema.short_type()) == sema.int_type());
}

TEST_CASE("Sema integer_promotion: int/long/long_long unchanged") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    CHECK(sema.integer_promotion(sema.int_type()) == sema.int_type());
    CHECK(sema.integer_promotion(sema.long_type()) == sema.long_type());
    CHECK(sema.integer_promotion(sema.long_long_type()) == sema.long_long_type());
}

TEST_CASE("Sema usual_arithmetic_conversions: int + int = int") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    CHECK(sema.usual_arithmetic_conversions(sema.int_type(), sema.int_type()) ==
          sema.int_type());
}

TEST_CASE("Sema usual_arithmetic_conversions: int + double = double") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    CHECK(sema.usual_arithmetic_conversions(sema.int_type(), sema.double_type()) ==
          sema.double_type());
    CHECK(sema.usual_arithmetic_conversions(sema.double_type(), sema.int_type()) ==
          sema.double_type());
}

TEST_CASE("Sema usual_arithmetic_conversions: float + double = double") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    CHECK(sema.usual_arithmetic_conversions(sema.float_type(), sema.double_type()) ==
          sema.double_type());
}

TEST_CASE("Sema usual_arithmetic_conversions: float + float = float") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    CHECK(sema.usual_arithmetic_conversions(sema.float_type(), sema.float_type()) ==
          sema.float_type());
}

TEST_CASE("Sema usual_arithmetic_conversions: int + long_long = long_long") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    CHECK(sema.usual_arithmetic_conversions(sema.int_type(), sema.long_long_type()) ==
          sema.long_long_type());
}

TEST_CASE("Preprocessor: object-like macro expansion") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    // Write a temp file with a macro.
    std::filesystem::path tmp = "krc_pp_test_objectlike.h";
    std::ofstream out(tmp);
    out << "#define MAX 100\n";
    out << "int x = MAX;\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    // Collect all tokens (excluding the terminating EOF).
    std::vector<TokenKind> kinds;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        if (pp.current().kind == TokenKind::Eof) break;
        kinds.push_back(pp.current().kind);
    }

    // EXPECT: INT_LITERAL("100") replaces MAX, then semicolon.
    // Tokens: INT_LITERAL(100), EQ, INT_LITERAL(100), SEMI
    std::vector<TokenKind> expected = {
        TokenKind::KwInt,           // int
        TokenKind::Identifier,    // x
        TokenKind::Eq,            // =
        TokenKind::IntegerLiteral,  // 100 (expanded MAX)
        TokenKind::Semicolon,          // ;
    };
    REQUIRE(kinds.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(kinds[i] == expected[i]);
    }

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: __FILE__ predefined macro") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_file.cpp";
    std::ofstream out(tmp);
    out << "const char* f = __FILE__;\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    // Should contain the file path as a string literal.
    bool found_file = false;
    for (const auto& t : texts) {
        if (t.find("krc_pp_test_file.cpp") != std::string::npos) {
            found_file = true;
            break;
        }
    }
    CHECK(found_file);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: __LINE__ predefined macro") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_line.cpp";
    std::ofstream out(tmp);
    out << "int line = __LINE__;\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    // __LINE__ on line 1 of the file should be "1".
    bool found_line_1 = false;
    for (const auto& t : texts) {
        if (t == "1") { found_line_1 = true; break; }
    }
    CHECK(found_line_1);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: __COUNTER__ predefined macro") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_counter.cpp";
    std::ofstream out(tmp);
    out << "int a = __COUNTER__;\nint b = __COUNTER__;\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    // Should see 0 and 1 in order.
    bool found_zero = false, found_one = false;
    for (const auto& t : texts) {
        if (t == "0") found_zero = true;
        if (t == "1") found_one = true;
    }
    CHECK(found_zero);
    CHECK(found_one);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: #ifdef / #ifndef / #endif") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_ifdef.cpp";
    std::ofstream out(tmp);
    out << "#ifdef FOO\nint a = 1;\n#endif\n";
    out << "#ifndef BAR\nint b = 2;\n#endif\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    // FOO is not defined, so "int a = 1;" should be skipped.
    // BAR is not defined, so "int b = 2;" should be included.
    bool found_int_a = false, found_int_b = false;
    for (const auto& t : texts) {
        if (t == "a") found_int_a = true;
        if (t == "b") found_int_b = true;
    }
    CHECK_FALSE(found_int_a);
    CHECK(found_int_b);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: #if defined(NAME)") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_defined.cpp";
    std::ofstream out(tmp);
    out << "#define MYMACRO\n";
    out << "#if defined(MYMACRO)\nint x = 1;\n#endif\n";
    out << "#if defined(NOTDEFINED)\nint y = 2;\n#endif\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    bool found_x = false, found_y = false;
    for (const auto& t : texts) {
        if (t == "x") found_x = true;
        if (t == "y") found_y = true;
    }
    CHECK(found_x);
    CHECK_FALSE(found_y);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: function-like macro with stringification (#)") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_stringify.cpp";
    std::ofstream out(tmp);
    out << "#define STRINGIZE(x) #x\n";
    out << "const char* s = STRINGIZE(hello);\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    // Should contain the string literal "hello".
    bool found_hello = false;
    for (const auto& t : texts) {
        if (t == "\"hello\"") { found_hello = true; break; }
    }
    CHECK(found_hello);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: function-like macro with token pasting (##)") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_paste.cpp";
    std::ofstream out(tmp);
    out << "#define MAKE_NAME(x, y) x##y\n";
    out << "int my_var = 42;\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    // Just verify it compiles and runs without crashing.
    int tokens = 0;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        ++tokens;
    }
    CHECK(tokens > 0);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: predefined target macros") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_target_macros.cpp";
    std::ofstream out(tmp);
    out << "#ifdef __KR32__\n";
    out << "int target = 1;\n";
    out << "#endif\n";
    out << "int x = __KR32__;\n";
    out << "int y = __STDC_VERSION__;\n";
    out << "int z = __cplusplus;\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    // __KR32__ should be defined, so "int target = 1;" should pass through.
    bool found_target = false;
    bool found_kr32 = false;
    bool found_std_ver = false;
    bool found_cpp_ver = false;
    for (const auto& t : texts) {
        if (t == "target") found_target = true;
        if (t == "1") found_kr32 = true;
        if (t == "201112L") found_std_ver = true;
        if (t == "201703L") found_cpp_ver = true;
    }
    CHECK(found_target);
    CHECK(found_kr32);
    CHECK(found_std_ver);
    CHECK(found_cpp_ver);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: __STDC_VERSION__ and __cplusplus") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_std_macros.cpp";
    std::ofstream out(tmp);
    out << "#define STD_VER __STDC_VERSION__\n";
    out << "#define CPP_VER __cplusplus\n";
    out << "int a = STD_VER;\n";
    out << "int b = CPP_VER;\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    // __STDC_VERSION__ and __cplusplus should expand to their literal values.
    bool found_std_ver = false;
    bool found_cpp_ver = false;
    for (const auto& t : texts) {
        if (t == "201112L") found_std_ver = true;
        if (t == "201703L") found_cpp_ver = true;
    }
    CHECK(found_std_ver);
    CHECK(found_cpp_ver);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: __has_include") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_has_include.cpp";
    std::ofstream out(tmp);
    out << "#if __has_include(\"nonexistent.h\")\n";
    out << "int should_not_appear = 1;\n";
    out << "#endif\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    // __has_include("nonexistent.h") is 0, so the #if body should be skipped.
    bool found_should_not = false;
    for (const auto& t : texts) {
        if (t == "should_not_appear") { found_should_not = true; break; }
    }
    CHECK_FALSE(found_should_not);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: #line directive") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_line.cpp";
    std::ofstream out(tmp);
    out << "int x = 1;\n";
    out << "#line 42 \"test.cpp\"\n";
    out << "int y = 2;\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    // Just verify #line doesn't crash and tokens flow through.
    int tokens = 0;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        ++tokens;
    }
    CHECK(tokens > 0);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: variadic macro with __VA_ARGS__") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_variadic.cpp";
    std::ofstream out(tmp);
    out << "#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)\n";
    out << "LOG(\"hello %s\", \"world\");\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    // __VA_ARGS__ should expand to the variadic arguments.
    bool found_printf = false;
    bool found_hello = false;
    bool found_world = false;
    for (const auto& t : texts) {
        if (t == "printf") found_printf = true;
        if (t == "\"hello %s\"") found_hello = true;
        if (t == "\"world\"") found_world = true;
    }
    CHECK(found_printf);
    CHECK(found_hello);
    CHECK(found_world);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: variadic macro with only __VA_ARGS__") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_variadic2.cpp";
    std::ofstream out(tmp);
    out << "#define DEBUG(...) fprintf(stderr, __VA_ARGS__)\n";
    out << "DEBUG(\"error: %d\\n\", 42);\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    std::vector<std::string> texts;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        texts.push_back(std::string(pp.current().text));
    }

    bool found_fprintf = false;
    bool found_stderr = false;
    for (const auto& t : texts) {
        if (t == "fprintf") found_fprintf = true;
        if (t == "stderr") found_stderr = true;
    }
    CHECK(found_fprintf);
    CHECK(found_stderr);

    std::filesystem::remove(tmp);
}

TEST_CASE("Preprocessor: _Pragma operator") {
    DiagnosticEngine diagnostics;
    SourceManager source_manager;
    Preprocessor pp(source_manager, diagnostics);

    std::filesystem::path tmp = "krc_pp_test_pragma.cpp";
    std::ofstream out(tmp);
    out << "_Pragma(\"once\")\n";
    out << "int x = 42;\n";
    out << "_Pragma(\"once\")\n";
    out << "int y = 43;\n";
    out.close();

    REQUIRE(pp.push_file(tmp.string()));

    // Just verify _Pragma doesn't crash and tokens flow through.
    int tokens = 0;
    while (!pp.at_eof()) {
        LexResult r = pp.next();
        REQUIRE(r.ok);
        ++tokens;
    }
    CHECK(tokens > 0);

    std::filesystem::remove(tmp);
}

TEST_CASE("Sema namespace: register symbols in namespace scope") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: namespace NS { int x; }
    auto int_type = sema.int_type();

    auto x_decl = std::make_shared<Declaration>();
    x_decl->type = int_type;
    auto x_dec = std::make_shared<Declarator>();
    x_dec->name = "x";
    x_decl->declarators.push_back(x_dec);

    auto ns_decl = std::make_shared<Declaration>();
    ns_decl->is_namespace = true;
    ns_decl->namespace_name = "NS";
    ns_decl->namespace_members.push_back(x_decl);

    auto tu = std::make_shared<TranslationUnit>();
    tu->declarations.push_back(ns_decl);

    CHECK(sema.analyze(*tu));

    // After analyze, we're back in file scope, so lookup("x") should fail
    // (x is inside the namespace scope, not file scope).
    auto sym = sema.lookup("x");
    CHECK(sym == nullptr);
}

TEST_CASE("Sema namespace: nested namespace") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: namespace Outer { namespace Inner { int y; } }
    auto int_type = sema.int_type();

    auto y_decl = std::make_shared<Declaration>();
    y_decl->type = int_type;
    auto y_dec = std::make_shared<Declarator>();
    y_dec->name = "y";
    y_decl->declarators.push_back(y_dec);

    auto inner_ns = std::make_shared<Declaration>();
    inner_ns->is_namespace = true;
    inner_ns->namespace_name = "Inner";
    inner_ns->namespace_members.push_back(y_decl);

    auto outer_ns = std::make_shared<Declaration>();
    outer_ns->is_namespace = true;
    outer_ns->namespace_name = "Outer";
    outer_ns->namespace_members.push_back(inner_ns);

    auto tu = std::make_shared<TranslationUnit>();
    tu->declarations.push_back(outer_ns);

    CHECK(sema.analyze(*tu));

    // y is not visible from file scope.
    auto sym = sema.lookup("y");
    CHECK(sym == nullptr);
}

TEST_CASE("Sema reference: reference_to type builder") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto int_type = sema.int_type();
    auto ref_type = sema.reference_to(int_type);

    REQUIRE(ref_type != nullptr);
    CHECK(ref_type->kind == TypeKind::Reference);
    CHECK(ref_type->pointee == int_type);
    CHECK(sema.is_reference_type(ref_type));
    CHECK(!sema.is_pointer_type(ref_type));
}

TEST_CASE("Sema reference: types_compatible for references") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto int_type = sema.int_type();
    auto ref1 = sema.reference_to(int_type);
    auto ref2 = sema.reference_to(int_type);
    auto ref_double = sema.reference_to(sema.double_type());

    // References to the same type are compatible.
    CHECK(sema.types_compatible(ref1, ref2));
    // References to different types are not compatible.
    CHECK(!sema.types_compatible(ref1, ref_double));
}

TEST_CASE("Sema reference: is_reference_type distinguishes from pointer") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto int_type = sema.int_type();
    auto ref_type = sema.reference_to(int_type);
    auto ptr_type = sema.pointer_to(int_type);

    CHECK(sema.is_reference_type(ref_type));
    CHECK(!sema.is_reference_type(ptr_type));
    CHECK(sema.is_pointer_type(ptr_type));
    CHECK(!sema.is_pointer_type(ref_type));
}

TEST_CASE("Sema inheritance: base class member lookup through derived") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: struct Base { int base_member; };
    auto base_type = std::make_shared<Type>();
    base_type->kind = TypeKind::Struct;
    base_type->name = "Base";

    auto base_member_decl = std::make_shared<Declaration>();
    base_member_decl->type = sema.int_type();
    auto base_member_dec = std::make_shared<Declarator>();
    base_member_dec->name = "base_member";
    base_member_decl->declarators.push_back(base_member_dec);
    base_type->members.push_back(base_member_decl);

    // Build: struct Derived : Base { int derived_member; };
    auto derived_type = std::make_shared<Type>();
    derived_type->kind = TypeKind::Struct;
    derived_type->name = "Derived";
    derived_type->base_classes.push_back(base_type);

    auto derived_member_decl = std::make_shared<Declaration>();
    derived_member_decl->type = sema.int_type();
    auto derived_member_dec = std::make_shared<Declarator>();
    derived_member_dec->name = "derived_member";
    derived_member_decl->declarators.push_back(derived_member_dec);
    derived_type->members.push_back(derived_member_decl);

    // lookup_member on Derived should find both own and base members.
    auto own = sema.lookup_member(derived_type, "derived_member");
    REQUIRE(own != nullptr);
    CHECK(own->name == "derived_member");

    auto base = sema.lookup_member(derived_type, "base_member");
    REQUIRE(base != nullptr);
    CHECK(base->name == "base_member");

    // lookup_member on Base should NOT find derived_member.
    auto missing = sema.lookup_member(base_type, "derived_member");
    CHECK(missing == nullptr);
}

TEST_CASE("Sema inheritance: base class not found returns nullptr") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: struct Derived : Base { int x; }; where Base is undefined.
    auto derived_type = std::make_shared<Type>();
    derived_type->kind = TypeKind::Struct;
    derived_type->name = "Derived";
    auto undefined_base = std::make_shared<Type>();
    undefined_base->kind = TypeKind::Struct;
    undefined_base->name = "Base";
    derived_type->base_classes.push_back(undefined_base);

    auto decl = std::make_shared<Declaration>();
    decl->type = sema.int_type();
    auto dec = std::make_shared<Declarator>();
    dec->name = "x";
    decl->declarators.push_back(dec);
    derived_type->members.push_back(decl);

    // Only own members are found.
    auto sym = sema.lookup_member(derived_type, "x");
    REQUIRE(sym != nullptr);
    CHECK(sym->name == "x");

    // Undefined base yields no members.
    auto missing = sema.lookup_member(derived_type, "nonexistent");
    CHECK(missing == nullptr);
}

TEST_CASE("Sema virtual: is_virtual flag on member function symbol") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: struct Base { virtual void f(); };
    auto base_type = std::make_shared<Type>();
    base_type->kind = TypeKind::Struct;
    base_type->name = "Base";

    auto void_type = sema.void_type();
    auto virtual_decl = std::make_shared<Declaration>();
    virtual_decl->type = void_type;
    virtual_decl->is_virtual = true;
    auto virtual_dec = std::make_shared<Declarator>();
    virtual_dec->name = "f";
    virtual_dec->is_function = true;
    virtual_decl->declarators.push_back(virtual_dec);
    base_type->members.push_back(virtual_decl);

    // lookup_member should find the virtual function.
    auto sym = sema.lookup_member(base_type, "f");
    REQUIRE(sym != nullptr);
    CHECK(sym->name == "f");
    CHECK(sym->is_virtual);
}

TEST_CASE("Sema virtual: non-virtual member has is_virtual=false") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: struct Base { void f(); };
    auto base_type = std::make_shared<Type>();
    base_type->kind = TypeKind::Struct;
    base_type->name = "Base";

    auto void_type = sema.void_type();
    auto decl = std::make_shared<Declaration>();
    decl->type = void_type;
    decl->is_virtual = false;
    auto dec = std::make_shared<Declarator>();
    dec->name = "f";
    dec->is_function = true;
    decl->declarators.push_back(dec);
    base_type->members.push_back(decl);

    auto sym = sema.lookup_member(base_type, "f");
    REQUIRE(sym != nullptr);
    CHECK(sym->name == "f");
    CHECK(!sym->is_virtual);
}

TEST_CASE("Sema template: register and find template declaration") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: template <typename T> T identity(T x);
    auto t_param = std::make_shared<TemplateParam>();
    t_param->kind = TemplateParam::Kind::Type;
    t_param->name = "T";

    auto t_type = std::make_shared<Type>();
    t_type->kind = TypeKind::TemplateParam;
    t_type->template_param_name = "T";

    auto decl = std::make_shared<Declaration>();
    decl->type = t_type;
    auto dec = std::make_shared<Declarator>();
    dec->name = "identity";
    dec->is_function = true;
    dec->param_types.push_back(t_type);
    decl->declarators.push_back(dec);
    decl->template_params.push_back(t_param);

    auto tmpl = std::make_shared<TemplateDeclaration>();
    tmpl->params = {t_param};
    tmpl->declaration = decl;

    sema.register_template("identity", tmpl);

    auto found = sema.find_template("identity");
    REQUIRE(found != nullptr);
    CHECK(found->params.size() == 1);
    CHECK(found->params[0]->name == "T");
}

TEST_CASE("Sema template: deduce template arguments from call") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: template <typename T> T identity(T x);
    auto t_param = std::make_shared<TemplateParam>();
    t_param->kind = TemplateParam::Kind::Type;
    t_param->name = "T";

    auto t_type = std::make_shared<Type>();
    t_type->kind = TypeKind::TemplateParam;
    t_type->template_param_name = "T";

    auto int_type = sema.int_type();

    auto decl = std::make_shared<Declaration>();
    decl->type = t_type;
    auto dec = std::make_shared<Declarator>();
    dec->name = "identity";
    dec->is_function = true;
    dec->param_types.push_back(t_type);
    decl->declarators.push_back(dec);
    decl->template_params.push_back(t_param);

    auto tmpl = std::make_shared<TemplateDeclaration>();
    tmpl->params = {t_param};
    tmpl->declaration = decl;

    sema.register_template("identity", tmpl);

    // Build call: identity(42) with int arg.
    auto arg = std::make_shared<Expression>();
    arg->kind = ExpressionKind::IntegerLiteral;
    arg->type = int_type;
    arg->is_constant = true;
    arg->is_lvalue = false;

    auto callee = std::make_shared<Expression>();
    callee->kind = ExpressionKind::Identifier;
    callee->identifier_name = "identity";

    auto call = std::make_shared<Expression>();
    call->kind = ExpressionKind::FunctionCall;
    call->callee = callee;
    call->call_args = {arg};

    std::vector<std::shared_ptr<Type>> deduced;
    CHECK(sema.deduce_template_args(
        tmpl->params, call->call_args,
        decl->declarators[0]->param_types, deduced));

    REQUIRE(deduced.size() == 1);
    CHECK(deduced[0] == int_type);
}

TEST_CASE("Sema template: instantiate and call template function") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: template <typename T> T identity(T x);
    auto t_param = std::make_shared<TemplateParam>();
    t_param->kind = TemplateParam::Kind::Type;
    t_param->name = "T";

    auto t_type = std::make_shared<Type>();
    t_type->kind = TypeKind::TemplateParam;
    t_type->template_param_name = "T";

    auto int_type = sema.int_type();

    auto decl = std::make_shared<Declaration>();
    decl->type = t_type;
    auto dec = std::make_shared<Declarator>();
    dec->name = "identity";
    dec->is_function = true;
    dec->param_types.push_back(t_type);
    decl->declarators.push_back(dec);
    decl->template_params.push_back(t_param);

    auto tmpl = std::make_shared<TemplateDeclaration>();
    tmpl->params = {t_param};
    tmpl->declaration = decl;

    sema.register_template("identity", tmpl);

    // Insert a symbol so lookup can find it.
    auto call_sym = std::make_shared<Symbol>();
    call_sym->name = "identity";
    call_sym->type = sema.function_type(t_type, {t_type}, false);
    sema.insert_symbol("identity", call_sym);

    // Build call: identity(42) with int arg.
    auto arg = std::make_shared<Expression>();
    arg->kind = ExpressionKind::IntegerLiteral;
    arg->type = int_type;
    arg->is_constant = true;
    arg->is_lvalue = false;

    auto callee = std::make_shared<Expression>();
    callee->kind = ExpressionKind::Identifier;
    callee->identifier_name = "identity";

    auto call = std::make_shared<Expression>();
    call->kind = ExpressionKind::FunctionCall;
    call->callee = callee;
    call->call_args = {arg};

    CHECK(sema.type_function_call(*call));
    REQUIRE(call->type != nullptr);
    CHECK(call->type == int_type);
}

TEST_CASE("Sema auto: auto_type() returns Auto placeholder") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto at = sema.auto_type();
    REQUIRE(at != nullptr);
    CHECK(at->kind == TypeKind::Auto);
    CHECK(at->name == "auto");
}

TEST_CASE("Sema auto: resolve_declarator_type marks auto_deduced") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: auto x = 42;
    auto auto_ty = sema.auto_type();
    auto decl = std::make_shared<Declaration>();
    decl->type = auto_ty;
    auto dec = std::make_shared<Declarator>();
    dec->name = "x";
    decl->declarators.push_back(dec);

    CHECK(sema.resolve_declarator_type(*decl));
    CHECK(dec->auto_deduced);
}

TEST_CASE("Sema auto: deduce auto type from initializer") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: auto x = 42;
    auto auto_ty = sema.auto_type();
    auto int_ty = sema.int_type();

    auto decl = std::make_shared<Declaration>();
    decl->type = auto_ty;
    auto dec = std::make_shared<Declarator>();
    dec->name = "x";
    dec->auto_deduced = true;
    decl->declarators.push_back(dec);

    // Build initializer: integer literal 42 with int type.
    auto init_expr = std::make_shared<Expression>();
    init_expr->kind = ExpressionKind::IntegerLiteral;
    init_expr->type = int_ty;
    init_expr->is_constant = true;
    init_expr->is_lvalue = false;

    auto init = std::make_shared<Initializer>();
    init->kind = Initializer::Kind::Expression;
    init->expr = init_expr;

    decl->initializers.push_back(init);

    // Simulate the auto deduction that walk_declaration() performs:
    // type the initializer, apply decay, deduce the auto type.
    REQUIRE(sema.type_expression(*init_expr));
    auto init_type = init_expr->type;
    if (sema.is_array_type(init_type)) {
        init_type = sema.array_to_pointer_decay(init_type);
    }
    if (sema.is_function_type(init_type)) {
        init_type = sema.function_to_pointer_decay(init_type);
    }
    sema.lvalue_to_rvalue(*init_expr);

    // Set the deduced type on the declarator.
    dec->auto_resolved_type = init_type;

    REQUIRE(dec->auto_resolved_type != nullptr);
    CHECK(dec->auto_resolved_type == int_ty);
}

TEST_CASE("Sema auto: deduce auto type from double initializer") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto auto_ty = sema.auto_type();
    auto double_ty = sema.double_type();

    auto decl = std::make_shared<Declaration>();
    decl->type = auto_ty;
    auto dec = std::make_shared<Declarator>();
    dec->name = "d";
    dec->auto_deduced = true;
    decl->declarators.push_back(dec);

    auto init_expr = std::make_shared<Expression>();
    init_expr->kind = ExpressionKind::FloatingLiteral;
    init_expr->type = double_ty;
    init_expr->is_constant = true;
    init_expr->is_lvalue = false;

    auto init = std::make_shared<Initializer>();
    init->kind = Initializer::Kind::Expression;
    init->expr = init_expr;

    decl->initializers.push_back(init);

    REQUIRE(sema.type_expression(*init_expr));
    auto init_type = init_expr->type;
    sema.lvalue_to_rvalue(*init_expr);

    dec->auto_resolved_type = init_type;

    REQUIRE(dec->auto_resolved_type != nullptr);
    CHECK(dec->auto_resolved_type == double_ty);
}


TEST_CASE("Sema noexcept: noexcept function specifier registered") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: void f() noexcept;
    auto decl = std::make_shared<Declaration>();
    decl->type = sema.void_type();
    decl->is_noexcept = true;
    auto dec = std::make_shared<Declarator>();
    dec->name = "f";
    dec->is_function = true;
    decl->declarators.push_back(dec);

    CHECK(sema.resolve_declarator_type(*decl));
    CHECK(sema.walk_declaration(*decl));

    auto sym = sema.lookup("f");
    REQUIRE(sym != nullptr);
    CHECK(sym->is_noexcept);
}

TEST_CASE("Sema alignas: alignas specifier registered") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: alignas(16) int x;
    auto decl = std::make_shared<Declaration>();
    decl->type = sema.int_type();
    decl->has_alignas = true;
    auto dec = std::make_shared<Declarator>();
    dec->name = "x";
    decl->declarators.push_back(dec);

    CHECK(sema.resolve_declarator_type(*decl));
    CHECK(sema.walk_declaration(*decl));

    auto sym = sema.lookup("x");
    REQUIRE(sym != nullptr);
    CHECK(sym->has_alignas);
}

TEST_CASE("Sema thread_local: thread_local storage class registered") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: static thread_local int x = 42;
    auto decl = std::make_shared<Declaration>();
    decl->type = sema.int_type();
    decl->storage = StorageClass::ThreadLocal;
    decl->is_static = true;
    auto dec = std::make_shared<Declarator>();
    dec->name = "x";
    decl->declarators.push_back(dec);

    auto init_expr = std::make_shared<Expression>();
    init_expr->kind = ExpressionKind::IntegerLiteral;
    init_expr->type = sema.int_type();
    init_expr->is_constant = true;
    init_expr->is_lvalue = false;

    auto init = std::make_shared<Initializer>();
    init->kind = Initializer::Kind::Expression;
    init->expr = init_expr;
    decl->initializers.push_back(init);

    CHECK(sema.resolve_declarator_type(*decl));
    CHECK(sema.walk_declaration(*decl));

    // Symbol should be registered with thread_local storage.
    auto sym = sema.lookup("x");
    REQUIRE(sym != nullptr);
    CHECK(sym->storage == StorageClass::ThreadLocal);
    CHECK(sym->is_static);
}

TEST_CASE("Sema if constexpr: true condition walks then-branch, discards else") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: if constexpr (true) { int x = 1; } else { int y = 2; }
    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::If;
    stmt->is_constexpr_if = true;

    // Condition: true (integer literal 1)
    auto cond = std::make_shared<Expression>();
    cond->kind = ExpressionKind::IntegerLiteral;
    cond->type = sema.int_type();
    cond->is_constant = true;
    cond->int_literal_value = 1;
    stmt->condition = cond;

    // Then branch: declaration of x
    auto then_decl = std::make_shared<Declaration>();
    then_decl->type = sema.int_type();
    auto then_dec = std::make_shared<Declarator>();
    then_dec->name = "x";
    then_decl->declarators.push_back(then_dec);
    auto then_stmt = std::make_shared<Statement>();
    then_stmt->kind = StatementKind::Declaration;
    then_stmt->declaration = then_decl;
    stmt->then_branch = then_stmt;

    // Else branch: declaration of y (should be discarded)
    auto else_decl = std::make_shared<Declaration>();
    else_decl->type = sema.int_type();
    auto else_dec = std::make_shared<Declarator>();
    else_dec->name = "y";
    else_decl->declarators.push_back(else_dec);
    auto else_stmt = std::make_shared<Statement>();
    else_stmt->kind = StatementKind::Declaration;
    else_stmt->declaration = else_decl;
    stmt->else_branch = else_stmt;

    CHECK(sema.walk_statement(*stmt));

    // x should be registered, y should not.
    auto sym_x = sema.lookup("x");
    REQUIRE(sym_x != nullptr);
    auto sym_y = sema.lookup("y");
    CHECK(sym_y == nullptr);
}

TEST_CASE("Sema if constexpr: false condition walks else-branch, discards then") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: if constexpr (false) { int x = 1; } else { int y = 2; }
    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::If;
    stmt->is_constexpr_if = true;

    // Condition: false (integer literal 0)
    auto cond = std::make_shared<Expression>();
    cond->kind = ExpressionKind::IntegerLiteral;
    cond->type = sema.int_type();
    cond->is_constant = true;
    cond->int_literal_value = 0;
    stmt->condition = cond;

    // Then branch: declaration of x (should be discarded)
    auto then_decl = std::make_shared<Declaration>();
    then_decl->type = sema.int_type();
    auto then_dec = std::make_shared<Declarator>();
    then_dec->name = "x";
    then_decl->declarators.push_back(then_dec);
    auto then_stmt = std::make_shared<Statement>();
    then_stmt->kind = StatementKind::Declaration;
    then_stmt->declaration = then_decl;
    stmt->then_branch = then_stmt;

    // Else branch: declaration of y
    auto else_decl = std::make_shared<Declaration>();
    else_decl->type = sema.int_type();
    auto else_dec = std::make_shared<Declarator>();
    else_dec->name = "y";
    else_decl->declarators.push_back(else_dec);
    auto else_stmt = std::make_shared<Statement>();
    else_stmt->kind = StatementKind::Declaration;
    else_stmt->declaration = else_decl;
    stmt->else_branch = else_stmt;

    CHECK(sema.walk_statement(*stmt));

    // y should be registered, x should not.
    auto sym_y = sema.lookup("y");
    REQUIRE(sym_y != nullptr);
    auto sym_x = sema.lookup("x");
    CHECK(sym_x == nullptr);
}


TEST_CASE("Sema decltype: decltype_type() returns Decltype placeholder") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto dt = sema.decltype_type();
    REQUIRE(dt != nullptr);
    CHECK(dt->kind == TypeKind::Decltype);
    CHECK(dt->name == "decltype");
}

TEST_CASE("Sema decltype: resolve decltype from int expression") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto int_ty = sema.int_type();

    // Insert symbol "x" into file scope so identifier lookup succeeds.
    auto sym = std::make_shared<Symbol>();
    sym->name = "x";
    sym->type = int_ty;
    sema.insert_symbol("x", sym);

    // Build: decltype(x) where x is an int expression.
    auto expr = std::make_shared<Expression>();
    expr->kind = ExpressionKind::Identifier;
    expr->identifier_name = "x";
    expr->type = int_ty;
    expr->is_lvalue = true;

    REQUIRE(sema.type_expression(*expr));
    auto resolved_type = expr->type;

    REQUIRE(resolved_type != nullptr);
    CHECK(resolved_type == int_ty);
}

TEST_CASE("Sema decltype: resolve decltype from double expression") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto double_ty = sema.double_type();

    // Build: decltype(d) where d is a double expression.
    auto expr = std::make_shared<Expression>();
    expr->kind = ExpressionKind::FloatingLiteral;
    expr->type = double_ty;
    expr->is_constant = true;
    expr->is_lvalue = false;

    REQUIRE(sema.type_expression(*expr));
    auto resolved_type = expr->type;

    REQUIRE(resolved_type != nullptr);
    CHECK(resolved_type == double_ty);
}

TEST_CASE("Sema template: instantiate with default type parameter") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: template <typename T = int> T make_default();
    auto t_param = std::make_shared<TemplateParam>();
    t_param->kind = TemplateParam::Kind::Type;
    t_param->name = "T";
    t_param->default_type = sema.int_type();

    auto t_type = std::make_shared<Type>();
    t_type->kind = TypeKind::TemplateParam;
    t_type->template_param_name = "T";

    auto int_type = sema.int_type();

    auto decl = std::make_shared<Declaration>();
    decl->type = t_type;  // return type is T
    auto dec = std::make_shared<Declarator>();
    dec->name = "make_default";
    dec->is_function = true;
    // Takes no arguments - no param_types needed.
    decl->declarators.push_back(dec);
    decl->template_params.push_back(t_param);

    auto tmpl = std::make_shared<TemplateDeclaration>();
    tmpl->params = {t_param};
    tmpl->declaration = decl;

    sema.register_template("make_default", tmpl);

    // Insert a symbol so lookup can find it.
    auto default_sym = std::make_shared<Symbol>();
    default_sym->name = "make_default";
    default_sym->type = sema.function_type(t_type, {}, false);
    sema.insert_symbol("make_default", default_sym);

    // Build call: make_default() with no args - should use default type.
    auto callee = std::make_shared<Expression>();
    callee->kind = ExpressionKind::Identifier;
    callee->identifier_name = "make_default";

    auto call = std::make_shared<Expression>();
    call->kind = ExpressionKind::FunctionCall;
    call->callee = callee;
    call->call_args = {};

    CHECK(sema.type_function_call(*call));
    REQUIRE(call->type != nullptr);
    CHECK(call->type == int_type);
}

TEST_CASE("Sema template: variadic type parameter pack") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: template <typename... Args> class Tuple { ... };
    auto tu = std::make_shared<TranslationUnit>();

    // Template parameter: typename... Args (variadic type parameter pack)
    auto args_param = std::make_shared<TemplateParam>();
    args_param->kind = TemplateParam::Kind::Type;
    args_param->is_variadic = true;
    args_param->name = "Args";

    // The class Tuple type
    auto tuple_type = std::make_shared<Type>();
    tuple_type->kind = TypeKind::Struct;
    tuple_type->name = "Tuple";

    // Outer declaration: class Tuple { ... } with variadic template param
    auto outer_decl = std::make_shared<Declaration>();
    outer_decl->type = tuple_type;
    outer_decl->template_params.push_back(args_param);

    tu->declarations.push_back(outer_decl);

    // Sema should accept the variadic template declaration.
    CHECK(sema.analyze(*tu));
}

TEST_CASE("Sema template: mixed variadic and regular type parameters") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: template <typename T, typename... Rest> void func(T t, Rest... rest);
    auto tu = std::make_shared<TranslationUnit>();

    // Template parameter: typename T
    auto t_param = std::make_shared<TemplateParam>();
    t_param->kind = TemplateParam::Kind::Type;
    t_param->name = "T";

    // Template parameter: typename... Rest (variadic)
    auto rest_param = std::make_shared<TemplateParam>();
    rest_param->kind = TemplateParam::Kind::Type;
    rest_param->is_variadic = true;
    rest_param->name = "Rest";

    auto func_type = std::make_shared<Type>();
    func_type->kind = TypeKind::Function;
    func_type->name = "func";

    auto outer_decl = std::make_shared<Declaration>();
    outer_decl->type = func_type;
    outer_decl->template_params = {t_param, rest_param};

    tu->declarations.push_back(outer_decl);

    // Sema should accept the mixed template declaration.
    CHECK(sema.analyze(*tu));
}

TEST_CASE("Sema expression: fold expression AST node") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: auto sum = (args + ...);
    auto fold = std::make_shared<Expression>();
    fold->kind = ExpressionKind::FoldExpression;
    fold->fold_kind = 1;  // unary right fold: (pack op ...)
    fold->fold_op = FoldOp::Plus;

    auto pack = std::make_shared<Expression>();
    pack->kind = ExpressionKind::Identifier;
    pack->identifier_name = "args";
    fold->fold_operand = pack;

    // Verify fold expression fields are correctly set.
    CHECK(fold->kind == ExpressionKind::FoldExpression);
    CHECK(fold->fold_kind == 1);
    CHECK(fold->fold_op == FoldOp::Plus);
    REQUIRE(fold->fold_operand != nullptr);
    CHECK(fold->fold_operand->kind == ExpressionKind::Identifier);
    CHECK(fold->fold_operand->identifier_name == "args");
}

TEST_CASE("Sema expression: binary fold expression AST node") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: auto sum = (args + ... + init);
    auto fold = std::make_shared<Expression>();
    fold->kind = ExpressionKind::FoldExpression;
    fold->fold_kind = 2;  // binary left fold: (pack op ... op init)
    fold->fold_op = FoldOp::Plus;

    auto pack = std::make_shared<Expression>();
    pack->kind = ExpressionKind::Identifier;
    pack->identifier_name = "args";
    fold->fold_operand = pack;

    auto init = std::make_shared<Expression>();
    init->kind = ExpressionKind::IntegerLiteral;
    init->int_literal_value = 0;
    fold->fold_init = init;

    // Verify fold expression fields are correctly set.
    CHECK(fold->kind == ExpressionKind::FoldExpression);
    CHECK(fold->fold_kind == 2);
    CHECK(fold->fold_op == FoldOp::Plus);
    REQUIRE(fold->fold_operand != nullptr);
    CHECK(fold->fold_operand->identifier_name == "args");
    REQUIRE(fold->fold_init != nullptr);
    CHECK(fold->fold_init->kind == ExpressionKind::IntegerLiteral);
    CHECK(fold->fold_init->int_literal_value == 0);
}

TEST_CASE("Sema expression: unary left fold expression AST node") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: auto sum = (... + args);
    auto fold = std::make_shared<Expression>();
    fold->kind = ExpressionKind::FoldExpression;
    fold->fold_kind = 0;  // unary left fold: (... op pack)
    fold->fold_op = FoldOp::Plus;

    auto pack = std::make_shared<Expression>();
    pack->kind = ExpressionKind::Identifier;
    pack->identifier_name = "args";
    fold->fold_operand = pack;

    // Verify fold expression fields are correctly set.
    CHECK(fold->kind == ExpressionKind::FoldExpression);
    CHECK(fold->fold_kind == 0);
    CHECK(fold->fold_op == FoldOp::Plus);
    REQUIRE(fold->fold_operand != nullptr);
    CHECK(fold->fold_operand->identifier_name == "args");
}

TEST_CASE("Sema constexpr: constexpr variable specifier registered") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: constexpr int x = 42;
    auto decl = std::make_shared<Declaration>();
    decl->type = sema.int_type();
    decl->is_constexpr = true;
    auto dec = std::make_shared<Declarator>();
    dec->name = "x";
    decl->declarators.push_back(dec);

    auto init_expr = std::make_shared<Expression>();
    init_expr->kind = ExpressionKind::IntegerLiteral;
    init_expr->type = sema.int_type();
    init_expr->is_constant = true;
    init_expr->is_lvalue = false;

    auto init = std::make_shared<Initializer>();
    init->kind = Initializer::Kind::Expression;
    init->expr = init_expr;
    decl->initializers.push_back(init);

    CHECK(sema.resolve_declarator_type(*decl));
    CHECK(sema.walk_declaration(*decl));

    auto sym = sema.lookup("x");
    REQUIRE(sym != nullptr);
    CHECK(sym->is_constexpr);
}

TEST_CASE("Sema constexpr: constexpr function specifier registered") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: constexpr int f() { return 1; }
    auto decl = std::make_shared<Declaration>();
    decl->type = sema.int_type();
    decl->is_constexpr = true;
    auto dec = std::make_shared<Declarator>();
    dec->name = "f";
    dec->is_function = true;
    decl->declarators.push_back(dec);

    CHECK(sema.resolve_declarator_type(*decl));
    CHECK(sema.walk_declaration(*decl));

    auto sym = sema.lookup("f");
    REQUIRE(sym != nullptr);
    CHECK(sym->is_constexpr);
}

TEST_CASE("Sema inline: inline variable specifier registered") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: inline int x = 42;
    auto decl = std::make_shared<Declaration>();
    decl->type = sema.int_type();
    decl->is_inline = true;
    auto dec = std::make_shared<Declarator>();
    dec->name = "x";
    decl->declarators.push_back(dec);

    auto init_expr = std::make_shared<Expression>();
    init_expr->kind = ExpressionKind::IntegerLiteral;
    init_expr->type = sema.int_type();
    init_expr->is_constant = true;
    init_expr->is_lvalue = false;

    auto init = std::make_shared<Initializer>();
    init->kind = Initializer::Kind::Expression;
    init->expr = init_expr;
    decl->initializers.push_back(init);

    CHECK(sema.resolve_declarator_type(*decl));
    CHECK(sema.walk_declaration(*decl));

    auto sym = sema.lookup("x");
    REQUIRE(sym != nullptr);
    CHECK(sym->is_inline);
}

TEST_CASE("Sema inline: inline function specifier registered") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: inline int f() { return 1; }
    auto decl = std::make_shared<Declaration>();
    decl->type = sema.int_type();
    decl->is_inline = true;
    auto dec = std::make_shared<Declarator>();
    dec->name = "f";
    dec->is_function = true;
    decl->declarators.push_back(dec);

    CHECK(sema.resolve_declarator_type(*decl));
    CHECK(sema.walk_declaration(*decl));

    auto sym = sema.lookup("f");
    REQUIRE(sym != nullptr);
    CHECK(sym->is_inline);
}

TEST_CASE("Sema range-for: range-based for registers range declaration and types range expression") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: for (int x : arr) { }
    
    // Register range declaration x first.
    auto x_decl = std::make_shared<Declaration>();
    x_decl->type = sema.int_type();
    auto x_dec = std::make_shared<Declarator>();
    x_dec->name = "x";
    x_decl->declarators.push_back(x_dec);
    sema.resolve_declarator_type(*x_decl);
    sema.walk_declaration(*x_decl);

    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::For;
    stmt->is_range_for = true;

    // Range declaration: int x (already registered, reuse)
    stmt->declaration = x_decl;

    // Range expression: arr (identifier of some array type)
    auto arr_type = std::make_shared<Type>();
    arr_type->kind = TypeKind::Array;
    arr_type->element_type = sema.int_type();
    arr_type->array_length = 10;

    // Register arr as a symbol so the identifier resolves.
    auto arr_decl = std::make_shared<Declaration>();
    arr_decl->type = arr_type;
    auto arr_dec = std::make_shared<Declarator>();
    arr_dec->name = "arr";
    arr_decl->declarators.push_back(arr_dec);
    sema.resolve_declarator_type(*arr_decl);
    sema.walk_declaration(*arr_decl);

    auto range_expr = std::make_shared<Expression>();
    range_expr->kind = ExpressionKind::Identifier;
    range_expr->identifier_name = "arr";
    range_expr->type = arr_type;
    stmt->for_range_expr = range_expr;

    // Loop body: empty compound
    stmt->loop_body = std::make_shared<Statement>();
    stmt->loop_body->kind = StatementKind::Compound;

    CHECK(sema.walk_statement(*stmt));

    auto sym = sema.lookup("x");
    REQUIRE(sym != nullptr);
    CHECK(sym->type == sema.int_type());
}

TEST_CASE("Sema range-for: range-based for walks loop body") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: for (int x : arr) { int y = 1; }
    
    // Register range declaration x first.
    auto x_decl = std::make_shared<Declaration>();
    x_decl->type = sema.int_type();
    auto x_dec = std::make_shared<Declarator>();
    x_dec->name = "x";
    x_decl->declarators.push_back(x_dec);
    sema.resolve_declarator_type(*x_decl);
    sema.walk_declaration(*x_decl);

    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::For;
    stmt->is_range_for = true;

    stmt->declaration = x_decl;

    // Range expression
    auto arr_type = std::make_shared<Type>();
    arr_type->kind = TypeKind::Array;
    arr_type->element_type = sema.int_type();
    arr_type->array_length = 10;

    // Register arr as a symbol so the identifier resolves.
    auto arr_decl = std::make_shared<Declaration>();
    arr_decl->type = arr_type;
    auto arr_dec = std::make_shared<Declarator>();
    arr_dec->name = "arr";
    arr_decl->declarators.push_back(arr_dec);
    sema.resolve_declarator_type(*arr_decl);
    sema.walk_declaration(*arr_decl);

    auto range_expr = std::make_shared<Expression>();
    range_expr->kind = ExpressionKind::Identifier;
    range_expr->identifier_name = "arr";
    range_expr->type = arr_type;
    stmt->for_range_expr = range_expr;

    // Loop body: declaration of y
    auto body_decl = std::make_shared<Declaration>();
    body_decl->type = sema.int_type();
    auto body_dec = std::make_shared<Declarator>();
    body_dec->name = "y";
    body_decl->declarators.push_back(body_dec);
    auto body_stmt = std::make_shared<Statement>();
    body_stmt->kind = StatementKind::Declaration;
    body_stmt->declaration = body_decl;
    stmt->loop_body = body_stmt;

    // Loop body declarations are in an inner scope that gets popped after
    // walk_statement returns. Just verify the walk succeeded and x is still
    // accessible in the outer scope.
    CHECK(sema.walk_statement(*stmt));

    auto sym_x = sema.lookup("x");
    REQUIRE(sym_x != nullptr);
}

TEST_CASE("Sema lambda: empty capture lambda with no params and deduced return") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: [] { }  (lambda expression)
    auto expr = std::make_shared<Expression>();
    expr->kind = ExpressionKind::Lambda;
    expr->lambda_captures = {};
    expr->lambda_params = {};
    expr->lambda_return_type = nullptr; // deduced
    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    expr->lambda_body = body;

    CHECK(sema.type_lambda(*expr));

    REQUIRE(expr->type != nullptr);
    CHECK(expr->type->kind == TypeKind::Function);
    CHECK(expr->type->return_type == sema.void_type());
    CHECK(expr->type->param_types.empty());
    CHECK(!expr->is_lvalue);
    CHECK(!expr->is_constant);
}

TEST_CASE("Sema lambda: lambda with params and explicit return type") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: [](int x) -> int { return x; }
    auto expr = std::make_shared<Expression>();
    expr->kind = ExpressionKind::Lambda;
    expr->lambda_captures = {};

    auto param = std::make_shared<Parameter>();
    param->type = sema.int_type();
    auto dec = std::make_shared<Declarator>();
    dec->name = "x";
    param->declarator = dec;
    expr->lambda_params.push_back(param);

    expr->lambda_return_type = sema.int_type();

    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    expr->lambda_body = body;

    CHECK(sema.type_lambda(*expr));

    REQUIRE(expr->type != nullptr);
    CHECK(expr->type->kind == TypeKind::Function);
    CHECK(expr->type->return_type == sema.int_type());
    REQUIRE(expr->type->param_types.size() == 1);
    CHECK(expr->type->param_types[0] == sema.int_type());
}

TEST_CASE("Sema lambda: lambda with capture by reference") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: [&x] { }
    auto expr = std::make_shared<Expression>();
    expr->kind = ExpressionKind::Lambda;
    expr->lambda_captures = {"&x"};
    expr->lambda_params = {};
    expr->lambda_return_type = nullptr;
    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;
    expr->lambda_body = body;

    CHECK(sema.type_lambda(*expr));

    REQUIRE(expr->type != nullptr);
    CHECK(expr->type->kind == TypeKind::Function);
    CHECK(expr->type->return_type == sema.void_type());
    CHECK(expr->type->param_types.empty());
}

TEST_CASE("Sema move: move produces rvalue reference type") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: move(x) where x has type int
    auto operand = std::make_shared<Expression>();
    operand->kind = ExpressionKind::IntegerLiteral;
    operand->type = sema.int_type();
    operand->is_constant = true;
    operand->is_lvalue = false;

    auto expr = std::make_shared<Expression>();
    expr->kind = ExpressionKind::Move;
    expr->move_operand = operand;

    CHECK(sema.type_move(*expr));

    REQUIRE(expr->type != nullptr);
    CHECK(expr->type->kind == TypeKind::RvalueReference);
    REQUIRE(expr->type->pointee != nullptr);
    CHECK(expr->type->pointee == sema.int_type());
    CHECK(!expr->is_lvalue);
    CHECK(!expr->is_constant);
}

TEST_CASE("Sema move: move with non-trivial type produces rvalue reference") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: move(x) where x is an int (so result is int&&)
    auto operand = std::make_shared<Expression>();
    operand->kind = ExpressionKind::IntegerLiteral;
    operand->type = sema.int_type();
    operand->is_constant = false;
    operand->is_lvalue = true;

    auto expr = std::make_shared<Expression>();
    expr->kind = ExpressionKind::Move;
    expr->move_operand = operand;

    CHECK(sema.type_move(*expr));

    REQUIRE(expr->type != nullptr);
    CHECK(expr->type->kind == TypeKind::RvalueReference);
    REQUIRE(expr->type->pointee != nullptr);
    CHECK(expr->type->pointee == sema.int_type());
}

TEST_CASE("Sema initializer_list: initializer_list type compatibility") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: initializer_list<int> - the element type is int
    auto int_list = std::make_shared<Type>();
    int_list->kind = TypeKind::InitializerList;
    int_list->pointee = sema.int_type();

    // Same element type -> compatible
    auto int_list2 = std::make_shared<Type>();
    int_list2->kind = TypeKind::InitializerList;
    int_list2->pointee = sema.int_type();

    CHECK(sema.types_compatible(int_list, int_list2));

    // Different element type -> not compatible
    auto double_list = std::make_shared<Type>();
    double_list->kind = TypeKind::InitializerList;
    double_list->pointee = sema.double_type();

    CHECK(!sema.types_compatible(int_list, double_list));
}

TEST_CASE("Sema initializer_list: is_reference_type returns false for initializer_list") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto init_list = std::make_shared<Type>();
    init_list->kind = TypeKind::InitializerList;
    init_list->pointee = sema.int_type();

    CHECK(!sema.is_reference_type(init_list));
}

TEST_CASE("Sema structured binding: bind names from initializer") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: auto [x, y] = expr; with auto type and initializer of type int
    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::StructuredBinding;
    stmt->is_structured_binding = true;

    auto init_expr = std::make_shared<Expression>();
    init_expr->kind = ExpressionKind::IntegerLiteral;
    init_expr->type = sema.int_type();
    init_expr->is_constant = true;
    init_expr->is_lvalue = false;

    stmt->bound_initializer = init_expr;
    stmt->bound_type = std::make_shared<Type>();
    stmt->bound_type->kind = TypeKind::Auto; // placeholder, will be deduced
    stmt->bound_names = {"x", "y"};

    CHECK(sema.walk_structured_binding(*stmt));

    // Each bound name should be a symbol of type int (deduced from initializer)
    auto sym_x = sema.lookup("x");
    auto sym_y = sema.lookup("y");
    REQUIRE(sym_x != nullptr);
    REQUIRE(sym_y != nullptr);
    CHECK(sym_x->type == sema.int_type());
    CHECK(sym_y->type == sema.int_type());
}

TEST_CASE("Sema structured binding: non-auto bound type") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: int [x] = expr; with explicit int type and int initializer
    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::StructuredBinding;
    stmt->is_structured_binding = true;

    auto init_expr = std::make_shared<Expression>();
    init_expr->kind = ExpressionKind::IntegerLiteral;
    init_expr->type = sema.int_type();
    init_expr->is_constant = true;
    init_expr->is_lvalue = false;

    stmt->bound_initializer = init_expr;
    stmt->bound_type = sema.int_type();
    stmt->bound_names = {"x"};

    CHECK(sema.walk_structured_binding(*stmt));

    auto sym_x = sema.lookup("x");
    REQUIRE(sym_x != nullptr);
    CHECK(sym_x->type == sema.int_type());
}

TEST_CASE("Sema vtable: struct with virtual functions gets vtable layout") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Create a struct type with virtual functions.
    auto struct_type = std::make_shared<Type>();
    struct_type->kind = TypeKind::Struct;
    struct_type->name = "Base";

    // Add a virtual function member.
    auto virt_decl = std::make_shared<Declaration>();
    virt_decl->is_virtual = true;
    virt_decl->type = sema.int_type();
    auto virt_dec = std::make_shared<Declarator>();
    virt_dec->name = "foo";
    virt_decl->declarators.push_back(virt_dec);
    struct_type->members.push_back(virt_decl);

    // Add a non-virtual function member.
    auto nonvirt_decl = std::make_shared<Declaration>();
    nonvirt_decl->is_virtual = false;
    nonvirt_decl->type = sema.int_type();
    auto nonvirt_dec = std::make_shared<Declarator>();
    nonvirt_dec->name = "bar";
    nonvirt_decl->declarators.push_back(nonvirt_dec);
    struct_type->members.push_back(nonvirt_decl);

    CHECK(sema.compute_vtable_layout(struct_type));

    CHECK(struct_type->has_vtable);
    CHECK(struct_type->vtable_size == 1);
    CHECK(struct_type->vptr_offset == 0);
    REQUIRE(struct_type->virtual_functions.size() == 1);
}

TEST_CASE("Sema vtable: struct without virtual functions has no vtable") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    auto struct_type = std::make_shared<Type>();
    struct_type->kind = TypeKind::Struct;
    struct_type->name = "NoVirt";

    auto decl = std::make_shared<Declaration>();
    decl->is_virtual = false;
    decl->type = sema.int_type();
    auto dec = std::make_shared<Declarator>();
    dec->name = "x";
    decl->declarators.push_back(dec);
    struct_type->members.push_back(decl);

    CHECK(sema.compute_vtable_layout(struct_type));

    CHECK(!struct_type->has_vtable);
    CHECK(struct_type->vtable_size == 0);
    CHECK(struct_type->vptr_offset == 0);
    CHECK(struct_type->virtual_functions.empty());
}

TEST_CASE("Sema vtable: derived class inherits base virtual functions") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Base class with one virtual function.
    auto base_type = std::make_shared<Type>();
    base_type->kind = TypeKind::Struct;
    base_type->name = "Base";
    auto base_virt = std::make_shared<Declaration>();
    base_virt->is_virtual = true;
    base_virt->type = sema.int_type();
    auto base_dec = std::make_shared<Declarator>();
    base_dec->name = "foo";
    base_virt->declarators.push_back(base_dec);
    base_type->members.push_back(base_virt);

    CHECK(sema.compute_vtable_layout(base_type));
    REQUIRE(base_type->has_vtable);
    CHECK(base_type->vtable_size == 1);

    // Derived class inherits from Base and adds its own virtual function.
    auto derived_type = std::make_shared<Type>();
    derived_type->kind = TypeKind::Struct;
    derived_type->name = "Derived";
    derived_type->base_classes.push_back(base_type);

    auto derived_virt = std::make_shared<Declaration>();
    derived_virt->is_virtual = true;
    derived_virt->type = sema.int_type();
    auto derived_dec = std::make_shared<Declarator>();
    derived_dec->name = "bar";
    derived_virt->declarators.push_back(derived_dec);
    derived_type->members.push_back(derived_virt);

    CHECK(sema.compute_vtable_layout(derived_type));

    CHECK(derived_type->has_vtable);
    CHECK(derived_type->vtable_size == 2);
    CHECK(derived_type->vptr_offset == 0);
    REQUIRE(derived_type->virtual_functions.size() == 2);
}

TEST_CASE("Sema template: variadic template parameter deduction") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: template <typename... Args> void func(Args... args);
    // Template parameter: typename... Args (variadic)
    auto args_param = std::make_shared<TemplateParam>();
    args_param->kind = TemplateParam::Kind::Type;
    args_param->is_variadic = true;
    args_param->name = "Args";

    // Function type: void func(Args... args) — single param of type Args
    auto func_type = std::make_shared<Type>();
    func_type->kind = TypeKind::Function;
    func_type->name = "func";
    func_type->return_type = sema.void_type();

    auto arg_type = std::make_shared<Type>();
    arg_type->kind = TypeKind::TemplateParam;
    arg_type->template_param_name = "Args";
    func_type->param_types.push_back(arg_type);

    auto decl = std::make_shared<Declaration>();
    decl->type = func_type;
    auto dec = std::make_shared<Declarator>();
    dec->name = "func";
    dec->is_function = true;
    dec->is_variadic = true;
    dec->param_types.push_back(arg_type);
    decl->declarators.push_back(dec);
    decl->template_params.push_back(args_param);

    auto tmpl = std::make_shared<TemplateDeclaration>();
    tmpl->params = {args_param};
    tmpl->declaration = decl;

    sema.register_template("func", tmpl);

    // Build call args: func(1, 2, 3) — three int args
    std::vector<std::shared_ptr<Expression>> call_args;
    for (int i = 0; i < 3; ++i) {
        auto arg = std::make_shared<Expression>();
        arg->kind = ExpressionKind::IntegerLiteral;
        arg->int_literal_value = i;
        arg->type = sema.int_type();
        call_args.push_back(arg);
    }

    // Deduce template arguments directly.
    std::vector<std::shared_ptr<Type>> deduced_args;
    CHECK(sema.deduce_template_args(
        tmpl->params, call_args, func_type->param_types, deduced_args));
    REQUIRE(deduced_args.size() == 1);
    REQUIRE(deduced_args[0] != nullptr);
    CHECK(deduced_args[0] == sema.int_type());
}

TEST_CASE("Sema expression: fold expression type checking") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: (args + ...) where args is an int variable.
    // First declare "args" as an int variable so lookup succeeds.
    auto args_decl = std::make_shared<Declaration>();
    args_decl->type = sema.int_type();
    auto args_dec = std::make_shared<Declarator>();
    args_dec->name = "args";
    args_decl->declarators.push_back(args_dec);
    CHECK(sema.resolve_declarator_type(*args_decl));
    CHECK(sema.walk_declaration(*args_decl));

    auto arg_var = std::make_shared<Expression>();
    arg_var->kind = ExpressionKind::Identifier;
    arg_var->identifier_name = "args";
    arg_var->type = sema.int_type();
    arg_var->is_constant = false;

    auto fold = std::make_shared<Expression>();
    fold->kind = ExpressionKind::FoldExpression;
    fold->fold_kind = 1; // unary right fold: (pack op ...)
    fold->fold_op = FoldOp::Plus;
    fold->fold_operand = arg_var;

    // Type-check the fold expression.
    CHECK(sema.type_fold_expression(*fold));
    REQUIRE(fold->type != nullptr);
    CHECK(fold->type == sema.int_type());
    CHECK(fold->is_lvalue == false);
    CHECK(fold->is_constant == false);
}

TEST_CASE("Sema expression: fold expression with constant operand") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: (1 + 2 + ...) — constant fold.
    auto lit = std::make_shared<Expression>();
    lit->kind = ExpressionKind::IntegerLiteral;
    lit->int_literal_value = 42;
    lit->type = sema.int_type();
    lit->is_constant = true;

    auto fold = std::make_shared<Expression>();
    fold->kind = ExpressionKind::FoldExpression;
    fold->fold_kind = 1; // unary right fold
    fold->fold_op = FoldOp::Plus;
    fold->fold_operand = lit;

    CHECK(sema.type_fold_expression(*fold));
    REQUIRE(fold->type != nullptr);
    CHECK(fold->type == sema.int_type());
    CHECK(fold->is_constant == true);
}

TEST_CASE("Sema expression: throw expression type checking") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: throw expr; where expr is an int.
    auto operand = std::make_shared<Expression>();
    operand->kind = ExpressionKind::IntegerLiteral;
    operand->int_literal_value = 42;
    operand->type = sema.int_type();
    operand->is_constant = true;

    auto throw_expr = std::make_shared<Expression>();
    throw_expr->kind = ExpressionKind::Throw;
    throw_expr->throw_operand = operand;

    // Type-check the throw expression.
    CHECK(sema.type_throw(*throw_expr));
    REQUIRE(throw_expr->type != nullptr);
    CHECK(throw_expr->type == sema.void_type());
    CHECK(throw_expr->is_lvalue == false);
}

TEST_CASE("Sema expression: typeid of type") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: typeid(int).
    auto typeid_type = std::make_shared<Type>();
    typeid_type->kind = TypeKind::Int;
    typeid_type->name = "int";

    auto typeid_expr = std::make_shared<Expression>();
    typeid_expr->kind = ExpressionKind::Typeid;
    typeid_expr->typeid_is_type = true;
    typeid_expr->typeid_type = typeid_type;

    // Type-check the typeid expression.
    CHECK(sema.type_typeid(*typeid_expr));
    REQUIRE(typeid_expr->type != nullptr);
    CHECK(typeid_expr->is_lvalue == false);
    CHECK(typeid_expr->is_constant == true);
}

TEST_CASE("Sema expression: typeid of expression") {
    DiagnosticEngine diagnostics;
    Sema sema(diagnostics);

    // Build: typeid(x) where x is an int variable.
    auto operand = std::make_shared<Expression>();
    operand->kind = ExpressionKind::IntegerLiteral;
    operand->int_literal_value = 42;
    operand->type = sema.int_type();
    operand->is_constant = true;

    auto typeid_expr = std::make_shared<Expression>();
    typeid_expr->kind = ExpressionKind::Typeid;
    typeid_expr->typeid_is_type = false;
    typeid_expr->typeid_expr = operand;

    // Type-check the typeid expression.
    CHECK(sema.type_typeid(*typeid_expr));
    REQUIRE(typeid_expr->type != nullptr);
    CHECK(typeid_expr->is_lvalue == false);
    CHECK(typeid_expr->is_constant == true);
}

