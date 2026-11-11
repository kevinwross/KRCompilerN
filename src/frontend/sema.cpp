#include "sema.h"
#include "mangle.h"

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
        if (it != s->symbols.end() && !it->second.empty()) {
            // Return the most recently inserted symbol.
            return it->second.back();
        }
        s = s->parent;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Symbol>> Scope::lookup_all(
    std::string_view name) const {
    const Scope* s = this;
    while (s != nullptr) {
        auto it = s->symbols.find(std::string(name));
        if (it != s->symbols.end() && !it->second.empty()) {
            return it->second;
        }
        s = s->parent;
    }
    return {};
}

bool Scope::insert(std::string_view name, std::shared_ptr<Symbol> sym) {
    std::string key(name);
    auto& vec = symbols[key];
    // Prevent exact duplicate (same name and same type).
    for (const auto& existing : vec) {
        if (existing->type == sym->type) {
            return false;
        }
    }
    vec.push_back(std::move(sym));
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
    builtin_auto_ = make_builtin(TypeKind::Auto, "auto", 0, 1);
    builtin_decltype_ =
        make_builtin(TypeKind::Decltype, "decltype", 0, 1);

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
        if (it != s->symbols.end() && !it->second.empty()) {
            return it->second.back();
        }
        s = s->parent;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Symbol>> Sema::lookup_all(
    std::string_view name) const {
    const Scope* s = scope_stack_.empty() ? nullptr : scope_stack_.back().get();
    while (s != nullptr) {
        auto it = s->symbols.find(std::string(name));
        if (it != s->symbols.end() && !it->second.empty()) {
            return it->second;
        }
        s = s->parent;
    }
    return {};
}

std::shared_ptr<Symbol> Sema::lookup_member(
    const std::shared_ptr<Type>& class_type,
    std::string_view name) const {
    if (!class_type) return nullptr;
    // Class members are stored in the Type's members vector.  Search by name.
    for (const auto& m : class_type->members) {
        for (const auto& d : m->declarators) {
            if (d->name == name) {
                auto sym = std::make_shared<Symbol>();
                sym->name = d->name;
                sym->type = m->type;
                sym->storage = m->storage;
                sym->is_static = m->is_static;
                sym->is_virtual = m->is_virtual;
                sym->location = d->location;
                return sym;
            }
        }
    }
    // Single inheritance: search base classes recursively.
    for (const auto& base : class_type->base_classes) {
        if (auto sym = lookup_member(base, name)) {
            return sym;
        }
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

bool Sema::is_reference_type(const std::shared_ptr<Type>& t) const {
    return t && (t->kind == TypeKind::Reference || t->kind == TypeKind::RvalueReference);
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
        case TypeKind::Reference:
            return types_compatible(a->pointee, b->pointee);
        case TypeKind::RvalueReference:
            return types_compatible(a->pointee, b->pointee);
        case TypeKind::InitializerList:
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
        case TypeKind::TemplateParam:
            // Template parameters are compatible if they have the same name.
            return a->template_param_name == b->template_param_name;
        case TypeKind::Auto:
        case TypeKind::Decltype:
            // Placeholder types are compatible only if they are the same
            // placeholder (same pointer).
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

std::shared_ptr<Type> Sema::reference_to(
    const std::shared_ptr<Type>& pointee) const {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Reference;
    t->pointee = pointee;
    t->size = 4;   // KR32: references are 32-bit (same as pointers)
    t->align = 4;
    return t;
}

std::shared_ptr<Type> Sema::rvalue_reference_to(
    const std::shared_ptr<Type>& pointee) const {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::RvalueReference;
    t->pointee = pointee;
    t->size = 4;   // KR32: rvalue references are 32-bit (same as pointers)
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
    // C++ template declaration: register it for later instantiation.
    if (!decl.template_params.empty()) {
        auto tmpl = std::make_shared<TemplateDeclaration>();
        tmpl->params = decl.template_params;
        tmpl->declaration = std::make_shared<Declaration>(decl);
        // Clear template_params from the copy (it's the template, not an
        // instantiation).
        tmpl->declaration->template_params.clear();
        // Determine the template name from the declaration.
        std::string tmpl_name;
        if (!decl.declarators.empty()) {
            tmpl_name = decl.declarators[0]->name;
        } else if (decl.type && !decl.type->name.empty()) {
            tmpl_name = std::string(decl.type->name);
        }
        if (!tmpl_name.empty()) {
            register_template(tmpl_name, tmpl);
        }
        // Still walk the declaration body to type-check the template.
    }

    // C++ namespace: push a new scope, walk all members, then pop.
    if (decl.is_namespace) {
        push_scope();
        for (auto& member : decl.namespace_members) {
            if (!walk_declaration(*member)) {
                pop_scope();
                return false;
            }
        }
        pop_scope();
        return true;
    }

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

    // M6: Resolve `decltype(expr)` — type the expression and replace the
    // placeholder type with the expression's type.
    if (decl.type && decl.type->kind == TypeKind::Decltype && decl.type->decltype_expr) {
        if (!type_expression(*decl.type->decltype_expr)) {
            return false;
        }
        auto expr_type = decl.type->decltype_expr->type;
        if (is_array_type(expr_type)) {
            expr_type = array_to_pointer_decay(expr_type);
        }
        if (is_function_type(expr_type)) {
            expr_type = function_to_pointer_decay(expr_type);
        }
        lvalue_to_rvalue(*decl.type->decltype_expr);
        decl.type = expr_type;
    }

    // M6: Compute vtable layout for structs with virtual functions.
    if (decl.type && decl.type->kind == TypeKind::Struct) {
        if (!compute_vtable_layout(decl.type)) {
            return false;
        }
    }

    // Register a named class/struct/enum type in file scope when no
    // declarator follows (a pure type definition, e.g. `class Foo { ... };`).
    if (decl.type && decl.declarators.empty() &&
        (decl.type->kind == TypeKind::Struct ||
         decl.type->kind == TypeKind::Union ||
         decl.type->kind == TypeKind::Enum) &&
        !decl.type->name.empty()) {
        auto sym = std::make_shared<Symbol>();
        sym->name = std::string(decl.type->name);
        sym->type = decl.type;
        sym->is_definition = true;
        sym->location = decl.location;
        if (!file_scope_->insert(sym->name, sym)) {
            return fail(DiagnosticID{4003, "sema-error-redefinition"},
                        "redefinition of '" + sym->name + "'", decl.location);
        }
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
            // Use captured parameter types (C++ overloading support),
            // falling back to int for each unnamed parameter.
            std::vector<std::shared_ptr<Type>> param_types;
            if (!d->param_types.empty()) {
                param_types = d->param_types;
            } else {
                for (const auto& pn : d->param_names) {
                    (void)pn;
                    param_types.push_back(builtin_int_);
                }
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
        sym->is_thread_local = (decl.storage == StorageClass::ThreadLocal);
        sym->is_extern = decl.is_extern;
        sym->is_virtual = decl.is_virtual;
        sym->is_inline = decl.is_inline;
        sym->is_noexcept = decl.is_noexcept;
        sym->is_constexpr = decl.is_constexpr;
        sym->has_alignas = decl.has_alignas;
        sym->is_definition = decl.function_def != nullptr;
        sym->location = d->location;

        // Compute C++ mangled name for external function symbols.
        if (d->is_function && is_cpp_ && sym->linkage == Linkage::External) {
            sym->mangled_name = compute_mangled_name(*sym);
        }

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

                // M6: `auto` type deduction — resolve the placeholder type
                // from the initializer expression's type.
                if (d->auto_deduced) {
                    d->auto_resolved_type = init_type;
                    // Update the symbol's type to the deduced type.
                    sym->type = init_type;
                    // Update the declarator's type as well.
                    ty = init_type;
                }
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

bool Sema::walk_structured_binding(Statement& stmt) {
    // Type the initializer expression.
    if (!stmt.bound_initializer) {
        return fail(DiagnosticID{4016, "sema-error-structured-binding"},
                    "structured binding: missing initializer", stmt.location);
    }
    if (!type_expression(*stmt.bound_initializer)) {
        return false;
    }

    auto init_type = stmt.bound_initializer->type;
    if (!init_type) {
        return fail(DiagnosticID{4016, "sema-error-structured-binding"},
                    "structured binding: initializer has no type", stmt.location);
    }

    // If the bound type is `auto`, deduce it from the initializer.
    if (stmt.bound_type && stmt.bound_type->kind == TypeKind::Auto) {
        stmt.bound_type = init_type;
    }

    // For each bound name, create a symbol with the element type.
    // For simplicity, each binding gets the same type as the initializer
    // (full structured binding decomposition with tuples/arrays is deferred).
    for (const auto& name : stmt.bound_names) {
        auto sym = std::make_shared<Symbol>();
        sym->name = name;
        sym->type = init_type;
        sym->location = stmt.location;
        if (!insert_symbol(name, sym)) {
            return fail(DiagnosticID{4003, "sema-error-redefinition"},
                        "redefinition of '" + name + "'", stmt.location);
        }
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

    // M6: If the base type is `auto`, mark each declarator for deferred
    // type deduction. The actual type will be resolved from the
    // initializer expression in walk_declaration() after the initializer
    // has been walked.
    if (base->kind == TypeKind::Auto) {
        for (auto& d : decl.declarators) {
            d->auto_deduced = true;
        }
    }

    return true;
}

bool Sema::compute_vtable_layout(std::shared_ptr<Type> struct_type) {
    if (!struct_type || struct_type->kind != TypeKind::Struct) {
        return true;
    }

    // Check if any member is a virtual function.
    bool has_virtual = false;
    for (const auto& m : struct_type->members) {
        if (m->is_virtual) {
            has_virtual = true;
            break;
        }
    }
    if (!has_virtual) {
        return true;
    }

    // Collect virtual functions from base classes first (single inheritance).
    std::vector<std::shared_ptr<Declaration>> vfuncs;
    for (const auto& base : struct_type->base_classes) {
        if (base && base->has_vtable) {
            for (const auto& vf : base->virtual_functions) {
                vfuncs.push_back(vf);
            }
        }
    }

    // Add this class's own virtual functions (in declaration order).
    for (const auto& m : struct_type->members) {
        if (m->is_virtual && !m->declarators.empty()) {
            vfuncs.push_back(m);
        }
    }

    // Set vtable layout on the struct type.
    struct_type->has_vtable = true;
    struct_type->vptr_offset = 0;  // vptr is always the first member
    struct_type->virtual_functions = vfuncs;
    struct_type->vtable_size = static_cast<std::uint32_t>(vfuncs.size());

    // Add a hidden vptr member as the first member of the struct.
    auto vptr_decl = std::make_shared<Declaration>();
    vptr_decl->type = builtin_void_;   // pointee type; declarator makes it a pointer
    vptr_decl->storage = StorageClass::None;
    vptr_decl->is_static = false;
    vptr_decl->access = AccessSpecifier::Public;

    auto vptr_dec = std::make_shared<Declarator>();
    vptr_dec->name = "__vptr";
    vptr_dec->is_pointer = true;
    vptr_decl->declarators.push_back(vptr_dec);

    struct_type->members.insert(struct_type->members.begin(), vptr_decl);

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
            // Use captured parameter types (C++ overloading support),
            // falling back to int for each unnamed parameter.
            std::vector<std::shared_ptr<Type>> param_types;
            if (!d->param_types.empty()) {
                param_types = d->param_types;
            } else {
                for (const auto& pn : d->param_names) {
                    (void)pn;
                    param_types.push_back(builtin_int_);
                }
            }
            auto func_type = function_type(decl.type, param_types, d->is_variadic);

            auto sym = std::make_shared<Symbol>();
            sym->name = d->name;
            sym->type = func_type;
            sym->storage = decl.storage;
            sym->linkage = decl.linkage;
            sym->is_definition = true;
            sym->location = d->location;

            // Compute C++ mangled name for external function symbols.
            if (is_cpp_ && sym->linkage == Linkage::External) {
                sym->mangled_name = compute_mangled_name(*sym);
            }

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
        case StatementKind::StructuredBinding:
            return walk_structured_binding(stmt);
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
            // M6: if constexpr — evaluate the condition at compile time and
            // discard the unevaluated branch.
            if (stmt.is_constexpr_if) {
                bool cond_true = false;
                if (stmt.condition && stmt.condition->is_constant) {
                    if (stmt.condition->kind == ExpressionKind::IntegerLiteral) {
                        cond_true = stmt.condition->int_literal_value != 0;
                    } else if (stmt.condition->kind == ExpressionKind::FloatingLiteral) {
                        cond_true = stmt.condition->float_literal_value != 0.0;
                    }
                }
                if (cond_true) {
                    // Keep then-branch, discard else-branch.
                    if (stmt.then_branch) {
                        if (!walk_statement(*stmt.then_branch)) return false;
                    }
                    stmt.else_branch = nullptr;
                } else {
                    // Keep else-branch, discard then-branch.
                    if (stmt.else_branch) {
                        if (!walk_statement(*stmt.else_branch)) return false;
                    }
                    stmt.then_branch = nullptr;
                }
            } else {
                if (stmt.then_branch) {
                    if (!walk_statement(*stmt.then_branch)) return false;
                }
                if (stmt.else_branch) {
                    if (!walk_statement(*stmt.else_branch)) return false;
                }
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
            if (stmt.is_range_for) {
                // Range-based for: for ( decl : range_expr ) body
                if (stmt.declaration) {
                    if (!walk_declaration(*stmt.declaration)) {
                        pop_scope();
                        return false;
                    }
                }
                if (stmt.for_range_expr) {
                    if (!type_expression(*stmt.for_range_expr)) {
                        pop_scope();
                        return false;
                    }
                    // Range expression should have array or class type (begin/end).
                    // For now, accept any type; backend handles validation.
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
        case StatementKind::Try: {
            // M10: walk the try block and all catch clauses.
            if (stmt.try_block) {
                if (!walk_statement(*stmt.try_block)) return false;
            }
            for (auto& cc : stmt.catch_clauses) {
                if (!walk_statement(*cc)) return false;
            }
            return true;
        }
        case StatementKind::Catch: {
            // M10: walk the catch exception declaration and body.
            if (stmt.catch_exception_decl) {
                if (!walk_declaration(*stmt.catch_exception_decl)) return false;
            }
            if (stmt.catch_body) {
                if (!walk_statement(*stmt.catch_body)) return false;
            }
            return true;
        }
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
        case ExpressionKind::This:
            return type_this(expr);
        case ExpressionKind::New:
            return type_new(expr);
        case ExpressionKind::Delete:
            return type_delete(expr);
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
        case ExpressionKind::Lambda:
            return type_lambda(expr);
        case ExpressionKind::Move:
            return type_move(expr);
        case ExpressionKind::FoldExpression:
            return type_fold_expression(expr);
        case ExpressionKind::Throw:
            return type_throw(expr);
        case ExpressionKind::Typeid:
            return type_typeid(expr);
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

bool Sema::type_this(Expression& expr) {
    // `this` is only valid inside a non-static member function.
    if (!current_class_type_) {
        return fail(DiagnosticID{4012, "sema-error-this"},
                    "'this' is only valid inside a non-static member function",
                    expr.location);
    }

    // `this` has type "pointer to the class type".
    expr.type = pointer_to(current_class_type_);
    expr.is_lvalue = false; // `this` is a prvalue
    expr.is_constant = true;
    return true;
}

bool Sema::type_new(Expression& expr) {
    // `new` requires a complete object type.
    if (!expr.new_type) {
        return fail(DiagnosticID{4013, "sema-error-new"},
                    "new: missing allocated type", expr.location);
    }
    if (expr.new_type->kind == TypeKind::Void ||
        expr.new_type->kind == TypeKind::Function ||
        expr.new_type->kind == TypeKind::Incomplete) {
        return fail(DiagnosticID{4013, "sema-error-new"},
                    "new: cannot allocate type '" +
                        std::string(expr.new_type->name) + "'", expr.location);
    }

    // For `new[]`, the array size (if present) must be integral.
    if (expr.new_alloc_expr) {
        if (!type_expression(*expr.new_alloc_expr)) return false;
        if (!is_integer_type(expr.new_alloc_expr->type)) {
            return fail(DiagnosticID{4013, "sema-error-new"},
                "new[]: array size must have integral type",
                expr.new_alloc_expr->location);
        }
        lvalue_to_rvalue(*expr.new_alloc_expr);
    }

    // Result type is pointer to the allocated type.
    expr.type = pointer_to(expr.new_type);
    expr.is_lvalue = false; // `new` is a prvalue
    expr.is_constant = false;
    return true;
}

bool Sema::type_delete(Expression& expr) {
    // `delete` requires a pointer operand.
    if (!expr.delete_operand) {
        return fail(DiagnosticID{4014, "sema-error-delete"},
                    "delete: missing operand", expr.location);
    }
    if (!type_expression(*expr.delete_operand)) return false;

    if (!is_pointer_type(expr.delete_operand->type)) {
        return fail(DiagnosticID{4014, "sema-error-delete"},
            "delete: operand must be a pointer type",
            expr.delete_operand->location);
    }

    lvalue_to_rvalue(*expr.delete_operand);

    // Result type is void.
    expr.type = builtin_void_;
    expr.is_lvalue = false;
    expr.is_constant = false;
    return true;
}

bool Sema::type_lambda(Expression& expr) {
    // A lambda expression is a prvalue of a unique closure type.
    // For now, we represent the closure type as a function type built from
    // the lambda's parameters and return type (or void if no return type).
    std::shared_ptr<Type> return_type = expr.lambda_return_type;
    if (!return_type) {
        return_type = builtin_void_;
    }
    std::vector<std::shared_ptr<Type>> param_types;
    for (auto& param : expr.lambda_params) {
        param_types.push_back(param->type);
    }
    // Build a unique closure type (function type with the lambda's signature).
    auto closure_type = std::make_shared<Type>();
    closure_type->kind = TypeKind::Function;
    closure_type->return_type = return_type;
    closure_type->param_types = param_types;
    expr.type = closure_type;
    expr.is_lvalue = false;
    expr.is_constant = false;
    return true;
}

bool Sema::type_move(Expression& expr) {
    // `move(expr)` casts its operand to an rvalue reference type.
    if (!expr.move_operand) {
        return fail(DiagnosticID{4015, "sema-error-move"},
                    "move: missing operand", expr.location);
    }
    if (!type_expression(*expr.move_operand)) {
        return false;
    }
    // Result type is rvalue reference to the operand's type.
    expr.type = rvalue_reference_to(expr.move_operand->type);
    expr.is_lvalue = false; // rvalue reference is a prvalue
    expr.is_constant = false;
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

bool Sema::type_fold_expression(Expression& expr) {
    // M10: type-check a C++17 fold expression.
    // The fold operand (the pack) must be typed first.
    if (!expr.fold_operand) {
        return fail(DiagnosticID{4016, "sema-error-fold"},
                    "fold expression: missing pack operand", expr.location);
    }
    if (!type_expression(*expr.fold_operand)) return false;

    // For binary folds, also type the init expression.
    if (expr.fold_kind == 2 || expr.fold_kind == 3) {
        if (!expr.fold_init) {
            return fail(DiagnosticID{4016, "sema-error-fold"},
                "fold expression: missing init for binary fold",
                expr.location);
        }
        if (!type_expression(*expr.fold_init)) return false;
    }

    // Result type: the type of the fold operand (simplified; a full
    // implementation would compute the result of applying fold_op to the
    // pack element type).
    expr.type = expr.fold_operand->type;
    expr.is_lvalue = false; // fold expressions are prvalues
    expr.is_constant = expr.fold_operand->is_constant;
    return true;
}

bool Sema::type_throw(Expression& expr) {
    // M10: type-check a throw expression: `throw expr;`
    if (!expr.throw_operand) {
        return fail(DiagnosticID{4017, "sema-error-throw"},
                    "throw: missing operand", expr.location);
    }
    if (!type_expression(*expr.throw_operand)) return false;
    lvalue_to_rvalue(*expr.throw_operand);

    // A throw expression has type void and is a prvalue.
    expr.type = builtin_void_;
    expr.is_lvalue = false;
    expr.is_constant = false;
    return true;
}

bool Sema::type_typeid(Expression& expr) {
    // M10: type-check a typeid expression: `typeid(T)` or `typeid(expr)`.
    if (expr.typeid_is_type) {
        if (!expr.typeid_type) {
            return fail(DiagnosticID{4018, "sema-error-typeid"},
                        "typeid: missing type", expr.location);
        }
        // typeid(T) — the type must be complete.
        if (expr.typeid_type->kind == TypeKind::Void ||
            expr.typeid_type->kind == TypeKind::Incomplete) {
            return fail(DiagnosticID{4018, "sema-error-typeid"},
                "typeid: cannot apply to incomplete or void type",
                expr.location);
        }
    } else {
        if (!expr.typeid_expr) {
            return fail(DiagnosticID{4018, "sema-error-typeid"},
                        "typeid: missing expression", expr.location);
        }
        if (!type_expression(*expr.typeid_expr)) return false;
        lvalue_to_rvalue(*expr.typeid_expr);
    }

    // typeid yields a const std::type_info&; model as a pointer to a
    // unique type_info type. For now, use void* as a placeholder.
    expr.type = pointer_to(builtin_void_);
    expr.is_lvalue = false;
    expr.is_constant = true;
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

// Score how well an argument type matches a parameter type for overload
// resolution. Returns 0 (exact match), 1 (promotion), 2 (conversion),
// or -1 (no match).
int Sema::arg_match_score(const std::shared_ptr<Type>& param_type,
                          const std::shared_ptr<Type>& arg_type) const {
    if (!param_type || !arg_type) return -1;
    if (types_compatible(param_type, arg_type)) return 0;
    // Integer promotion: narrow integer to int/long
    if (is_integer_type(arg_type)) {
        if (param_type == builtin_int_ || param_type == builtin_long_) return 1;
    }
    // Float promotion: float to double
    if (arg_type == builtin_float_ && param_type == builtin_double_) return 1;
    // Arithmetic conversion
    if (is_arithmetic_type(arg_type) && is_arithmetic_type(param_type)) return 2;
    return -1;
}

// Resolve overload: given candidate symbols and argument expressions,
// return the index of the best match, or -1 if no match.
int Sema::resolve_overload(
    const std::vector<std::shared_ptr<Symbol>>& candidates,
    const std::vector<std::shared_ptr<Expression>>& args) const {
    int best_idx = -1;
    int best_score = std::numeric_limits<int>::max();
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        auto& sym = candidates[i];
        if (!sym->type || !is_function_type(sym->type)) continue;
        auto& func_type = sym->type;
        if (func_type->param_types.size() != args.size()) continue;
        int total = 0;
        bool ok = true;
        for (std::size_t j = 0; j < args.size(); ++j) {
            auto arg_type = args[j]->type;
            int score = arg_match_score(func_type->param_types[j], arg_type);
            if (score < 0) { ok = false; break; }
            total += score;
        }
        if (ok && total < best_score) {
            best_score = total;
            best_idx = static_cast<int>(i);
        }
    }
    return best_idx;
}

std::string Sema::compute_mangled_name(const Symbol& sym) const {
    // Only mangle C++ external function symbols.
    if (!is_cpp_ || sym.linkage != Linkage::External) {
        return {};
    }
    if (!sym.type || !is_function_type(sym.type)) {
        return {};
    }

    // Build the scope from the current class type, if any.
    std::vector<std::string> scope;
    if (current_class_type_ && !current_class_type_->name.empty()) {
        scope.push_back(std::string(current_class_type_->name));
    }

    // Detect constructors/destructors by name matching the class.
    bool is_constructor = false;
    bool is_destructor = false;
    if (current_class_type_ && !current_class_type_->name.empty()) {
        std::string class_name(current_class_type_->name);
        if (sym.name == class_name) {
            is_constructor = true;
        } else if (sym.name.size() > class_name.size() + 1 &&
                   sym.name[0] == '~' &&
                   sym.name.substr(1) == class_name) {
            is_destructor = true;
        }
    }

    return Mangler::mangle_function(scope, sym.name,
                                    sym.type->param_types,
                                    sym.type->return_type,
                                    is_constructor, is_destructor);
}

bool Sema::type_function_call(Expression& expr) {
    // If callee is an identifier, check if it's a template function that
    // needs instantiation.
    bool is_template_call = false;
    if (expr.callee && expr.callee->kind == ExpressionKind::Identifier) {
        auto candidates = lookup_all(expr.callee->identifier_name);
        std::vector<std::shared_ptr<Symbol>> func_candidates;
        for (auto& c : candidates) {
            if (c->type && is_function_type(c->type)) {
                func_candidates.push_back(c);
            }
        }

        // Check if this is a template function (has a registered template
        // declaration with matching name).
        if (!func_candidates.empty()) {
            auto tmpl = find_template(expr.callee->identifier_name);
            if (tmpl && !tmpl->params.empty()) {
                is_template_call = true;
                // Type all args first so we can deduce template arguments.
                for (auto& arg : expr.call_args) {
                    if (!type_expression(*arg)) return false;
                    lvalue_to_rvalue(*arg);
                }

                // Deduce template arguments from call arguments.
                std::vector<std::shared_ptr<Type>> deduced_args;
                if (!deduce_template_args(
                        tmpl->params, expr.call_args,
                        func_candidates[0]->type->param_types,
                        deduced_args)) {
                    return fail(
                        DiagnosticID{4010, "sema-error-template"},
                        "could not deduce template arguments for call to '" +
                            expr.callee->identifier_name + "'",
                        expr.location);
                }

                // Substitute to create a specialized declaration.
                auto specialized = substitute_template_args(tmpl, deduced_args);
                if (!specialized || !specialized->type) {
                    return fail(
                        DiagnosticID{4011, "sema-error-template"},
                        "failed to instantiate template '" +
                            expr.callee->identifier_name + "'",
                        expr.location);
                }

                // Use the specialized type for the callee.
                // Build a function type from the specialized declarator's
                // return type and parameter types.
                auto func_type = std::make_shared<Type>();
                func_type->kind = TypeKind::Function;
                func_type->return_type = specialized->type;
                if (!specialized->declarators.empty()) {
                    func_type->param_types =
                        specialized->declarators[0]->param_types;
                }
                expr.callee->type = func_type;
            }
        }

        if (func_candidates.size() > 1 && !is_template_call) {
            // Type all args first so we can compare against parameter types.
            for (auto& arg : expr.call_args) {
                if (!type_expression(*arg)) return false;
                lvalue_to_rvalue(*arg);
            }
            int best = resolve_overload(func_candidates, expr.call_args);
            if (best < 0) {
                return fail(DiagnosticID{4008, "sema-error-call"},
                            "no matching function for call to '" +
                                expr.callee->identifier_name + "'",
                            expr.location);
            }
            expr.callee->type = func_candidates[best]->type;
        } else if (func_candidates.size() == 1 && !is_template_call) {
            // Single candidate: type the callee to get its function type.
            if (!type_expression(*expr.callee)) return false;
        }
    }

    if (expr.callee && !is_template_call && !expr.callee->type) {
        if (!type_expression(*expr.callee)) return false;
    }

    auto callee_type = expr.callee ? expr.callee->type : nullptr;
    if (callee_type && is_function_type(callee_type)) {
        // Validate argument count against parameter count.
        if (callee_type->param_types.size() != expr.call_args.size()) {
            return fail(DiagnosticID{4008, "sema-error-call"},
                "wrong number of arguments in call to '" +
                    (expr.callee ? expr.callee->identifier_name : "") + "'",
                expr.location);
        }
        // Function designator decays to pointer-to-function.
        lvalue_to_rvalue(*expr.callee);
    } else if (callee_type && is_pointer_type(callee_type) &&
               is_function_type(callee_type->pointee)) {
        if (callee_type->pointee->param_types.size() !=
            expr.call_args.size()) {
            return fail(DiagnosticID{4008, "sema-error-call"},
                "wrong number of arguments in call to '" +
                    (expr.callee ? expr.callee->identifier_name : "") + "'",
                expr.location);
        }
        lvalue_to_rvalue(*expr.callee);
    } else {
        return fail(DiagnosticID{4008, "sema-error-call"},
                    "callee is not a function", expr.location);
    }

    // Check argument types against parameter types.
    if (callee_type && is_function_type(callee_type)) {
        for (std::size_t i = 0; i < expr.call_args.size(); ++i) {
            if (i < callee_type->param_types.size()) {
                if (!types_compatible(callee_type->param_types[i],
                                      expr.call_args[i]->type)) {
                    return fail(
                        DiagnosticID{4008, "sema-error-call"},
                        "argument " + std::to_string(i + 1) +
                            " type mismatch in call to '" +
                            (expr.callee ? expr.callee->identifier_name : "") +
                            "'", expr.location);
                }
            }
        }
    }

    expr.type = callee_type && callee_type->return_type
                    ? callee_type->return_type
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
        if (base_type->pointee &&
            (base_type->pointee->kind == TypeKind::Struct ||
             base_type->pointee->kind == TypeKind::Union)) {
            auto member = lookup_member(base_type->pointee, expr.member_name);
            if (!member) {
                return fail(DiagnosticID{4009, "sema-error-member"},
                            "no member '" + expr.member_name + "' in class",
                            expr.location);
            }
            expr.type = member->type;
            expr.is_lvalue = !member->is_static;
            return true;
        }
        return fail(DiagnosticID{4009, "sema-error-member"},
                    "member access on non-struct pointer", expr.location);
    }

    if (base_type &&
        (base_type->kind == TypeKind::Struct ||
         base_type->kind == TypeKind::Union)) {
        // base.member
        auto member = lookup_member(base_type, expr.member_name);
        if (!member) {
            return fail(DiagnosticID{4009, "sema-error-member"},
                        "no member '" + expr.member_name + "' in class",
                        expr.location);
        }
        expr.type = member->type;
        expr.is_lvalue = expr.member_base->is_lvalue && !member->is_static;
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
// Template instantiation (M6)
// =====================================================================

void Sema::register_template(
    std::string_view name,
    std::shared_ptr<TemplateDeclaration> tmpl) {
    template_declarations_[std::string(name)] = std::move(tmpl);
}

std::shared_ptr<TemplateDeclaration> Sema::find_template(
    std::string_view name) const {
    auto it = template_declarations_.find(std::string(name));
    if (it == template_declarations_.end()) {
        return nullptr;
    }
    return it->second;
}

bool Sema::deduce_template_args(
    const std::vector<std::shared_ptr<TemplateParam>>& params,
    const std::vector<std::shared_ptr<Expression>>& call_args,
    const std::vector<std::shared_ptr<Type>>& param_types,
    std::vector<std::shared_ptr<Type>>& deduced_args) const {
    deduced_args.assign(params.size(), nullptr);

    // Map template parameter name → deduced type.
    std::unordered_map<std::string, std::shared_ptr<Type>> deduced;

    std::size_t call_arg_idx = 0;
    for (std::size_t i = 0; i < param_types.size() && call_arg_idx < call_args.size();
         ++i) {
        auto param_type = param_types[i];
        auto arg_type = call_args[call_arg_idx]->type;
        if (!arg_type) {
            // Argument not yet typed; can't deduce.
            return false;
        }

        if (param_type && param_type->kind == TypeKind::TemplateParam) {
            // Parameter type is a template parameter: deduce it from the
            // argument type.
            auto param_name = param_type->template_param_name;

            // M10: variadic template parameter pack — consume all remaining
            // call arguments for this parameter.
            if (i < params.size() && params[i]->is_variadic) {
                // Collect all remaining call args for the variadic parameter.
                // They must all be mutually compatible.
                auto first_type = arg_type;
                for (std::size_t j = call_arg_idx + 1; j < call_args.size(); ++j) {
                    if (!call_args[j]->type) return false;
                    if (!types_compatible(first_type, call_args[j]->type)) {
                        return false; // conflicting types in variadic pack
                    }
                }
                deduced[param_name] = first_type;
                call_arg_idx = call_args.size(); // consume all remaining args
            } else {
                auto it = deduced.find(param_name);
                if (it == deduced.end()) {
                    // First occurrence: deduce the type.
                    deduced[param_name] = arg_type;
                } else if (!types_compatible(it->second, arg_type)) {
                    // Conflicting deductions for the same parameter.
                    return false;
                }
                ++call_arg_idx;
            }
        } else {
            // Non-template-parameter type: check compatibility.
            if (!types_compatible(param_type, arg_type)) {
                return false;
            }
            ++call_arg_idx;
        }
    }

    // Fill in deduced args in parameter order. Undeduced parameters get
    // their default type (if any).
    for (std::size_t i = 0; i < params.size(); ++i) {
        auto it = deduced.find(params[i]->name);
        if (it != deduced.end()) {
            deduced_args[i] = it->second;
        } else if (params[i]->default_type) {
            deduced_args[i] = params[i]->default_type;
        } else {
            // No deduction and no default: can't instantiate.
            return false;
        }
    }

    return true;
}

std::shared_ptr<Declaration> Sema::substitute_template_args(
    const std::shared_ptr<TemplateDeclaration>& tmpl,
    const std::vector<std::shared_ptr<Type>>& template_args) const {
    auto decl = std::make_shared<Declaration>();
    *decl = *tmpl->declaration;  // Copy the base declaration

    // Clear template_params from the specialization (it's now concrete).
    decl->template_params.clear();

    // Build a substitution map: template parameter name → concrete type.
    std::unordered_map<std::string, std::shared_ptr<Type>> subst;
    for (std::size_t i = 0; i < tmpl->params.size() && i < template_args.size();
         ++i) {
        subst[tmpl->params[i]->name] = template_args[i];
    }

    // Substitute in the declaration's type.
    if (decl->type) {
        decl->type = substitute_type(decl->type, subst);
    }

    // Substitute in each declarator.
    for (auto& d : decl->declarators) {
        // Substitute in parameter types for function declarators.
        for (auto& pt : d->param_types) {
            pt = substitute_type(pt, subst);
        }
    }

    return decl;
}

std::shared_ptr<Type> Sema::substitute_type(
    const std::shared_ptr<Type>& ty,
    const std::unordered_map<std::string, std::shared_ptr<Type>>& subst) const {
    if (!ty) return nullptr;

    if (ty->kind == TypeKind::TemplateParam) {
        auto it = subst.find(ty->template_param_name);
        if (it != subst.end()) {
            return it->second;
        }
        // No substitution available: return as-is.
        return ty;
    }

    auto result = std::make_shared<Type>();
    *result = *ty;

    if (result->pointee) {
        result->pointee = substitute_type(result->pointee, subst);
    }
    if (result->element_type) {
        result->element_type = substitute_type(result->element_type, subst);
    }
    if (result->return_type) {
        result->return_type = substitute_type(result->return_type, subst);
    }
    for (auto& pt : result->param_types) {
        pt = substitute_type(pt, subst);
    }
    for (auto& m : result->members) {
        if (m->type) {
            m->type = substitute_type(m->type, subst);
        }
    }
    for (auto& bc : result->base_classes) {
        bc = substitute_type(bc, subst);
    }

    return result;
}

// =====================================================================
// walk_function_def / walk_declaration (entry from translation unit)
// =====================================================================

bool Sema::walk_function_def(FunctionDef& fdef) {
    return check_function_definition(fdef);
}

} // namespace krc
