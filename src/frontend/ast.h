#ifndef KRC_AST_H
#define KRC_AST_H

#include "source_manager.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace krc {

struct TranslationUnit;
struct FunctionDef;
struct Declaration;
struct Statement;
struct Expression;
struct Type;
struct Declarator;
struct Parameter;
struct Initializer;
struct Enumerator;
struct Designator;
struct TemplateParam;
struct CompoundLiteral;

struct AstNode {
    SourceLocation location;
    virtual ~AstNode() = default;
};

enum class TypeKind : std::uint8_t {
    Void, Bool, Char, Short, Int, Long, LongLong,
    Float, Double, LongDouble,
    Enum, Struct, Union,
    Pointer, Reference, RvalueReference, Array, Function, Typedef, Incomplete,
    TemplateParam,  // M6: template parameter type (e.g. `T` in `template <typename T>`)
    Auto,           // M6: `auto` placeholder type, resolved from initializer
    Decltype,       // M6: `decltype(expr)` placeholder type
    InitializerList, // M6: std::initializer_list<T>
    Pair,           // M10: std::pair<T, U>
    Tuple,          // M10: std::tuple<Ts...>
    UniquePtr,      // M10: std::unique_ptr<T>
    SharedPtr,      // M10: std::shared_ptr<T>
};

struct Type : AstNode {
    TypeKind kind = TypeKind::Void;
    std::string name;
    std::uint32_t size = 0;
    std::uint32_t align = 0;
    std::shared_ptr<Type> pointee;
    std::shared_ptr<Type> element_type;
    std::uint64_t array_length = 0;
    bool is_vla = false;
    std::shared_ptr<Type> return_type;
    std::vector<std::shared_ptr<Type>> param_types;
    bool is_variadic = false;
    std::vector<std::shared_ptr<Declaration>> members;
    // C++ base classes (for single inheritance).
    std::vector<std::shared_ptr<Type>> base_classes;
    // M6: for TypeKind::TemplateParam, the name of the template parameter.
    std::string template_param_name;
    // M6: for TypeKind::Decltype, the expression whose type is queried.
    std::shared_ptr<Expression> decltype_expr;
    // M10: for TypeKind::Pair, the two element types (first, second).
    std::shared_ptr<Type> pair_first_type;
    std::shared_ptr<Type> pair_second_type;
    // M10: for TypeKind::Tuple, the element types (T1, T2, ...).
    std::vector<std::shared_ptr<Type>> tuple_element_types;
    // M10: for TypeKind::UniquePtr/SharedPtr, the pointee type T.
    std::shared_ptr<Type> smart_ptr_type;
    std::shared_ptr<Type> underlying;
    // M6: vtable layout for classes with virtual functions.
    bool has_vtable = false;          // true if this struct has virtual functions
    std::uint32_t vptr_offset = 0;    // byte offset of the hidden vptr member
    std::vector<std::shared_ptr<Declaration>> virtual_functions;  // ordered vtable entries
    std::uint32_t vtable_size = 0;    // number of vtable entries
    // Enum enumerators (name -> value).
    std::vector<std::shared_ptr<Enumerator>> enumerators;
};

// Template parameter: `typename T`, `class T`, or `int N`.
// `is_variadic` is true for `typename... T` (variadic type parameter pack).
struct TemplateParam : AstNode {
    enum class Kind { Type, Value };
    Kind kind = Kind::Type;
    std::string name;
    bool is_variadic = false;  // M10: variadic template parameter pack
    // For type parameters: the default type (e.g. `typename T = int`).
    std::shared_ptr<Type> default_type;
    // For value parameters: the parameter type (e.g. `int N`).
    std::shared_ptr<Type> param_type;
    // For value parameters: the default value expression.
    std::shared_ptr<Expression> default_value;
};

// A declaration wrapped with template parameters, e.g.
//   template <typename T> class Foo { ... };
//   template <typename T> T max(T a, T b) { ... }
struct TemplateDeclaration : AstNode {
    std::vector<std::shared_ptr<TemplateParam>> params;
    std::shared_ptr<Declaration> declaration;
};

// A template instantiation: a template declaration specialized with
// concrete template arguments. Used for template instantiation in M6.
struct TemplateInstantiation : AstNode {
    std::shared_ptr<TemplateDeclaration> template_decl;
    // Template arguments substituted for the template parameters, in order.
    std::vector<std::shared_ptr<Type>> template_args;
    // The specialized declaration produced by substitution.
    std::shared_ptr<Declaration> specialization;
};

