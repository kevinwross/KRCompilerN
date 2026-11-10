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
struct CompoundLiteral;

struct AstNode {
    SourceLocation location;
    virtual ~AstNode() = default;
};

enum class TypeKind : std::uint8_t {
    Void, Bool, Char, Short, Int, Long, LongLong,
    Float, Double, LongDouble,
    Enum, Struct, Union,
    Pointer, Array, Function, Typedef, Incomplete,
};

struct Type : AstNode {
    TypeKind kind = TypeKind::Void;
    std::string_view name;
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
    std::shared_ptr<Type> underlying;
};

std::shared_ptr<Type> void_type();
std::shared_ptr<Type> bool_type();
std::shared_ptr<Type> char_type();
std::shared_ptr<Type> short_type();
std::shared_ptr<Type> int_type();
std::shared_ptr<Type> long_type();
std::shared_ptr<Type> long_long_type();
std::shared_ptr<Type> float_type();
std::shared_ptr<Type> double_type();

struct Declarator : AstNode {
    std::string name;
    bool is_pointer = false;
    std::vector<bool> pointer_qualified;
    std::vector<std::pair<std::uint64_t, bool>> array_sizes;
    bool is_function = false;
    std::vector<std::string> param_names;
    bool is_variadic = false;
};

enum class StorageClass : std::uint8_t {
    None, Typedef, Extern, Static, Auto, Register, ThreadLocal,
};

enum class Linkage : std::uint8_t {
    None, Internal, External,
};

struct Declaration : AstNode {
    StorageClass storage = StorageClass::None;
    Linkage linkage = Linkage::None;
    bool is_typedef = false;
    bool is_extern = false;
    bool is_static = false;
    bool is_const = false;
    bool is_volatile = false;
    bool is_restrict = false;
    bool is_inline = false;
    bool is_noreturn = false;
    std::shared_ptr<Type> type;
    std::vector<std::shared_ptr<Declarator>> declarators;
    std::vector<std::shared_ptr<Initializer>> initializers;
    std::shared_ptr<FunctionDef> function_def;
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
    std::string goto_target;
    std::shared_ptr<Expression> return_value;
};

enum class ExpressionKind : std::uint8_t {
    IntegerLiteral, FloatingLiteral, CharLiteral, StringLiteral,
    Identifier, FunctionCall, Unary, Binary, Conditional, Assignment,
    Cast, SizeofExpr, SizeofType, MemberAccess, ArraySubscript,
    CompoundLiteral, StatementExpr, StringLiteralConcat,
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

struct Expression : AstNode {
    ExpressionKind kind = ExpressionKind::Identifier;
    std::shared_ptr<Type> type;
    bool is_lvalue = false;
    bool is_constant = false;
    std::int64_t int_value = 0;
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
