#include "mangle.h"
#include "ast.h"

#include <doctest.h>

#include <memory>
#include <string>
#include <vector>

using namespace krc;

// Forward declarations so helpers can be used in any order.
static std::shared_ptr<Type> int_t();
static std::shared_ptr<Type> void_t();
static std::shared_ptr<Type> ptr_t();

static std::shared_ptr<Type> int_t() {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Int;
    t->name = "int";
    t->size = 4;
    t->align = 4;
    return t;
}

static std::shared_ptr<Type> void_t() {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Void;
    t->name = "void";
    return t;
}

static std::shared_ptr<Type> bool_t() {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Bool;
    t->name = "bool";
    return t;
}

static std::shared_ptr<Type> char_t() {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Char;
    t->name = "char";
    return t;
}

static std::shared_ptr<Type> dbl_t() {
    // Note: "double" is a standard typedef name in <cmath>, so the helper is
    // named dbl_t to avoid a clash.
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Double;
    t->name = "double";
    return t;
}

static std::shared_ptr<Type> long_long_t() {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::LongLong;
    t->name = "long long";
    return t;
}

static std::shared_ptr<Type> ptr_t() {
    auto p = std::make_shared<Type>();
    p->kind = TypeKind::Pointer;
    p->pointee = int_t();
    return p;
}

static std::vector<std::shared_ptr<Type>> params(
    std::initializer_list<std::shared_ptr<Type>> init) {
    return std::vector<std::shared_ptr<Type>>(init);
}

TEST_CASE("Mangle builtin types") {
    CHECK(Mangler::mangle_type(int_t()) == "i");
    CHECK(Mangler::mangle_type(void_t()) == "v");
    CHECK(Mangler::mangle_type(bool_t()) == "b");
    CHECK(Mangler::mangle_type(char_t()) == "c");
    CHECK(Mangler::mangle_type(dbl_t()) == "d");
    CHECK(Mangler::mangle_type(long_long_t()) == "x");
}

TEST_CASE("Mangle pointer types") {
    // int*
    auto p = std::make_shared<Type>();
    p->kind = TypeKind::Pointer;
    p->pointee = int_t();
    CHECK(Mangler::mangle_type(p) == "Pi");
    // int**
    auto p2 = std::make_shared<Type>();
    p2->kind = TypeKind::Pointer;
    p2->pointee = p;
    CHECK(Mangler::mangle_type(p2) == "PPi");
    // void*
    auto vp = std::make_shared<Type>();
    vp->kind = TypeKind::Pointer;
    vp->pointee = void_t();
    CHECK(Mangler::mangle_type(vp) == "Pv");
}

TEST_CASE("Mangle reference types") {
    // int&
    auto r = std::make_shared<Type>();
    r->kind = TypeKind::Reference;
    r->pointee = int_t();
    CHECK(Mangler::mangle_type(r) == "Ri");
}

TEST_CASE("Mangle array types") {
    // int[5]
    auto a = std::make_shared<Type>();
    a->kind = TypeKind::Array;
    a->array_length = 5;
    a->element_type = int_t();
    CHECK(Mangler::mangle_type(a) == "A5_i");
    // int[0]
    auto a0 = std::make_shared<Type>();
    a0->kind = TypeKind::Array;
    a0->array_length = 0;
    a0->element_type = int_t();
    CHECK(Mangler::mangle_type(a0) == "A0_i");
}

TEST_CASE("Mangle function types") {
    // int(int, double)
    auto ft = std::make_shared<Type>();
    ft->kind = TypeKind::Function;
    ft->param_types = params({int_t(), dbl_t()});
    ft->return_type = int_t();
    CHECK(Mangler::mangle_type(ft) == "FidEi");
    // void()
    auto fv = std::make_shared<Type>();
    fv->kind = TypeKind::Function;
    fv->return_type = void_t();
    CHECK(Mangler::mangle_type(fv) == "FEv");
}

