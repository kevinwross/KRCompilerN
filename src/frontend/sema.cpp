#include "sema.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace krc {

// =====================================================================
// Scope
// =====================================================================

std::shared_ptr<Symbol> Scope::lookup(std::string_view name) const {
    const Scope* s = this;
    while (s != nullptr) {
        auto it = s->symbols.find(std::string(name));
        if (it != s->symbols.end()) {
            return it->second;
        }
        s = s->parent;
    }
    return nullptr;
}

bool Scope::insert(std::string_view name, std::shared_ptr<Symbol> sym) {
    std::string key(name);
    if (symbols.find(key) != symbols.end()) {
        return false; // already declared in this scope
    }
    symbols[key] = std::move(sym);
    return true;
}

// =====================================================================
// Sema
// =====================================================================

Sema::Sema(DiagnosticEngine& diagnostics) : diagnostics_(diagnostics) {
    // Create builtin types. These are canonical singleton instances.
    auto make_builtin = [&](TypeKind kind, std::string_view name,
                            std::uint32_t size, std::uint32_t align) {
        auto t = std::make_shared<Type>();
        t->kind = kind;
        t->name = name;
        t->size = size;
        t->align = align;
        return t;
    };

    builtin_void_ = make_builtin(TypeKind::Void, "void", 0, 1);
    builtin_bool_ = make_builtin(TypeKind::Bool, "_Bool", 1, 1);
    builtin_char_ = make_builtin(TypeKind::Char, "char", 1, 1);
    builtin_short_ = make_builtin(TypeKind::Short, "short", 2, 2);
    builtin_int_ = make_builtin(TypeKind::Int, "int", 4, 4);
    builtin_long_ = make_builtin(TypeKind::Long, "long", 4, 4);   // ILP32
    builtin_long_long_ =
        make_builtin(TypeKind::LongLong, "long long", 8, 8);
    builtin_float_ = make_builtin(TypeKind::Float, "float", 4, 4);
    builtin_double_ =
        make_builtin(TypeKind::Double, "double", 8, 8);

    // Set up the file (global) scope as the first scope on the stack.
    scope_stack_.emplace_back(std::make_unique<Scope>());
    file_scope_ = scope_stack_.back().get();
}

// =====================================================================
// Scope management
// =====================================================================

void Sema::push_scope() {
    auto child = std::make_unique<Scope>();
    child->parent = scope_stack_.empty() ? nullptr : scope_stack_.back().get();
    scope_stack_.push_back(std::move(child));
}

void Sema::pop_scope() {
    if (scope_stack_.size() > 1) {
        scope_stack_.pop_back();
    }
}

std::shared_ptr<Symbol> Sema::lookup(std::string_view name) const {
    const Scope* s = scope_stack_.empty() ? nullptr : scope_stack_.back().get();
    while (s != nullptr) {
        auto it = s->symbols.find(std::string(name));
        if (it != s->symbols.end()) {
            return it->second;
        }
        s = s->parent;
    }
    return nullptr;
}

bool Sema::insert_symbol(std::string_view name, std::shared_ptr<Symbol> sym) {
    return current_scope().insert(name, std::move(sym));
}

void Sema::enter_function_scope() {
    push_scope();
}

void Sema::leave_function_scope() {
    pop_scope();
}

// =====================================================================
// Type classification helpers
// =====================================================================

bool Sema::is_integer_type(const std::shared_ptr<Type>& t) const {
    if (!t) return false;
    switch (t->kind) {
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::Short:
        case TypeKind::Int:
        case TypeKind::Long:
        case TypeKind::LongLong:
        case TypeKind::Enum:
            return true;
        default:
            return false;
    }
}

bool Sema::is_floating_type(const std::shared_ptr<Type>& t) const {
    if (!t) return false;
    switch (t->kind) {
        case TypeKind::Float:
        case TypeKind::Double:
        case TypeKind::LongDouble:
            return true;
        default:
            return false;
    }
}

bool Sema::is_arithmetic_type(const std::shared_ptr<Type>& t) const {
    return is_integer_type(t) || is_floating_type(t);
}

bool Sema::is_pointer_type(const std::shared_ptr<Type>& t) const {
    return t && t->kind == TypeKind::Pointer;
}

bool Sema::is_void_type(const std::shared_ptr<Type>& t) const {
    return t && t->kind == TypeKind::Void;
}

bool Sema::is_array_type(const std::shared_ptr<Type>& t) const {
    return t && t->kind == TypeKind::Array;
}

bool Sema::is_function_type(const std::shared_ptr<Type>& t) const {
    return t && t->kind == TypeKind::Function;
}

bool Sema::is_scalar_type(const std::shared_ptr<Type>& t) const {
    return is_arithmetic_type(t) || is_pointer_type(t);
}

