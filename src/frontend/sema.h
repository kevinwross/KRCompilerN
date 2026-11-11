#ifndef KRC_SEMA_H
#define KRC_SEMA_H

#include "ast.h"
#include "diagnostics.h"

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace krc {

// A symbol: a name bound to a type, with linkage and storage properties.
struct Symbol {
    std::string name;
    std::string mangled_name;   // C++ mangled name (empty for C linkage)
    std::shared_ptr<Type> type;
    Linkage linkage = Linkage::None;
    StorageClass storage = StorageClass::None;
    bool is_definition = false;   // true if this is a defining declaration
    bool is_typedef = false;      // true if this is a typedef name
    bool is_const = false;
    bool is_volatile = false;
    bool is_restrict = false;
    bool is_static = false;
    bool is_thread_local = false;  // M10: thread_local storage duration
    bool is_extern = false;
    bool is_virtual = false;      // C++: virtual member function
    bool is_inline = false;       // C++: inline function/variable specifier
    bool is_noexcept = false;     // C++: noexcept function specifier
    bool is_constexpr = false;    // C++11: constexpr function/variable specifier
    bool has_alignas = false;     // C++: alignas specifier present
    SourceLocation location;      // where the symbol was declared
};

// A scope: a symbol table with a parent scope for lexical scoping.
// Multiple symbols may share the same name (C++ overloading).
struct Scope {
    Scope* parent = nullptr;
    std::unordered_map<std::string, std::vector<std::shared_ptr<Symbol>>> symbols;

    // Look up a name in this scope and all ancestor scopes. Returns the
    // most recently inserted symbol (backward-compatible for non-overloaded
    // names). Returns null if not found.
    std::shared_ptr<Symbol> lookup(std::string_view name) const;

    // Look up all symbols with this name in this scope and ancestor scopes.
    // Returns an empty vector if not found.
    std::vector<std::shared_ptr<Symbol>> lookup_all(std::string_view name) const;

    // Insert a symbol into this scope only (no parent search). Returns true
    // unless an identical symbol (same name and type) already exists in this
    // scope, in which case returns false (redefinition).
    bool insert(std::string_view name, std::shared_ptr<Symbol> sym);
};

// The semantic analyzer. Walks the AST, resolves symbols, checks types,
// performs conversions, and reports diagnostics.
class Sema {
public:
    Sema(DiagnosticEngine& diagnostics);

    // Run semantic analysis on a translation unit. Returns true on success.
    bool analyze(TranslationUnit& tu);

    // Access to builtin types.
    std::shared_ptr<Type> void_type() const { return builtin_void_; }
    std::shared_ptr<Type> bool_type() const { return builtin_bool_; }
    std::shared_ptr<Type> char_type() const { return builtin_char_; }
    std::shared_ptr<Type> short_type() const { return builtin_short_; }
    std::shared_ptr<Type> int_type() const { return builtin_int_; }
    std::shared_ptr<Type> long_type() const { return builtin_long_; }
    std::shared_ptr<Type> long_long_type() const { return builtin_long_long_; }
    std::shared_ptr<Type> float_type() const { return builtin_float_; }
    std::shared_ptr<Type> double_type() const { return builtin_double_; }
    std::shared_ptr<Type> builtin_ulong_type() const;
    std::shared_ptr<Type> auto_type() const { return builtin_auto_; }
    std::shared_ptr<Type> decltype_type() const { return builtin_decltype_; }

    // Type compatibility and conversion helpers.
    bool types_compatible(const std::shared_ptr<Type>& a,
                          const std::shared_ptr<Type>& b) const;
    bool is_integer_type(const std::shared_ptr<Type>& t) const;
    bool is_floating_type(const std::shared_ptr<Type>& t) const;
    bool is_scalar_type(const std::shared_ptr<Type>& t) const;
    bool is_arithmetic_type(const std::shared_ptr<Type>& t) const;
    bool is_pointer_type(const std::shared_ptr<Type>& t) const;
    bool is_reference_type(const std::shared_ptr<Type>& t) const;
    bool is_void_type(const std::shared_ptr<Type>& t) const;
    bool is_array_type(const std::shared_ptr<Type>& t) const;
    bool is_function_type(const std::shared_ptr<Type>& t) const;

    // Usual arithmetic conversions: given two arithmetic types, return the
    // result type they should be converted to.
    std::shared_ptr<Type> usual_arithmetic_conversions(
        const std::shared_ptr<Type>& a,
        const std::shared_ptr<Type>& b) const;

    // Integer promotion: promote a narrow integer type to int (or unsigned
    // int if int cannot represent all values of the original type).
    std::shared_ptr<Type> integer_promotion(
        const std::shared_ptr<Type>& t) const;

    // Array-to-pointer decay: convert "array of T" to "pointer to T".
    std::shared_ptr<Type> array_to_pointer_decay(
        const std::shared_ptr<Type>& t) const;

    // Function-to-pointer decay: convert "function returning T" to
    // "pointer to function returning T".
    std::shared_ptr<Type> function_to_pointer_decay(
        const std::shared_ptr<Type>& t) const;

    // Lvalue-to-rvalue conversion: if the expression is an lvalue of
    // object type, its type becomes the value type (dropping lvalue-ness).
    void lvalue_to_rvalue(Expression& expr) const;
    bool check_assignment(const std::shared_ptr<Type>& left,
                          const std::shared_ptr<Type>& right,
                          SourceLocation loc) const;

    // Type builders (public for testing).
    std::shared_ptr<Type> pointer_to(
        const std::shared_ptr<Type>& pointee) const;
    std::shared_ptr<Type> reference_to(
        const std::shared_ptr<Type>& pointee) const;
    std::shared_ptr<Type> rvalue_reference_to(
        const std::shared_ptr<Type>& pointee) const;
    std::shared_ptr<Type> array_of(
        const std::shared_ptr<Type>& element, std::uint64_t length) const;
    std::shared_ptr<Type> function_type(
        const std::shared_ptr<Type>& return_type,
        const std::vector<std::shared_ptr<Type>>& param_types,
        bool is_variadic) const;
    Scope& current_scope() { return *scope_stack_.back(); }
    const Scope& current_scope() const { return *scope_stack_.back(); }

    void push_scope();
    void pop_scope();

    // Look up a name in the current scope chain.
    std::shared_ptr<Symbol> lookup(std::string_view name) const;

    // Look up all symbols with this name in the current scope chain (for
    // overloaded function resolution).
    std::vector<std::shared_ptr<Symbol>> lookup_all(
        std::string_view name) const;

    // Look up a member by name in a class/struct type.
    std::shared_ptr<Symbol> lookup_member(
        const std::shared_ptr<Type>& class_type,
        std::string_view name) const;

    // Insert a symbol into the current scope.
    bool insert_symbol(std::string_view name, std::shared_ptr<Symbol> sym);

    // Enter a function scope (for function prototypes/parameter lists).
    void enter_function_scope();
    void leave_function_scope();

    // Set C++ language mode (enables name mangling for external symbols).
    void set_cpp_mode(bool is_cpp) { is_cpp_ = is_cpp; }
    bool is_cpp_mode() const { return is_cpp_; }

    // Set the current class type (for `this` and member lookup; public for
    // testing).
    void set_current_class_type(const std::shared_ptr<Type>& cls) {
        current_class_type_ = cls;
    }

private:
    DiagnosticEngine& diagnostics_;

    // Builtin types (singleton-like shared instances).
    std::shared_ptr<Type> builtin_void_;
    std::shared_ptr<Type> builtin_bool_;
    std::shared_ptr<Type> builtin_char_;
    std::shared_ptr<Type> builtin_short_;
    std::shared_ptr<Type> builtin_int_;
    std::shared_ptr<Type> builtin_long_;
    std::shared_ptr<Type> builtin_long_long_;
    std::shared_ptr<Type> builtin_float_;
    std::shared_ptr<Type> builtin_double_;
    std::shared_ptr<Type> builtin_auto_;    // M6: `auto` placeholder type
    std::shared_ptr<Type> builtin_decltype_; // M6: `decltype` placeholder type

    // Scope stack: back() is the innermost scope.
    std::vector<std::unique_ptr<Scope>> scope_stack_;

    // File scope (global scope) is scope_stack_[0].
    Scope* file_scope_ = nullptr;

    // Typedef names currently in scope (for disambiguating type names
    // in declarations).
    std::unordered_set<std::string> typedef_names_;

    // Declarations seen so far, for duplicate detection.
    std::unordered_map<std::string, std::vector<std::shared_ptr<Symbol>>>
        declared_names_;

    // The class/struct type we are currently inside (for `this` and member
    // lookup during Sema of member functions). Null when not in a class.
    std::shared_ptr<Type> current_class_type_;

    // C++ language mode: when true, external function symbols get mangled
    // names via the Itanium-like Mangler.
    bool is_cpp_ = false;

    // Registered template declarations (name → TemplateDeclaration).
    // Populated during Sema walking of template declarations.
    std::unordered_map<std::string, std::shared_ptr<TemplateDeclaration>>
        template_declarations_;

    // ----- AST walkers -----
public:
    bool walk_translation_unit(TranslationUnit& tu);
    bool walk_declaration(Declaration& decl);
    bool walk_structured_binding(Statement& stmt);
    bool walk_function_def(FunctionDef& fdef);
    bool walk_statement(Statement& stmt);
    bool walk_expression(Expression& expr);
    bool walk_initializer(Initializer& init);
    bool walk_type(Type& ty);

    // ----- Declaration / definition handling -----
public:
    bool check_declaration(Declaration& decl);
    bool resolve_declarator_type(Declaration& decl);
    bool compute_vtable_layout(std::shared_ptr<Type> struct_type);
    bool check_typedef(Declaration& decl);
    bool check_function_definition(FunctionDef& fdef);
    bool type_expression(Expression& expr);
    bool type_function_call(Expression& expr);
    bool type_member_access(Expression& expr);
    bool type_this(Expression& expr);
    bool type_new(Expression& expr);
    bool type_delete(Expression& expr);
    bool type_lambda(Expression& expr);
    bool type_move(Expression& expr);
    bool type_forward(Expression& expr);
    bool type_fold_expression(Expression& expr);
    bool type_throw(Expression& expr);
    bool type_typeid(Expression& expr);

private:
    // ----- Expression typing (internal) -----

    bool type_identifier(Expression& expr);
    bool type_integer_literal(Expression& expr);
    bool type_floating_literal(Expression& expr);
    bool type_char_literal(Expression& expr);
    bool type_string_literal(Expression& expr);
    bool type_unary(Expression& expr);
    bool type_binary(Expression& expr);
    bool type_conditional(Expression& expr);
    bool type_assignment(Expression& expr);
    bool type_cast(Expression& expr);
    bool type_sizeof(Expression& expr);
    bool type_array_subscript(Expression& expr);
    bool type_compound_literal(Expression& expr);

    // Overload resolution helpers.
public:
    // Overload resolution helpers (public for testing).
    int arg_match_score(const std::shared_ptr<Type>& param_type,
                        const std::shared_ptr<Type>& arg_type) const;
    int resolve_overload(
        const std::vector<std::shared_ptr<Symbol>>& candidates,
        const std::vector<std::shared_ptr<Expression>>& args) const;

    // Compute the C++ mangled name for a symbol (empty if not applicable).
    std::string compute_mangled_name(const Symbol& sym) const;

    // ----- Template instantiation (M6) -----

    // Register a template declaration so it can be found during
    // instantiation. Called from walk_declaration() for template
    // declarations.
    void register_template(
        std::string_view name,
        std::shared_ptr<TemplateDeclaration> tmpl);

    // Look up a registered template by name.
    std::shared_ptr<TemplateDeclaration> find_template(
        std::string_view name) const;

    // Deduce template arguments from call arguments and parameter types.
    // Returns true on success, with `args` filled in per template parameter.
    bool deduce_template_args(
        const std::vector<std::shared_ptr<TemplateParam>>& params,
        const std::vector<std::shared_ptr<Expression>>& call_args,
        const std::vector<std::shared_ptr<Type>>& param_types,
        std::vector<std::shared_ptr<Type>>& deduced_args) const;

    // Substitute deduced template arguments into a declaration, producing
    // a specialized declaration. Returns the specialized declaration.
    std::shared_ptr<Declaration> substitute_template_args(
        const std::shared_ptr<TemplateDeclaration>& tmpl,
        const std::vector<std::shared_ptr<Type>>& template_args) const;

private:
    // Substitute template arguments into a type, recursively.
    std::shared_ptr<Type> substitute_type(
        const std::shared_ptr<Type>& ty,
        const std::unordered_map<std::string, std::shared_ptr<Type>>& subst) const;

private:

    void error(DiagnosticID id, std::string_view message,
               SourceLocation loc) const;
    void warning(DiagnosticID id, std::string_view message,
                 SourceLocation loc) const;
    void note(std::string_view message, SourceLocation loc) const;

    // Report an error and return false.
    bool fail(DiagnosticID id, std::string_view message,
              SourceLocation loc) const;

    // Check that two types are compatible for a binary operation, and
    // annotate both operand types with the result type.
    bool check_binary_types(Expression& expr) const;
};


// Dump the symbol table of a scope (debugging).
void dump_scope(const Scope& scope, std::ostream& out, int indent = 0);

} // namespace krc

#endif // KRC_SEMA_H