TEST_CASE("Mangle simple function name") {
    // f(int) -> _Z1fi
    CHECK(Mangler::mangle_function({}, "f", params({int_t()}), int_t()) ==
          "_Z1fi");
}

TEST_CASE("Mangle function name with no params") {
    // f(void) -> _Z1fv
    CHECK(Mangler::mangle_function({}, "f", params({}), void_t()) == "_Z1fv");
}

TEST_CASE("Mangle overloaded functions differ by params") {
    CHECK(Mangler::mangle_function({}, "f", params({int_t()}), void_t()) ==
          "_Z1fi");
    CHECK(Mangler::mangle_function({}, "f", params({dbl_t()}), void_t()) ==
          "_Z1fd");
    CHECK(Mangler::mangle_function({}, "f", params({int_t(), int_t()}), void_t()) ==
          "_Z1fii");
}

TEST_CASE("Mangle qualified name (namespace)") {
    // A::f(int) -> _ZN1A1fEi  (return type NOT encoded at top level)
    CHECK(Mangler::mangle_function({"A"}, "f", params({int_t()}), void_t()) ==
          "_ZN1A1fEi");
}

TEST_CASE("Mangle nested qualified name") {
    // A::B::f(int) -> _ZN1A1B1fEi
    CHECK(Mangler::mangle_function({"A", "B"}, "f", params({int_t()}),
                                   void_t()) == "_ZN1A1B1fEi");
}

TEST_CASE("Mangle constructor and destructor") {
    // Foo::Foo() -> _ZN3FooC1Ev
    CHECK(Mangler::mangle_function({"Foo"}, "Foo", params({}), void_t(),
                                   true, false) == "_ZN3FooC1Ev");
    // Foo::~Foo() -> _ZN3FooD1Ev
    CHECK(Mangler::mangle_function({"Foo"}, "Foo", params({}), void_t(),
                                   false, true) == "_ZN3FooD1Ev");
}

TEST_CASE("Mangle data name") {
    // A::x -> _ZN1A1xE
    CHECK(Mangler::mangle_data({"A"}, "x") == "_ZN1A1xE");
    // global x -> _Z1x (no return type, no wrapper at top level)
    CHECK(Mangler::mangle_data({}, "x") == "_Z1x");
}

TEST_CASE("Mangle name length boundaries") {
    // Single-char name
    CHECK(Mangler::mangle_function({}, "a", params({}), void_t()) == "_Z1av");
    // Two-char name
    CHECK(Mangler::mangle_function({}, "ab", params({}), void_t()) == "_Z2abv");
    // Multi-digit length: 10-char name
    std::string long_name(10, 'x');
    std::string expected = "_Z10" + long_name + "v";
    CHECK(Mangler::mangle_function({}, long_name, params({}), void_t()) ==
          expected);
}

TEST_CASE("Mangle pointer and reference parameters") {
    // f(int*) -> _Z1fPi
    CHECK(Mangler::mangle_function({}, "f", params({ptr_t()}), void_t()) ==
          "_Z1fPi");
    // f(int&) -> _Z1fRi
    auto r = std::make_shared<Type>();
    r->kind = TypeKind::Reference;
    r->pointee = int_t();
    CHECK(Mangler::mangle_function({}, "f", params({r}), void_t()) == "_Z1fRi");
}

TEST_CASE("Mangle array parameter") {
    // f(int[5])  -> _Z1fA5_i  (return type NOT encoded at top level)
    auto a = std::make_shared<Type>();
    a->kind = TypeKind::Array;
    a->array_length = 5;
    a->element_type = int_t();
    CHECK(Mangler::mangle_function({}, "f", params({a}), void_t()) ==
          "_Z1fA5_i");
}

TEST_CASE("Mangle function returning pointer") {
    // f() -> int*  => _Z1fv  (empty params, return type NOT encoded)
    CHECK(Mangler::mangle_function({}, "f", params({}), ptr_t()) == "_Z1fv");
}