bool Sema::types_compatible(const std::shared_ptr<Type>& a,
                            const std::shared_ptr<Type>& b) const {
    if (!a || !b) return false;
    if (a == b) return true; // same canonical type
    if (a->kind != b->kind) return false;

    switch (a->kind) {
        case TypeKind::Void:
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::Short:
        case TypeKind::Int:
        case TypeKind::Long:
        case TypeKind::LongLong:
        case TypeKind::Float:
        case TypeKind::Double:
        case TypeKind::LongDouble:
            // Scalar builtins: compatible if same kind (qualifiers handled
            // separately at the declaration level).
            return true;
        case TypeKind::Enum:
        case TypeKind::Struct:
        case TypeKind::Union:
            // Tag types: compatible only if same declaration (same pointer).
            return a == b;
        case TypeKind::Pointer:
            return types_compatible(a->pointee, b->pointee);
        case TypeKind::Array: {
            if (a->array_length != b->array_length) return false;
            return types_compatible(a->element_type, b->element_type);
        }
        case TypeKind::Function: {
            if (a->is_variadic != b->is_variadic) return false;
            if (!types_compatible(a->return_type, b->return_type)) return false;
            if (a->param_types.size() != b->param_types.size()) return false;
            for (std::size_t i = 0; i < a->param_types.size(); ++i) {
                if (!types_compatible(a->param_types[i], b->param_types[i])) {
                    return false;
                }
            }
            return true;
        }
        case TypeKind::Typedef:
        case TypeKind::Incomplete:
            return a == b;
    }
    return false;
}

// =====================================================================
// Usual arithmetic conversions (C11 6.3.1.8)
// =====================================================================

std::shared_ptr<Type> Sema::integer_promotion(
    const std::shared_ptr<Type>& t) const {
    if (!t || !is_integer_type(t)) return t;

    // Types already at int rank or higher are unchanged.
    switch (t->kind) {
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::Short:
        case TypeKind::Enum:
            // Promote to int (KR32: int is 32-bit, can represent all values
            // of char/short).
            return builtin_int_;
        case TypeKind::Int:
        case TypeKind::Long:
        case TypeKind::LongLong:
            return t;
        default:
            return t;
    }
}

std::shared_ptr<Type> Sema::usual_arithmetic_conversions(
    const std::shared_ptr<Type>& a,
    const std::shared_ptr<Type>& b) const {
    if (!a || !b) return a ? a : b;

    // If either is floating, the other is converted to floating.
    if (is_floating_type(a) || is_floating_type(b)) {
        if (is_floating_type(a) && is_floating_type(b)) {
            // Double wins over float (KR32: long double is not implemented).
            if (a->kind == TypeKind::Double ||
                b->kind == TypeKind::Double) {
                return builtin_double_;
            }
            return builtin_float_;
        }
        if (is_floating_type(a)) {
            // b is integer: promote b to match a's floating type.
            return a;
        }
        return b;
    }

    // Both are integer.
    auto pa = integer_promotion(a);
    auto pb = integer_promotion(b);

    // If both have the same type after promotion, that's the result.
    if (types_compatible(pa, pb)) return pa;

    // Signed/unsigned rules (KR32: all ints are 32-bit except long long).
    // Simplified: if both are int-sized after promotion, result is int
    // unless one is unsigned.
    // For this implementation, we keep it simple: use the "larger" rank.
    // If either is long long, result is long long.
    if (pa->kind == TypeKind::LongLong || pb->kind == TypeKind::LongLong) {
        return builtin_long_long_;
    }
    // Otherwise both are int/long (32-bit on KR32); result is int.
    return builtin_int_;
}

// =====================================================================
// Conversions
// =====================================================================

std::shared_ptr<Type> Sema::array_to_pointer_decay(
    const std::shared_ptr<Type>& t) const {
    if (!t || t->kind != TypeKind::Array) return t;
    return pointer_to(t->element_type);
}

std::shared_ptr<Type> Sema::function_to_pointer_decay(
    const std::shared_ptr<Type>& t) const {
    if (!t || t->kind != TypeKind::Function) return t;
    return pointer_to(t);
}

void Sema::lvalue_to_rvalue(Expression& expr) const {
    if (!expr.is_lvalue) return;
    // The type of an lvalue is the object type; reading it yields the
    // value type. For arrays and functions, decay applies first.
    if (expr.type) {
        if (expr.type->kind == TypeKind::Array) {
            expr.type = array_to_pointer_decay(expr.type);
        } else if (expr.type->kind == TypeKind::Function) {
            expr.type = function_to_pointer_decay(expr.type);
        }
        // Scalar lvalue: type stays the same, just not an lvalue anymore.
    }
    expr.is_lvalue = false;
}

