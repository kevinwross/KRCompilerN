#include "ast.h"

#include <iostream>

namespace krc {

namespace {

std::shared_ptr<Type> g_void_type;
std::shared_ptr<Type> g_bool_type;
std::shared_ptr<Type> g_char_type;
std::shared_ptr<Type> g_short_type;
std::shared_ptr<Type> g_int_type;
std::shared_ptr<Type> g_long_type;
std::shared_ptr<Type> g_long_long_type;
std::shared_ptr<Type> g_unsigned_type;
std::shared_ptr<Type> g_float_type;
std::shared_ptr<Type> g_double_type;
std::shared_ptr<Type> g_auto_type;
std::shared_ptr<Type> g_decltype_type;

void init_builtin_types() {
    if (g_void_type) return;
    g_void_type = std::make_shared<Type>();
    g_void_type->kind = TypeKind::Void;
    g_void_type->name = "void";
    g_bool_type = std::make_shared<Type>();
    g_bool_type->kind = TypeKind::Bool;
    g_bool_type->name = "_Bool";
    g_bool_type->size = 1;
    g_bool_type->align = 1;
    g_char_type = std::make_shared<Type>();
    g_char_type->kind = TypeKind::Char;
    g_char_type->name = "char";
    g_char_type->size = 1;
    g_char_type->align = 1;
    g_short_type = std::make_shared<Type>();
    g_short_type->kind = TypeKind::Short;
    g_short_type->name = "short";
    g_short_type->size = 2;
    g_short_type->align = 2;
    g_int_type = std::make_shared<Type>();
    g_int_type->kind = TypeKind::Int;
    g_int_type->name = "int";
    g_int_type->size = 4;
    g_int_type->align = 4;
    g_long_type = std::make_shared<Type>();
    g_long_type->kind = TypeKind::Long;
    g_long_type->name = "long";
    g_long_type->size = 4;
    g_long_type->align = 4;
    g_long_long_type = std::make_shared<Type>();
    g_long_long_type->kind = TypeKind::LongLong;
    g_long_long_type->name = "long long";
    g_long_long_type->size = 8;
    g_long_long_type->align = 8;
    g_unsigned_type = std::make_shared<Type>();
    g_unsigned_type->kind = TypeKind::Int;
    g_unsigned_type->name = "unsigned int";
    g_unsigned_type->size = 4;
    g_unsigned_type->align = 4;
    g_float_type = std::make_shared<Type>();
    g_float_type->kind = TypeKind::Float;
    g_float_type->name = "float";
    g_float_type->size = 4;
    g_float_type->align = 4;
    g_double_type = std::make_shared<Type>();
    g_double_type->kind = TypeKind::Double;
    g_double_type->name = "double";
    g_double_type->size = 8;
    g_double_type->align = 8;
    g_auto_type = std::make_shared<Type>();
    g_auto_type->kind = TypeKind::Auto;
    g_auto_type->name = "auto";
    g_auto_type->size = 0;
    g_auto_type->align = 1;
    g_decltype_type = std::make_shared<Type>();
    g_decltype_type->kind = TypeKind::Decltype;
    g_decltype_type->name = "decltype";
    g_decltype_type->size = 0;
    g_decltype_type->align = 1;
}

} // namespace

std::shared_ptr<Type> void_type() { init_builtin_types(); return g_void_type; }
std::shared_ptr<Type> bool_type() { init_builtin_types(); return g_bool_type; }
std::shared_ptr<Type> char_type() { init_builtin_types(); return g_char_type; }
std::shared_ptr<Type> short_type() { init_builtin_types(); return g_short_type; }
std::shared_ptr<Type> int_type() { init_builtin_types(); return g_int_type; }
std::shared_ptr<Type> long_type() { init_builtin_types(); return g_long_type; }
std::shared_ptr<Type> long_long_type() { init_builtin_types(); return g_long_long_type; }
std::shared_ptr<Type> unsigned_type() { init_builtin_types(); return g_unsigned_type; }
std::shared_ptr<Type> float_type() { init_builtin_types(); return g_float_type; }
std::shared_ptr<Type> double_type() { init_builtin_types(); return g_double_type; }
std::shared_ptr<Type> auto_type() { init_builtin_types(); return g_auto_type; }
std::shared_ptr<Type> decltype_type() { init_builtin_types(); return g_decltype_type; }

namespace {

std::string indent_string(int depth) {
    return std::string(static_cast<std::size_t>(depth) * 2, ' ');
}

void dump_type(std::ostream& out, const std::shared_ptr<Type>& type, int depth) {
    if (!type) {
        out << "null-type";
        return;
    }
    const std::string indent = indent_string(depth);
    out << indent << "type: ";
    switch (type->kind) {
        case TypeKind::Void: out << "void"; break;
        case TypeKind::Bool: out << "_Bool"; break;
        case TypeKind::Char: out << "char"; break;
        case TypeKind::Short: out << "short"; break;
        case TypeKind::Int: out << "int"; break;
        case TypeKind::Long: out << "long"; break;
        case TypeKind::LongLong: out << "long long"; break;
        case TypeKind::Float: out << "float"; break;
        case TypeKind::Double: out << "double"; break;
        case TypeKind::LongDouble: out << "long double"; break;
        case TypeKind::Enum: out << "enum"; break;
        case TypeKind::Struct: out << "struct"; break;
        case TypeKind::Union: out << "union"; break;
        case TypeKind::Pointer: out << "ptr"; break;
        case TypeKind::Reference: out << "ref"; break;
        case TypeKind::RvalueReference: out << "rref"; break;
        case TypeKind::InitializerList: out << "init_list"; break;
        case TypeKind::Array: out << "array"; break;
        case TypeKind::Function: out << "function"; break;
        case TypeKind::Typedef: out << "typedef"; break;
        case TypeKind::Incomplete: out << "incomplete"; break;
        case TypeKind::TemplateParam: out << "typeparam"; break;
        case TypeKind::Auto: out << "auto"; break;
        case TypeKind::Decltype: out << "decltype"; break;
        case TypeKind::Pair: out << "pair"; break;
        case TypeKind::Tuple: out << "tuple"; break;
        case TypeKind::UniquePtr: out << "unique_ptr"; break;
        case TypeKind::SharedPtr: out << "shared_ptr"; break;
    }
    if (!type->name.empty()) {
        out << " '" << type->name << "'";
    }
    if (type->size > 0) {
        out << " size=" << type->size;
    }
    if (type->align > 0) {
        out << " align=" << type->align;
    }
    if (type->kind == TypeKind::Struct && type->has_vtable) {
        out << " vtable size=" << type->vtable_size
            << " vptr_offset=" << type->vptr_offset;
    }
    if (type->kind == TypeKind::Pointer && type->pointee) {
        out << " ->\n";
        dump_type(out, type->pointee, depth + 1);
    }
    if (type->kind == TypeKind::Array && type->element_type) {
        out << " len=" << type->array_length << "\n";
        dump_type(out, type->element_type, depth + 1);
    }
    if (type->kind == TypeKind::Function && type->return_type) {
        out << " returns:\n";
        dump_type(out, type->return_type, depth + 1);
    }
    out << "\n";
}

} // namespace

void dump_declaration(std::ostream& out, const std::shared_ptr<Declaration>& decl, int depth);

void dump_statement(std::ostream& out, const std::shared_ptr<Statement>& stmt, int depth);

void dump_expression(std::ostream& out, const std::shared_ptr<Expression>& expr, int depth) {
    if (!expr) {
        out << "null-expr";
        return;
    }
    const std::string indent = indent_string(depth);
    out << indent << "expr: ";
    switch (expr->kind) {
        case ExpressionKind::IntegerLiteral:
            out << "integer " << expr->int_literal_value; break;
        case ExpressionKind::FloatingLiteral:
            out << "floating " << expr->float_literal_value; break;
        case ExpressionKind::CharLiteral:
            out << "char '" << expr->char_value << "'"; break;
        case ExpressionKind::StringLiteral:
            out << "string \"" << expr->string_value << "\""; break;
        case ExpressionKind::BooleanLiteral:
            out << (expr->bool_value ? "true" : "false"); break;
        case ExpressionKind::Identifier:
            out << "ident " << expr->identifier_name; break;
        case ExpressionKind::This:
            out << "this"; break;
        case ExpressionKind::New:
            out << "new"; break;
        case ExpressionKind::Delete:
            out << "delete"; break;
        case ExpressionKind::Lambda:
            out << "lambda"; break;
        case ExpressionKind::Move:
            out << "move"; break;
        case ExpressionKind::Forward:
            out << "forward"; break;
        case ExpressionKind::FunctionCall:
            out << "call"; break;
        case ExpressionKind::Unary:
            out << "unary"; break;
        case ExpressionKind::Binary:
            out << "binary"; break;
        case ExpressionKind::Conditional:
            out << "conditional"; break;
        case ExpressionKind::Assignment:
            out << "assignment"; break;
        case ExpressionKind::Cast:
            out << "cast"; break;
        case ExpressionKind::SizeofExpr:
            out << "sizeof-expr"; break;
        case ExpressionKind::SizeofType:
            out << "sizeof-type"; break;
        case ExpressionKind::MemberAccess:
            out << "member-access"; break;
        case ExpressionKind::ArraySubscript:
            out << "array-subscript"; break;
        case ExpressionKind::CompoundLiteral:
            out << "compound-literal"; break;
        case ExpressionKind::StatementExpr:
            out << "statement-expr"; break;
        case ExpressionKind::StringLiteralConcat:
            out << "string-concat"; break;
        case ExpressionKind::FoldExpression:
            out << "fold-expression"; break;
        case ExpressionKind::Throw:
            out << "throw"; break;
        case ExpressionKind::Typeid:
            out << "typeid"; break;
        case ExpressionKind::VaArg:
            out << "va-arg"; break;
    }
    if (expr->type) {
        out << " type=";
        dump_type(out, expr->type, 0);
    }
    out << "\n";
}

void dump_statement(std::ostream& out, const std::shared_ptr<Statement>& stmt, int depth) {
    if (!stmt) {
        out << indent_string(depth) << "stmt: null\n";
        return;
    }
    const std::string indent = indent_string(depth);
    out << indent << "stmt: ";
    switch (stmt->kind) {
        case StatementKind::Compound:
            out << "compound\n";
            for (const auto& item : stmt->block_items) {
                dump_statement(out, item, depth + 1);
            }
            break;
        case StatementKind::Expression:
            out << "expression\n";
            dump_expression(out, stmt->expression, depth + 1);
            break;
        case StatementKind::Return:
            out << "return";
            if (stmt->return_value) {
                out << "\n";
                dump_expression(out, stmt->return_value, depth + 1);
            } else {
                out << "\n";
            }
            break;
        case StatementKind::If:
            out << "if\n";
            out << indent_string(depth + 1) << "cond:\n";
            dump_expression(out, stmt->condition, depth + 2);
            out << indent_string(depth + 1) << "then:\n";
            dump_statement(out, stmt->then_branch, depth + 2);
            if (stmt->else_branch) {
                out << indent_string(depth + 1) << "else:\n";
                dump_statement(out, stmt->else_branch, depth + 2);
            }
            break;
        case StatementKind::While:
            out << "while\n";
            out << indent_string(depth + 1) << "cond:\n";
            dump_expression(out, stmt->condition, depth + 2);
            out << indent_string(depth + 1) << "body:\n";
            dump_statement(out, stmt->loop_body, depth + 2);
            break;
        case StatementKind::For:
            out << "for\n";
            break;
        case StatementKind::Declaration:
            out << "declaration\n";
            dump_declaration(out, stmt->declaration, depth + 1);
            break;
        case StatementKind::Break:
            out << "break\n";
            break;
        case StatementKind::Continue:
            out << "continue\n";
            break;
        case StatementKind::Null:
            out << "null\n";
            break;
        case StatementKind::Try:
            out << "try\n";
            out << indent_string(depth + 1) << "block:\n";
            dump_statement(out, stmt->try_block, depth + 2);
            for (const auto& cc : stmt->catch_clauses) {
                out << indent_string(depth + 1) << "catch:\n";
                dump_statement(out, cc, depth + 2);
            }
            break;
        case StatementKind::Catch:
            out << "catch\n";
            if (stmt->catch_exception_decl) {
                out << indent_string(depth + 1) << "exception-decl:\n";
                dump_declaration(out, stmt->catch_exception_decl, depth + 2);
            }
            if (stmt->catch_body) {
                out << indent_string(depth + 1) << "body:\n";
                dump_statement(out, stmt->catch_body, depth + 2);
            }
            break;
        default:
            out << "other\n";
            break;
    }
}

void dump_declaration(std::ostream& out, const std::shared_ptr<Declaration>& decl, int depth) {
    if (!decl) {
        out << indent_string(depth) << "decl: null\n";
        return;
    }
    const std::string indent = indent_string(depth);
    out << indent << "decl: ";
    if (decl->is_typedef) out << "typedef ";
    if (decl->is_extern) out << "extern ";
    if (decl->is_static) out << "static ";
    if (decl->is_const) out << "const ";
    out << "\n";
    if (decl->type) {
        out << indent_string(depth + 1) << "type:\n";
        dump_type(out, decl->type, depth + 2);
    }
    for (const auto& d : decl->declarators) {
        out << indent_string(depth + 1) << "declarator: " << d->name;
        if (d->is_pointer) out << " [ptr]";
        if (d->is_function) out << " [func]";
        out << "\n";
    }
}

void dump_ast(const TranslationUnit& tu, std::ostream& out) {
    out << "translation-unit:\n";
    for (const auto& decl : tu.declarations) {
        dump_declaration(out, decl, 1);
    }
}

} // namespace krc