std::shared_ptr<Type> void_type();
std::shared_ptr<Type> bool_type();
std::shared_ptr<Type> char_type();
std::shared_ptr<Type> short_type();
std::shared_ptr<Type> int_type();
std::shared_ptr<Type> long_type();
std::shared_ptr<Type> long_long_type();
std::shared_ptr<Type> unsigned_type();
std::shared_ptr<Type> float_type();
std::shared_ptr<Type> double_type();
std::shared_ptr<Type> auto_type();   // M6: `auto` placeholder type
std::shared_ptr<Type> decltype_type(); // M6: `decltype` placeholder type

struct Declarator : AstNode {
    std::string name;
    // Number of pointer levels (e.g. 2 for `void**`).  Tracked via
    // pointer_qualified size: each '*' pushes one entry.
    bool is_pointer = false;
    std::vector<bool> pointer_qualified;
    std::vector<std::pair<std::uint64_t, bool>> array_sizes;
    bool is_function = false;
    std::vector<std::string> param_names;
    // Parameter types captured by the parser (C++ overloading support).
    std::vector<std::shared_ptr<Type>> param_types;
    bool is_variadic = false;
    // M6: `auto` type deduction. When true, the type is resolved from the
    // initializer expression after it has been walked.
    bool auto_deduced = false;
    // M6: Resolved `auto` type (set by Sema after deduction).
    std::shared_ptr<Type> auto_resolved_type;
};

enum class StorageClass : std::uint8_t {
    None, Typedef, Extern, Static, Auto, Register, ThreadLocal,
};

enum class Linkage : std::uint8_t {
    None, Internal, External,
};

// Access specifiers for class members (C++ only).
enum class AccessSpecifier : std::uint8_t {
    Public, Private, Protected,
};

struct Declaration : AstNode {
    StorageClass storage = StorageClass::None;
    Linkage linkage = Linkage::None;
    AccessSpecifier access = AccessSpecifier::Public;
    bool is_typedef = false;
    bool is_extern = false;
    bool is_static = false;
    bool is_thread_local = false;  // M10: C++ thread_local storage duration
    bool is_const = false;
    bool is_volatile = false;
    bool is_restrict = false;
    bool is_inline = false;
    bool is_noreturn = false;
    bool is_noexcept = false;     // C++: noexcept function specifier
    bool is_constexpr = false;    // C++11: constexpr function/variable specifier
    bool has_alignas = false;     // C++: alignas specifier present
    std::shared_ptr<Expression> alignas_expr;  // alignas(expression) or alignas(type-id)
    bool is_constructor = false;  // C++: name matches enclosing class
    bool is_destructor = false;   // C++: ~name
    bool is_virtual = false;      // C++: virtual member function
    // C++ namespace: non-empty when this declaration is a namespace block.
    bool is_namespace = false;
    std::string namespace_name;
    // Members of a namespace block (declarations inside `namespace X { ... }`).
    std::vector<std::shared_ptr<Declaration>> namespace_members;
    std::shared_ptr<Type> type;
    std::vector<std::shared_ptr<Declarator>> declarators;
    std::vector<std::shared_ptr<Initializer>> initializers;
    std::shared_ptr<FunctionDef> function_def;
    // Template parameters, if this is a template declaration
    // (e.g. `template <typename T> ...`).
    std::vector<std::shared_ptr<TemplateParam>> template_params;
};

struct Enumerator : AstNode {
    std::string name;
    std::int64_t value = 0;
    bool has_explicit_value = false;
};

struct Designator : AstNode {
    enum class Kind { Field, ArrayIndex };
    Kind kind = Kind::Field;
    std::string field_name;
    std::uint64_t array_index = 0;
};

struct Initializer : AstNode {
    enum class Kind { Expression, List, Designation };
    Kind kind = Kind::Expression;
    std::shared_ptr<Expression> expr;
    std::vector<std::shared_ptr<Initializer>> subinitializers;
    std::vector<Designator> designators;
};