bool Sema::check_assignment(const std::shared_ptr<Type>& left,
                            const std::shared_ptr<Type>& right,
                            SourceLocation loc) const {
    if (!left || !right) return false;

    // Same type is fine.
    if (types_compatible(left, right)) return true;

    // Pointer to void can be assigned from any object pointer, and vice
    // versa.
    if (is_pointer_type(left) && is_pointer_type(right)) {
        if (is_void_type(left->pointee) || is_void_type(right->pointee)) {
            return true;
        }
        return types_compatible(left->pointee, right->pointee);
    }

    // Both arithmetic: assignment is allowed (with conversion).
    if (is_arithmetic_type(left) && is_arithmetic_type(right)) return true;

    // Left is atomic struct/union with compatible right — not implemented.

    // Pointer to bool: _Bool can be assigned to any scalar.
    if (is_scalar_type(left) && is_integer_type(right) &&
        right->kind == TypeKind::Bool) return true;

    // Pointer on left, null pointer constant (0) on right.
    if (is_pointer_type(left) && is_integer_type(right)) {
        return false; // only constant 0 allowed, checked at expression level
    }

    error(DiagnosticID{4002, "sema-error-assign"},
          "incompatible types in assignment", loc);
    return false;
}

std::shared_ptr<Type> Sema::pointer_to(
    const std::shared_ptr<Type>& pointee) const {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Pointer;
    t->pointee = pointee;
    t->size = 4;   // KR32: pointers are 32-bit
    t->align = 4;
    return t;
}

std::shared_ptr<Type> Sema::array_of(
    const std::shared_ptr<Type>& element, std::uint64_t length) const {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Array;
    t->element_type = element;
    t->array_length = length;
    t->size = static_cast<std::uint32_t>(element->size * length);
    t->align = element->align;
    return t;
}

std::shared_ptr<Type> Sema::function_type(
    const std::shared_ptr<Type>& return_type,
    const std::vector<std::shared_ptr<Type>>& param_types,
    bool is_variadic) const {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Function;
    t->return_type = return_type;
    t->param_types = param_types;
    t->is_variadic = is_variadic;
    t->size = 4; // function type size is pointer-sized on KR32
    t->align = 4;
    return t;
}

// =====================================================================
// Error reporting
// =====================================================================

void Sema::error(DiagnosticID id, std::string_view message,
                 SourceLocation loc) const {
    diagnostics_.error(id, std::string(message), loc);
}

void Sema::warning(DiagnosticID id, std::string_view message,
                   SourceLocation loc) const {
    diagnostics_.warning(id, std::string(message), loc);
}

void Sema::note(std::string_view message, SourceLocation loc) const {
    diagnostics_.note(DiagnosticID{0, "sema-note"}, std::string(message), loc);
}

bool Sema::fail(DiagnosticID id, std::string_view message,
                SourceLocation loc) const {
    error(id, message, loc);
    return false;
}

// =====================================================================
// Main entry point
// =====================================================================

bool Sema::analyze(TranslationUnit& tu) {
    return walk_translation_unit(tu);
}

bool Sema::walk_translation_unit(TranslationUnit& tu) {
    for (auto& decl : tu.declarations) {
        if (!walk_declaration(*decl)) {
            return false;
        }
    }
    return true;
}

bool Sema::walk_declaration(Declaration& decl) {
    // If this declaration carries a function definition, handle it as a
    // function definition (which creates a new scope for the body).
    if (decl.function_def) {
        return check_function_definition(*decl.function_def);
    }

    // Resolve the type of the declaration from its specifiers and
    // declarators.
    if (!resolve_declarator_type(decl)) {
        return false;
    }

    // If this is a typedef, register the name.
    if (decl.is_typedef) {
        for (auto& d : decl.declarators) {
            auto sym = std::make_shared<Symbol>();
            sym->name = d->name;
            sym->type = decl.type;
            sym->is_typedef = true;
            sym->storage = decl.storage;
            sym->is_definition = true;
            sym->location = d->location;
            if (!insert_symbol(d->name, sym)) {
                return fail(DiagnosticID{4003, "sema-error-redefinition"},
                            "redefinition of typedef '" + d->name + "'",
                            d->location);
            }
            typedef_names_.insert(d->name);
        }
        return true;
    }

    // For each declarator, create/register symbols and check types.
    std::size_t init_idx = 0;
    for (auto& d : decl.declarators) {
        // Build the full type for this declarator from the base type plus
        // declarator modifiers (pointer, array, function).
        std::shared_ptr<Type> ty = decl.type;
        if (d->is_pointer) {
            ty = pointer_to(ty);
        }
        for (const auto& [size, is_vla] : d->array_sizes) {
            ty = array_of(ty, size);
        }
        if (d->is_function) {
            std::vector<std::shared_ptr<Type>> param_types;
            for (const auto& pn : d->param_names) {
                (void)pn;
                param_types.push_back(builtin_int_);
            }
            ty = function_type(ty, param_types, d->is_variadic);
        }

        auto sym = std::make_shared<Symbol>();
        sym->name = d->name;
        sym->type = ty;
        sym->storage = decl.storage;
        sym->linkage = decl.linkage;
        sym->is_const = decl.is_const;
        sym->is_volatile = decl.is_volatile;
        sym->is_restrict = decl.is_restrict;
        sym->is_static = decl.is_static;
        sym->is_extern = decl.is_extern;
        sym->is_definition = decl.function_def != nullptr;
        sym->location = d->location;

        if (!insert_symbol(d->name, sym)) {
            return fail(DiagnosticID{4003, "sema-error-redefinition"},
                        "redefinition of '" + d->name + "'", d->location);
        }

        // Walk the corresponding initializer, if any.
        if (init_idx < decl.initializers.size()) {
            auto& init = decl.initializers[init_idx];
            if (init->kind == Initializer::Kind::Expression && init->expr) {
                // Check initializer type against declared type.
                if (!type_expression(*init->expr)) {
                    return false;
                }
                auto init_type = init->expr->type;
                if (is_array_type(init_type)) {
                    init_type = array_to_pointer_decay(init_type);
                }
                if (is_function_type(init_type)) {
                    init_type = function_to_pointer_decay(init_type);
                }
                lvalue_to_rvalue(*init->expr);
                if (!check_assignment(ty, init_type, init->expr->location)) {
                    return false;
                }
                init->expr->is_lvalue = false; // not an lvalue after init
            } else {
                if (!walk_initializer(*init)) {
                    return false;
                }
            }
        }
        ++init_idx;
    }

    return true;
}

