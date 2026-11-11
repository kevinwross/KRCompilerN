#ifndef KRC_PARSER_H
#define KRC_PARSER_H

#include "ast.h"
#include "diagnostics.h"
#include "preprocessor.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace krc {

// A C11 recursive-descent parser producing a TranslationUnit AST.
//
// The parser wraps a (mutable) Preprocessor and a DiagnosticEngine.  It
// performs only syntactic analysis — no semantic analysis, no symbol table,
// no type checking.  Unsupported constructs are diagnosed as parse errors.
class Parser {
public:
    // Construct a parser reading from `preprocessor`.  `diagnostics` must
    // outlive the parser.  The preprocessor is non-const because parsing
    // advances it.
    Parser(Preprocessor& preprocessor, DiagnosticEngine& diagnostics);

    // Parse a complete translation unit.  Returns null on hard error.
    std::shared_ptr<TranslationUnit> parse_translation_unit();

private:
    // --- Token stream -----------------------------------------------------

    const Token& peek() const;
    const Token& current() const;
    bool at_eof() const;

    // Consume the current token and advance.  Returns the consumed token.
    Token advance();

    // If the current token has kind `expected`, consume it and return true;
    // otherwise report an error and return false.
    bool expect(TokenKind expected);

    // If the current token has kind `expected`, consume it and return true;
    // otherwise (without consuming) return false.  No error is reported.
    bool consume(TokenKind expected);

    // Report "expected `kind`" at the current position.
    void error_expected(TokenKind expected);

    // Report a custom message at the current position.
    void error(std::string_view message);

    // --- Declarations -----------------------------------------------------

    // external-declaration: function-definition | declaration
    std::shared_ptr<Declaration> parse_external_declaration();

    // template-parameter: typename T | class T | type N
    std::shared_ptr<TemplateParam> parse_template_parameter();

    // namespace-declaration: namespace identifier { ... }
    std::shared_ptr<Declaration> parse_namespace();

    // declaration-specifiers: (storage-class | type-specifier | type-qualifier |
    //                          function-specifier)*
    std::shared_ptr<Type> parse_declaration_specifiers(
        StorageClass& storage, Linkage& linkage,
        bool& is_typedef, bool& is_extern, bool& is_static,
        bool& is_const, bool& is_volatile, bool& is_restrict,
        bool& is_inline, bool& is_noreturn,
        bool& is_noexcept, bool& is_constexpr, bool& has_alignas);

    // Combine pairs like "long long", "unsigned int", "signed char".
    std::shared_ptr<Type> combine_type_specifiers(TokenKind first_kind);

    // type-specifier: void | _Bool | char | short | int | long | float |
    //                 double | struct-or-union-specifier | enum-specifier |
    //                 typedef-name
    std::shared_ptr<Type> parse_type_specifier();

    // struct-or-union-specifier: struct-or-union identifier? '{' ... '}'
    std::shared_ptr<Type> parse_struct_or_union();

    // class-specifier: class identifier? '{' member-specification '}'
    std::shared_ptr<Type> parse_class_spec();

    // enum-specifier: enum identifier? '{' ... '}'
    std::shared_ptr<Type> parse_enum_specifier();

    // init-declarator-list: declarator ('=' initializer)? (',' ...)*
    void parse_init_declarator_list(std::shared_ptr<Declaration> decl,
                                    std::shared_ptr<Type> base_type);

    // declarator: pointer? direct-declarator
    std::shared_ptr<Declarator> parse_declarator();

    // --- Block-scope declaration ------------------------------------------

    std::shared_ptr<Declaration> parse_declaration_block_scope();

    // --- Statements -------------------------------------------------------

    std::shared_ptr<Statement> parse_statement();
    std::shared_ptr<Statement> parse_expression_statement();
    std::shared_ptr<Statement> parse_compound_statement();
    std::shared_ptr<Statement> parse_selection_statement();
    std::shared_ptr<Statement> parse_iteration_statement();
    std::shared_ptr<Statement> parse_jump_statement();
    std::shared_ptr<Statement> parse_eh_statement();
    std::shared_ptr<Statement> parse_labeled_statement();

    // --- Expressions ------------------------------------------------------

    std::shared_ptr<Expression> parse_expression();
    std::shared_ptr<Expression> parse_assignment_expression();
    std::shared_ptr<Expression> parse_conditional_expression();
    std::shared_ptr<Expression> parse_logical_or_expression();
    std::shared_ptr<Expression> parse_logical_and_expression();
    std::shared_ptr<Expression> parse_inclusive_or_expression();
    std::shared_ptr<Expression> parse_exclusive_or_expression();
    std::shared_ptr<Expression> parse_and_expression();
    std::shared_ptr<Expression> parse_equality_expression();
    std::shared_ptr<Expression> parse_relational_expression();
    std::shared_ptr<Expression> parse_shift_expression();
    std::shared_ptr<Expression> parse_additive_expression();
    std::shared_ptr<Expression> parse_multiplicative_expression();
    std::shared_ptr<Expression> parse_cast_expression();
    std::shared_ptr<Expression> parse_unary_expression();
    std::shared_ptr<Expression> parse_postfix_expression();
    std::shared_ptr<Expression> parse_primary_expression();
    std::shared_ptr<Expression> parse_lambda_expression();

    // Postfix suffixes: [] () . -> ++ --
    void parse_postfix_rest(std::shared_ptr<Expression> expr);

    // --- Types for cast / sizeof ------------------------------------------

    std::shared_ptr<Type> parse_type_name();
    std::shared_ptr<Declarator> parse_abstract_declarator();

    // --- Initializers -----------------------------------------------------

    std::shared_ptr<Initializer> parse_initializer();
    void parse_initializer_list(std::shared_ptr<Initializer> init);
    void parse_designation(std::shared_ptr<Initializer> init);

    // Parse an expression used as an initializer (for structured bindings).
    std::shared_ptr<Expression> parse_initializer_expression();

    // --- Helpers ----------------------------------------------------------

    bool is_type_specifier_kind(TokenKind kind) const;
    bool is_declaration_specifier_kind(TokenKind kind) const;
    bool is_storage_class_kind(TokenKind kind) const;
    bool is_type_qualifier_kind(TokenKind kind) const;
    bool is_function_specifier_kind(TokenKind kind) const;
    bool is_cast_start(TokenKind kind) const;

    // M10: fold expression helpers
    bool is_fold_op(TokenKind kind) const;
    FoldOp parse_fold_op(TokenKind kind) const;

    Preprocessor& preprocessor_;
    DiagnosticEngine& diagnostics_;
    std::shared_ptr<Statement> pending_structured_binding_;
};

} // namespace krc

#endif // KRC_PARSER_H
