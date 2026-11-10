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
    std::shared_ptr<Type> type;
    Linkage linkage = Linkage::None;
    StorageClass storage = StorageClass::None;
    bool is_definition = false;   // true if this is a defining declaration
    bool is_typedef = false;      // true if this is a typedef name
    bool is_const = false;
    bool is_volatile = false;
    bool is_restrict = false;
    bool is_static = false;
    bool is_extern = false;
    SourceLocation location;      // where the symbol was declared
};

// A scope: a symbol table with a parent scope for lexical scoping.
struct Scope {
    Scope* parent = nullptr;
    std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols;

    // Look up a name in this scope and all ancestor scopes.
    std::shared_ptr<Symbol> lookup(std::string_view name) const;

    // Insert a symbol into this scope only (no parent search).
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

    // Type compatibility and conversion helpers.
    bool types_compatible(const std::shared_ptr<Type>& a,
                          const std::shared_ptr<Type>& b) const;
    bool is_integer_type(const std::shared_ptr<Type>& t) const;
    bool is_floating_type(const std::shared_ptr<Type>& t) const;
    bool is_scalar_type(const std::shared_ptr<Type>& t) const;
    bool is_arithmetic_type(const std::shared_ptr<Type>& t) const;
    bool is_pointer_type(const std::shared_ptr<Type>& t) const;
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

    // Check that an assignment's right operand is assignable to the left.
    bool check_assignment(const std::shared_ptr<Type>& left,
                          const std::shared_ptr<Type>& right,
                          SourceLocation loc) const;

    // Type builders (public for testing).
    std::shared_ptr<Type> pointer_to(
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

    // Insert a symbol into the current scope.
    bool insert_symbol(std::string_view name, std::shared_ptr<Symbol> sym);

    // Enter a function scope (for function prototypes/parameter lists).
    void enter_function_scope();
    void leave_function_scope();

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

    // ----- AST walkers -----

    bool walk_translation_unit(TranslationUnit& tu);
    bool walk_declaration(Declaration& decl);
    bool walk_function_def(FunctionDef& fdef);
    bool walk_statement(Statement& stmt);
    bool walk_expression(Expression& expr);
    bool walk_initializer(Initializer& init);
    bool walk_type(Type& ty);

    // ----- Declaration / definition handling -----

    bool check_declaration(Declaration& decl);
    bool resolve_declarator_type(Declaration& decl);
    bool check_typedef(Declaration& decl);
    bool check_function_definition(FunctionDef& fdef);

    // ----- Expression typing -----

    bool type_expression(Expression& expr);
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
    bool type_function_call(Expression& expr);
    bool type_member_access(Expression& expr);
    bool type_array_subscript(Expression& expr);
    bool type_compound_literal(Expression& expr);

    // ----- Helpers -----

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