bool Sema::resolve_declarator_type(Declaration& decl) {
    // Start from the base type (already set by the parser in decl.type).
    std::shared_ptr<Type> base = decl.type;
    if (!base) {
        base = builtin_int_; // C11: missing type specifiers defaults to int
        decl.type = base;
    }
    return true;
}

bool Sema::check_typedef(Declaration& /*decl*/) {
    // Typedefs are handled in walk_declaration.
    return true;
}

bool Sema::check_function_definition(FunctionDef& fdef) {
    auto& decl = *fdef.declaration;
    if (!decl.declarators.empty()) {
        auto& d = decl.declarators[0];

        // Register the function name in the file scope (parent of the
        // function body scope) so it can be referenced by other functions.
        if (d->is_function) {
            std::vector<std::shared_ptr<Type>> param_types;
            for (const auto& pn : d->param_names) {
                (void)pn;
                param_types.push_back(builtin_int_);
            }
            auto func_type = function_type(decl.type, param_types, d->is_variadic);

            auto sym = std::make_shared<Symbol>();
            sym->name = d->name;
            sym->type = func_type;
            sym->storage = decl.storage;
            sym->linkage = decl.linkage;
            sym->is_definition = true;
            sym->location = d->location;

            // Insert into file scope (scope_stack_[0]).
            if (!file_scope_->insert(d->name, sym)) {
                return fail(DiagnosticID{4003, "sema-error-redefinition"},
                            "redefinition of '" + d->name + "'", d->location);
            }
        }

        // Create a new scope for the function body.
        push_scope();

        // Register parameters in the function scope.
        if (d->is_function) {
            for (const auto& pname : d->param_names) {
                auto sym = std::make_shared<Symbol>();
                sym->name = pname;
                sym->type = builtin_int_; // default param type
                sym->storage = StorageClass::Auto;
                sym->location = d->location;
                insert_symbol(pname, sym);
            }
        }

        // Walk the function body.
        if (fdef.body) {
            if (!walk_statement(*fdef.body)) {
                pop_scope();
                return false;
            }
        }

        pop_scope();
    }
    return true;
}

// =====================================================================
// Statement walking
// =====================================================================

bool Sema::walk_statement(Statement& stmt) {
    switch (stmt.kind) {
        case StatementKind::Compound: {
            push_scope();
            for (auto& bi : stmt.block_items) {
                if (!walk_statement(*bi)) {
                    pop_scope();
                    return false;
                }
            }
            pop_scope();
            return true;
        }
        case StatementKind::Declaration:
            if (stmt.declaration) {
                return walk_declaration(*stmt.declaration);
            }
            return true;
        case StatementKind::Expression:
            if (stmt.expression) {
                return type_expression(*stmt.expression);
            }
            return true;
        case StatementKind::If: {
            if (stmt.condition) {
                if (!type_expression(*stmt.condition)) return false;
                // Condition must be scalar.
                if (!is_scalar_type(stmt.condition->type)) {
                    return fail(
                        DiagnosticID{4004, "sema-error-condition"},
                        "if condition must have scalar type",
                        stmt.condition->location);
                }
                lvalue_to_rvalue(*stmt.condition);
            }
            if (stmt.then_branch) {
                if (!walk_statement(*stmt.then_branch)) return false;
            }
            if (stmt.else_branch) {
                if (!walk_statement(*stmt.else_branch)) return false;
            }
            return true;
        }
        case StatementKind::While:
        case StatementKind::DoWhile: {
            if (stmt.condition) {
                if (!type_expression(*stmt.condition)) return false;
                if (!is_scalar_type(stmt.condition->type)) {
                    return fail(
                        DiagnosticID{4004, "sema-error-condition"},
                        "loop condition must have scalar type",
                        stmt.condition->location);
                }
                lvalue_to_rvalue(*stmt.condition);
            }
            push_scope();
            if (stmt.loop_body) {
                if (!walk_statement(*stmt.loop_body)) {
                    pop_scope();
                    return false;
                }
            }
            pop_scope();
            return true;
        }
        case StatementKind::For: {
            push_scope();
            if (stmt.for_init) {
                if (stmt.for_init->kind == StatementKind::Declaration) {
                    if (stmt.for_init->declaration) {
                        if (!walk_declaration(*stmt.for_init->declaration)) {
                            pop_scope();
                            return false;
                        }
                    }
                } else {
                    if (!type_expression(*stmt.for_init->expression)) {
                        pop_scope();
                        return false;
                    }
                }
            }
            if (stmt.for_condition) {
                if (!type_expression(*stmt.for_condition)) {
                    pop_scope();
                    return false;
                }
                if (!is_scalar_type(stmt.for_condition->type)) {
                    return fail(
                        DiagnosticID{4004, "sema-error-condition"},
                        "for condition must have scalar type",
                        stmt.for_condition->location);
                }
                lvalue_to_rvalue(*stmt.for_condition);
            }
            if (stmt.for_increment) {
                if (!type_expression(*stmt.for_increment)) {
                    pop_scope();
                    return false;
                }
            }
            if (stmt.loop_body) {
                if (!walk_statement(*stmt.loop_body)) {
                    pop_scope();
                    return false;
                }
            }
            pop_scope();
            return true;
        }
        case StatementKind::Return: {
            if (stmt.return_value) {
                if (!type_expression(*stmt.return_value)) return false;
                lvalue_to_rvalue(*stmt.return_value);
            }
            return true;
        }
        case StatementKind::Break:
        case StatementKind::Continue:
        case StatementKind::Goto:
        case StatementKind::Label:
        case StatementKind::Case:
        case StatementKind::Default:
        case StatementKind::Switch:
        case StatementKind::Null:
            return true;
    }
    return true;
}