enum class StatementKind : std::uint8_t {
    Label, Case, Default, Compound, Expression,
    If, Switch, While, DoWhile, For,
    Goto, Continue, Break, Return, Declaration, Null,
    StructuredBinding,  // C++17: auto [a, b] = expr;
    Try, Catch,         // M10: C++ exception handling (try/catch)
};

struct Statement : AstNode {
    StatementKind kind = StatementKind::Null;
    std::string label_name;
    std::shared_ptr<Expression> case_value;
    std::shared_ptr<Declaration> declaration;
    std::shared_ptr<Expression> expression;
    std::vector<std::shared_ptr<Statement>> block_items;
    std::shared_ptr<Expression> condition;
    std::shared_ptr<Statement> then_branch;
    std::shared_ptr<Statement> else_branch;
    std::shared_ptr<Statement> loop_body;
    std::shared_ptr<Statement> for_init;
    std::shared_ptr<Expression> for_condition;
    std::shared_ptr<Expression> for_increment;
    bool is_range_for = false;        // C++11: for ( decl : expr ) ...
    std::shared_ptr<Expression> for_range_expr;  // the iterable expression
    std::string goto_target;
    std::shared_ptr<Expression> return_value;
    bool is_constexpr_if = false;  // C++17: if constexpr ( condition ) ...
    bool is_structured_binding = false;  // C++17: auto [a, b] = expr;
    std::vector<std::string> bound_names;  // names being bound
    std::shared_ptr<Expression> bound_initializer;  // the initializer expression
    std::shared_ptr<Type> bound_type;  // the type (or auto placeholder)

    // M10: C++ exception handling (try/catch)
    // For StatementKind::Try: the try block and its catch clauses.
    std::shared_ptr<Statement> try_block;       // the try block body
    std::vector<std::shared_ptr<Statement>> catch_clauses;  // catch handlers
    // For StatementKind::Catch: the exception declaration and handler body.
    std::shared_ptr<Declaration> catch_exception_decl;  // e.g. `int e` in `catch (int e)`
    std::shared_ptr<Statement> catch_body;  // the catch block body
};

enum class ExpressionKind : std::uint8_t {
    IntegerLiteral, FloatingLiteral, CharLiteral, StringLiteral, BooleanLiteral,
    Identifier, This, FunctionCall, Unary, Binary, Conditional, Assignment,
    Cast, SizeofExpr, SizeofType, MemberAccess, ArraySubscript,
    CompoundLiteral, StatementExpr, StringLiteralConcat,
    New, Delete,
    Lambda, Move, Forward,
    FoldExpression,  // M10: C++17 fold expression (pack op ...)
    Throw, Typeid, VaArg,  // M10: C++ exception handling and RTTI; va_arg builtin
};

enum class UnaryOp : std::uint8_t {
    PreInc, PreDec, PostInc, PostDec,
    UnaryPlus, UnaryMinus, LogicalNot, BitwiseNot,
    AddressOf, Deref, Sizeof,
};

enum class BinaryOp : std::uint8_t {
    Mul, Div, Rem, Add, Sub, Shl, Shr,
    Lt, Gt, Le, Ge, Eq, Ne,
    BitAnd, BitXor, BitOr,
    LogicalAnd, LogicalOr,
    Assign, AddAssign, SubAssign, MulAssign, DivAssign, RemAssign,
    ShlAssign, ShrAssign, AndAssign, XorAssign, OrAssign, Comma,
};

enum class AssignmentOp : std::uint8_t {
    Assign, MulAssign, DivAssign, RemAssign, AddAssign, SubAssign,
    ShlAssign, ShrAssign, AndAssign, XorAssign, OrAssign,
};

// M10: fold expression operator (the operator between pack and ...)
enum class FoldOp : std::uint8_t {
    Plus, Minus, Mul, Div, Rem,
    Shl, Shr,
    BitAnd, BitXor, BitOr,
    LogicalAnd, LogicalOr,
    Comma,
    Assign, AddAssign, SubAssign, MulAssign, DivAssign, RemAssign,
    ShlAssign, ShrAssign, AndAssign, XorAssign, OrAssign,
    Eq, Ne, Lt, Gt, Le, Ge,
};