// =====================================================================
// Expression typing
// =====================================================================

bool Sema::walk_expression(Expression& expr) {
    return type_expression(expr);
}

bool Sema::type_expression(Expression& expr) {
    switch (expr.kind) {
        case ExpressionKind::Identifier:
            return type_identifier(expr);
        case ExpressionKind::IntegerLiteral:
            return type_integer_literal(expr);
        case ExpressionKind::FloatingLiteral:
            return type_floating_literal(expr);
        case ExpressionKind::CharLiteral:
            return type_char_literal(expr);
        case ExpressionKind::StringLiteral:
            return type_string_literal(expr);
        case ExpressionKind::Unary:
            return type_unary(expr);
        case ExpressionKind::Binary:
            return type_binary(expr);
        case ExpressionKind::Conditional:
            return type_conditional(expr);
        case ExpressionKind::Assignment:
            return type_assignment(expr);
        case ExpressionKind::Cast:
            return type_cast(expr);
        case ExpressionKind::SizeofExpr:
        case ExpressionKind::SizeofType:
            return type_sizeof(expr);
        case ExpressionKind::FunctionCall:
            return type_function_call(expr);
        case ExpressionKind::MemberAccess:
            return type_member_access(expr);
        case ExpressionKind::ArraySubscript:
            return type_array_subscript(expr);
        case ExpressionKind::CompoundLiteral:
            return type_compound_literal(expr);
        default:
            // Unknown expression kind — untyped.
            return true;
    }
}

bool Sema::type_identifier(Expression& expr) {
    auto sym = lookup(expr.identifier_name);
    if (!sym) {
        return fail(DiagnosticID{4001, "sema-error-undeclared"},
                    "use of undeclared identifier '" + expr.identifier_name + "'",
                    expr.location);
    }
    expr.type = sym->type;
    expr.is_lvalue = true; // identifiers refer to objects (lvalues)
    return true;
}

bool Sema::type_integer_literal(Expression& expr) {
    // Integer literals have type int (or larger if the value doesn't fit).
    expr.type = builtin_int_;
    expr.is_constant = true;
    expr.is_lvalue = false;
    return true;
}

bool Sema::type_floating_literal(Expression& expr) {
    expr.type = builtin_double_;
    expr.is_constant = true;
    expr.is_lvalue = false;
    return true;
}

bool Sema::type_char_literal(Expression& expr) {
    // In C11, character constants have type int.
    expr.type = builtin_int_;
    expr.is_constant = true;
    expr.is_lvalue = false;
    return true;
}

bool Sema::type_string_literal(Expression& expr) {
    // String literals have type "array of char" (or wchar_t, etc.).
    expr.type = array_of(builtin_char_, expr.string_value.size() + 1);
    expr.is_constant = true;
    expr.is_lvalue = true; // string literals are lvalues in C11
    return true;
}

bool Sema::type_unary(Expression& expr) {
    if (!type_expression(*expr.unary_operand)) return false;

    switch (expr.unary_op) {
        case UnaryOp::UnaryPlus:
        case UnaryOp::UnaryMinus: {
            if (!is_arithmetic_type(expr.unary_operand->type)) {
                return fail(DiagnosticID{4005, "sema-error-unary"},
                            "unary operator requires arithmetic type",
                            expr.location);
            }
            lvalue_to_rvalue(*expr.unary_operand);
            auto promoted = integer_promotion(expr.unary_operand->type);
            expr.type = promoted;
            expr.is_lvalue = false;
            return true;
        }
        case UnaryOp::LogicalNot: {
            if (!is_scalar_type(expr.unary_operand->type)) {
                return fail(DiagnosticID{4005, "sema-error-unary"},
                            "logical not requires scalar type",
                            expr.location);
            }
            lvalue_to_rvalue(*expr.unary_operand);
            expr.type = builtin_int_;
            expr.is_lvalue = false;
            return true;
        }
        case UnaryOp::BitwiseNot: {
            if (!is_integer_type(expr.unary_operand->type)) {
                return fail(DiagnosticID{4005, "sema-error-unary"},
                            "bitwise not requires integer type",
                            expr.location);
            }
            lvalue_to_rvalue(*expr.unary_operand);
            expr.type = integer_promotion(expr.unary_operand->type);
            expr.is_lvalue = false;
            return true;
        }
        case UnaryOp::AddressOf: {
            // &expr: pointer to the type of expr. expr must be an lvalue
            // or a function designator.
            if (!expr.unary_operand->is_lvalue &&
                !is_function_type(expr.unary_operand->type)) {
                return fail(
                    DiagnosticID{4005, "sema-error-unary"},
                    "cannot take address of non-lvalue", expr.location);
            }
            expr.type = pointer_to(expr.unary_operand->type);
            expr.is_lvalue = false;
            return true;
        }
        case UnaryOp::Deref: {
            if (!is_pointer_type(expr.unary_operand->type)) {
                return fail(
                    DiagnosticID{4005, "sema-error-unary"},
                    "dereference of non-pointer type", expr.location);
            }
            lvalue_to_rvalue(*expr.unary_operand);
            expr.type = expr.unary_operand->type->pointee;
            expr.is_lvalue = true;
            return true;
        }
        case UnaryOp::Sizeof: {
            // sizeof is a compile-time operator; operand is not evaluated.
            expr.type = builtin_ulong_type(); // size_t
            expr.is_constant = true;
            expr.is_lvalue = false;
            return true;
        }
        case UnaryOp::PreInc:
        case UnaryOp::PreDec:
        case UnaryOp::PostInc:
        case UnaryOp::PostDec: {
            if (!expr.unary_operand->is_lvalue) {
                return fail(DiagnosticID{4005, "sema-error-unary"},
                            "increment/decrement requires lvalue",
                            expr.location);
            }
            if (!is_scalar_type(expr.unary_operand->type)) {
                return fail(DiagnosticID{4005, "sema-error-unary"},
                            "increment/decrement requires scalar type",
                            expr.location);
            }
            lvalue_to_rvalue(*expr.unary_operand);
            expr.type = expr.unary_operand->type;
            expr.is_lvalue = (expr.unary_op == UnaryOp::PreInc ||
                              expr.unary_op == UnaryOp::PreDec);
            return true;
        }
    }
    return true;
}