struct Expression : AstNode {
    ExpressionKind kind = ExpressionKind::Identifier;
    std::shared_ptr<Type> type;
    bool is_lvalue = false;
    bool is_constant = false;
    std::int64_t int_value = 0;
    bool bool_value = false;
    std::string identifier_name;
    std::string string_value;
    char char_value = 0;
    long long int_literal_value = 0;
    double float_literal_value = 0.0;
    UnaryOp unary_op = UnaryOp::UnaryPlus;
    std::shared_ptr<Expression> unary_operand;
    std::shared_ptr<Type> unary_type;
    BinaryOp binary_op = BinaryOp::Add;
    std::shared_ptr<Expression> binary_left;
    std::shared_ptr<Expression> binary_right;
    AssignmentOp assign_op = AssignmentOp::Assign;
    std::shared_ptr<Expression> assign_left;
    std::shared_ptr<Expression> assign_right;
    std::shared_ptr<Expression> cond_condition;
    std::shared_ptr<Expression> cond_true;
    std::shared_ptr<Expression> cond_false;
    std::shared_ptr<Expression> callee;
    std::vector<std::shared_ptr<Expression>> call_args;
    std::shared_ptr<Type> cast_type;
    std::shared_ptr<Expression> cast_operand;
    std::shared_ptr<Expression> sizeof_operand;
    std::shared_ptr<Type> sizeof_type;
    bool is_ptr_member_access = false;
    std::shared_ptr<Expression> member_base;
    std::string member_name;
    std::shared_ptr<Expression> array_base;
    std::shared_ptr<Expression> array_index;
    std::shared_ptr<Type> compound_type;
    std::shared_ptr<Initializer> compound_initializer;
    // For ExpressionKind::New: the type being allocated.
    std::shared_ptr<Type> new_type;
    // For ExpressionKind::New: optional size expression for `new[]`.
    std::shared_ptr<Expression> new_alloc_expr;
    // For ExpressionKind::Delete: the expression being deleted.
    std::shared_ptr<Expression> delete_operand;
    // For ExpressionKind::Lambda: lambda capture list (empty = no captures).
    std::vector<std::string> lambda_captures;
    // For ExpressionKind::Lambda: lambda parameters.
    std::vector<std::shared_ptr<Parameter>> lambda_params;
    // For ExpressionKind::Lambda: trailing return type (empty = auto-deduced).
    std::shared_ptr<Type> lambda_return_type;
    // For ExpressionKind::Lambda: lambda body.
    std::shared_ptr<Statement> lambda_body;
    // For ExpressionKind::Move: the expression being moved (std::move(x)).
    std::shared_ptr<Expression> move_operand;

    // For ExpressionKind::Forward: the expression being forwarded (std::forward<T>(x)).
    std::shared_ptr<Expression> forward_operand;
    // The type T in std::forward<T>(x), used to determine the value category.
    std::shared_ptr<Type> forward_type;

    // For ExpressionKind::FoldExpression: C++17 fold expression.
    // fold_kind: 0 = unary right (pack op ...), 1 = unary left (... op pack),
    //            2 = binary left (pack op ... op init), 3 = binary right (init op ... op pack).
    std::uint8_t fold_kind = 0;
    FoldOp fold_op = FoldOp::Plus;
    std::shared_ptr<Expression> fold_operand;    // the pack expression
    std::shared_ptr<Expression> fold_init;       // init for binary folds (null for unary)

    // M10: C++ exception handling and RTTI
    // For ExpressionKind::Throw: the expression being thrown.
    std::shared_ptr<Expression> throw_operand;
    // For ExpressionKind::Typeid: the type or expression being queried.
    std::shared_ptr<Type> typeid_type;
    std::shared_ptr<Expression> typeid_expr;
    bool typeid_is_type = false;  // true if typeid(T), false if typeid(expr)
    // For ExpressionKind::VaArg: __builtin_va_arg(ap, type).
    std::shared_ptr<Expression> va_arg_list;
    std::shared_ptr<Type> va_arg_type;
};

struct FunctionDef : AstNode {
    std::shared_ptr<Declaration> declaration;
    std::shared_ptr<Statement> body;
};

struct Parameter : AstNode {
    std::shared_ptr<Type> type;
    std::shared_ptr<Declarator> declarator;
    bool is_variadic_param = false;
};

struct TranslationUnit : AstNode {
    std::vector<std::shared_ptr<Declaration>> declarations;
};

// Dump a human-readable AST to the output stream (debugging).
void dump_ast(const TranslationUnit& tu, std::ostream& out);

} // namespace krc

#endif // KRC_AST_H