bool Sema::type_binary(Expression& expr) {
    if (!type_expression(*expr.binary_left)) return false;
    if (!type_expression(*expr.binary_right)) return false;

    switch (expr.binary_op) {
        case BinaryOp::Comma: {
            // Comma: result is the type of the right operand.
            lvalue_to_rvalue(*expr.binary_right);
            expr.type = expr.binary_right->type;
            expr.is_lvalue = expr.binary_right->is_lvalue;
            return true;
        }
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
        case BinaryOp::OrAssign: {
            return type_assignment(expr);
        }
        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr: {
            if (!is_scalar_type(expr.binary_left->type) ||
                !is_scalar_type(expr.binary_right->type)) {
                return fail(DiagnosticID{4006, "sema-error-binary"},
                            "logical operator requires scalar operands",
                            expr.location);
            }
            lvalue_to_rvalue(*expr.binary_left);
            lvalue_to_rvalue(*expr.binary_right);
            expr.type = builtin_int_;
            expr.is_lvalue = false;
            return true;
        }
        case BinaryOp::Eq:
        case BinaryOp::Ne: {
            // Equality: both operands must be compatible (after promotions).
            if (!is_scalar_type(expr.binary_left->type) ||
                !is_scalar_type(expr.binary_right->type)) {
                return fail(DiagnosticID{4006, "sema-error-binary"},
                            "equality operator requires scalar operands",
                            expr.location);
            }
            lvalue_to_rvalue(*expr.binary_left);
            lvalue_to_rvalue(*expr.binary_right);
            expr.type = builtin_int_;
            expr.is_lvalue = false;
            return true;
        }
        case BinaryOp::Lt:
        case BinaryOp::Gt:
        case BinaryOp::Le:
        case BinaryOp::Ge: {
            // Relational: both operands must be compatible (after promotions).
            if (!is_scalar_type(expr.binary_left->type) ||
                !is_scalar_type(expr.binary_right->type)) {
                return fail(DiagnosticID{4006, "sema-error-binary"},
                            "relational operator requires scalar operands",
                            expr.location);
            }
            lvalue_to_rvalue(*expr.binary_left);
            lvalue_to_rvalue(*expr.binary_right);
            expr.type = builtin_int_;
            expr.is_lvalue = false;
            return true;
        }
        case BinaryOp::BitAnd:
        case BinaryOp::BitXor:
        case BinaryOp::BitOr: {
            if (!is_integer_type(expr.binary_left->type) ||
                !is_integer_type(expr.binary_right->type)) {
                return fail(DiagnosticID{4006, "sema-error-binary"},
                            "bitwise operator requires integer operands",
                            expr.location);
            }
            lvalue_to_rvalue(*expr.binary_left);
            lvalue_to_rvalue(*expr.binary_right);
            auto l = integer_promotion(expr.binary_left->type);
            auto r = integer_promotion(expr.binary_right->type);
            expr.type = usual_arithmetic_conversions(l, r);
            expr.is_lvalue = false;
            return true;
        }
        case BinaryOp::Add:
        case BinaryOp::Sub: {
            // Pointer arithmetic.
            auto lt = expr.binary_left->type;
            auto rt = expr.binary_right->type;
            lvalue_to_rvalue(*expr.binary_left);
            lvalue_to_rvalue(*expr.binary_right);

            if (is_pointer_type(lt) && is_integer_type(rt)) {
                expr.type = lt->pointee;
                expr.is_lvalue = true;
                return true;
            }
            if (is_integer_type(lt) && is_pointer_type(rt)) {
                expr.type = rt->pointee;
                expr.is_lvalue = true;
                return true;
            }
            if (is_arithmetic_type(lt) && is_arithmetic_type(rt)) {
                auto l = integer_promotion(lt);
                auto r = integer_promotion(rt);
                if (is_floating_type(lt) || is_floating_type(rt)) {
                    expr.type = usual_arithmetic_conversions(lt, r);
                } else {
                    expr.type = usual_arithmetic_conversions(l, r);
                }
                expr.is_lvalue = false;
                return true;
            }
            return fail(DiagnosticID{4006, "sema-error-binary"},
                        "invalid operands to binary + or -", expr.location);
        }
        case BinaryOp::Mul:
        case BinaryOp::Div:
        case BinaryOp::Rem:
        case BinaryOp::Shl:
        case BinaryOp::Shr: {
            if (!is_integer_type(expr.binary_left->type) ||
                !is_integer_type(expr.binary_right->type)) {
                return fail(DiagnosticID{4006, "sema-error-binary"},
                            "binary operator requires integer operands",
                            expr.location);
            }
            lvalue_to_rvalue(*expr.binary_left);
            lvalue_to_rvalue(*expr.binary_right);
            auto l = integer_promotion(expr.binary_left->type);
            auto r = integer_promotion(expr.binary_right->type);
            expr.type = usual_arithmetic_conversions(l, r);
            expr.is_lvalue = false;
            return true;
        }
    }
    return true;
}

bool Sema::type_assignment(Expression& expr) {
    if (!type_expression(*expr.assign_left)) return false;
    if (!type_expression(*expr.assign_right)) return false;

    if (!expr.assign_left->is_lvalue) {
        return fail(DiagnosticID{4007, "sema-error-assignment"},
                    "assignment to non-lvalue", expr.location);
    }

    auto left_type = expr.assign_left->type;
    auto right_type = expr.assign_right->type;

    // Array-to-pointer decay on the right.
    if (is_array_type(right_type)) {
        right_type = array_to_pointer_decay(right_type);
    }
    if (is_function_type(right_type)) {
        right_type = function_to_pointer_decay(right_type);
    }

    lvalue_to_rvalue(*expr.assign_right);

    if (!check_assignment(left_type, right_type, expr.location)) {
        return false;
    }

    expr.type = left_type;
    expr.is_lvalue = true;
    return true;
}

bool Sema::type_conditional(Expression& expr) {
    if (!type_expression(*expr.cond_condition)) return false;
    if (!type_expression(*expr.cond_true)) return false;
    if (!type_expression(*expr.cond_false)) return false;

    lvalue_to_rvalue(*expr.cond_condition);
    lvalue_to_rvalue(*expr.cond_true);
    lvalue_to_rvalue(*expr.cond_false);

    // Both branches must have compatible types.
    if (!types_compatible(expr.cond_true->type, expr.cond_false->type)) {
        return fail(DiagnosticID{4011, "sema-error-conditional"},
                    "incompatible types in conditional expression",
                    expr.location);
    }

    expr.type = expr.cond_true->type;
    expr.is_lvalue = expr.cond_true->is_lvalue && expr.cond_false->is_lvalue;
    return true;
}

bool Sema::type_cast(Expression& expr) {
    // Cast type should have been resolved by the parser; if not, default to
    // int.
    if (!expr.cast_type) {
        expr.cast_type = builtin_int_;
    }
    if (!type_expression(*expr.cast_operand)) return false;
    lvalue_to_rvalue(*expr.cast_operand);
    expr.type = expr.cast_type;
    expr.is_lvalue = false;
    return true;
}

bool Sema::type_sizeof(Expression& expr) {
    if (expr.kind == ExpressionKind::SizeofType) {
        if (!expr.sizeof_type) {
            expr.sizeof_type = builtin_int_;
        }
    } else {
        if (!type_expression(*expr.sizeof_operand)) return false;
        // sizeof does not evaluate its operand.
    }
    expr.type = builtin_ulong_type(); // size_t
    expr.is_constant = true;
    expr.is_lvalue = false;
    return true;
}

bool Sema::type_function_call(Expression& expr) {
    if (!type_expression(*expr.callee)) return false;

    auto callee_type = expr.callee->type;
    if (is_function_type(callee_type)) {
        // Function designator decays to pointer-to-function.
        lvalue_to_rvalue(*expr.callee);
    } else if (is_pointer_type(callee_type) &&
               is_function_type(callee_type->pointee)) {
        lvalue_to_rvalue(*expr.callee);
    } else {
        return fail(DiagnosticID{4008, "sema-error-call"},
                    "callee is not a function", expr.location);
    }

    // Check argument types against parameter types (simplified: no
    // prototyped checking in this stage; just type each argument).
    for (auto& arg : expr.call_args) {
        if (!type_expression(*arg)) return false;
        lvalue_to_rvalue(*arg);
    }

    expr.type = callee_type->return_type ? callee_type->return_type
                                         : builtin_void_;
    expr.is_lvalue = false;
    return true;
}

bool Sema::type_member_access(Expression& expr) {
    if (!type_expression(*expr.member_base)) return false;

    auto base_type = expr.member_base->type;
    if (is_array_type(base_type)) {
        base_type = array_to_pointer_decay(base_type);
    }
    if (is_function_type(base_type)) {
        base_type = function_to_pointer_decay(base_type);
    }

    if (is_pointer_type(base_type)) {
        // ptr->member: the pointee should be a struct/union.
        if (base_type->pointee && base_type->pointee->kind == TypeKind::Struct) {
            // Member lookup — simplified: we just mark the type as the
            // member's type (unknown without full struct layout).
            expr.type = base_type->pointee; // placeholder
            expr.is_lvalue = true;
            return true;
        }
        return fail(DiagnosticID{4009, "sema-error-member"},
                    "member access on non-struct pointer", expr.location);
    }

    if (base_type && base_type->kind == TypeKind::Struct) {
        // base.member
        expr.type = base_type; // placeholder
        expr.is_lvalue = expr.member_base->is_lvalue;
        return true;
    }

    return fail(DiagnosticID{4009, "sema-error-member"},
                "member access on non-struct type", expr.location);
}

bool Sema::type_array_subscript(Expression& expr) {
    if (!type_expression(*expr.array_base)) return false;
    if (!type_expression(*expr.array_index)) return false;

    auto base_type = expr.array_base->type;
    // array[E] is equivalent to *(array + E); array decays to pointer.
    if (is_array_type(base_type)) {
        base_type = array_to_pointer_decay(base_type);
    }
    if (is_pointer_type(base_type)) {
        if (!is_integer_type(expr.array_index->type)) {
            return fail(DiagnosticID{4010, "sema-error-subscript"},
                        "array index must be integer", expr.location);
        }
        lvalue_to_rvalue(*expr.array_base);
        lvalue_to_rvalue(*expr.array_index);
        expr.type = base_type->pointee;
        expr.is_lvalue = true;
        return true;
    }
    return fail(DiagnosticID{4010, "sema-error-subscript"},
                "subscript of non-array/pointer type", expr.location);
}

bool Sema::type_compound_literal(Expression& expr) {
    if (!expr.compound_type) {
        expr.compound_type = builtin_int_;
    }
    if (expr.compound_initializer) {
        if (!walk_initializer(*expr.compound_initializer)) return false;
    }
    expr.type = expr.compound_type;
    expr.is_lvalue = true; // compound literals are lvalues in C11
    return true;
}

// =====================================================================
// Initializer
// =====================================================================

bool Sema::walk_initializer(Initializer& init) {
    if (init.kind == Initializer::Kind::Expression) {
        if (init.expr) {
            return type_expression(*init.expr);
        }
    } else if (init.kind == Initializer::Kind::List) {
        for (auto& sub : init.subinitializers) {
            if (!walk_initializer(*sub)) return false;
        }
    } else if (init.kind == Initializer::Kind::Designation) {
        for (auto& sub : init.subinitializers) {
            if (!walk_initializer(*sub)) return false;
        }
    }
    return true;
}

// =====================================================================
// Builtin size_t type (unsigned long)
// =====================================================================

std::shared_ptr<Type> Sema::builtin_ulong_type() const {
    return builtin_long_; // on KR32, size_t is unsigned long (32-bit)
}

// =====================================================================
// walk_function_def / walk_declaration (entry from translation unit)
// =====================================================================

bool Sema::walk_function_def(FunctionDef& fdef) {
    return check_function_definition(fdef);
}

} // namespace krc
