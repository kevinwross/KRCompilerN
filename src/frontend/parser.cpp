#include "parser.h"

#include <string_view>

namespace krc {

namespace {

// Diagnostic IDs for parser errors.
const DiagnosticID kParseErrorExpected{1000, "parse-error-expected"};

} // namespace

Parser::Parser(Preprocessor& preprocessor, DiagnosticEngine& diagnostics)
    : preprocessor_(preprocessor), diagnostics_(diagnostics) {}

std::shared_ptr<TranslationUnit> Parser::parse_translation_unit() {
    auto tu = std::make_shared<TranslationUnit>();
    // Prime the preprocessor: read the first token.
    (void)preprocessor_.next();
    while (!at_eof()) {
        // Handle C++ template declarations: `template <...> <declaration>`.
        if (consume(TokenKind::KwTemplate)) {
            // Parse the template parameter list: <typename T, class U, int N>
            if (!consume(TokenKind::Less)) {
                error("expected '<' after 'template'");
                break;
            }
            std::vector<std::shared_ptr<TemplateParam>> params;
            while (!at_eof() && current().kind != TokenKind::Greater) {
                auto param = parse_template_parameter();
                if (!param) {
                    error("invalid template parameter");
                    break;
                }
                params.push_back(param);
                if (consume(TokenKind::Comma))
                    continue;
                break;
            }
            expect(TokenKind::Greater);

            // Parse the declaration that follows the parameter list.
            // Handle C++ namespace declarations: `namespace Name { ... }`.
        if (consume(TokenKind::KwNamespace)) {
            auto decl = parse_namespace();
            if (!decl) {
                error("invalid namespace declaration");
                break;
            }
            tu->declarations.push_back(decl);
            continue;
        }
        auto decl = parse_external_declaration();
            if (!decl) {
                error("missing declaration after template parameter list");
                break;
            }
            decl->template_params = std::move(params);
            tu->declarations.push_back(decl);
            continue;
        }
        // Handle C++ namespace declarations: `namespace Name { ... }`.
        if (consume(TokenKind::KwNamespace)) {
            auto decl = parse_namespace();
            if (!decl) {
                error("invalid namespace declaration");
                break;
            }
            tu->declarations.push_back(decl);
            continue;
        }
        auto decl = parse_external_declaration();
        if (!decl) {
            // Hard error — stop to avoid cascading failures.
            break;
        }
        tu->declarations.push_back(decl);
    }
    return tu;
}

// --- Token stream -----------------------------------------------------

const Token& Parser::peek() const { return preprocessor_.peek(); }
const Token& Parser::current() const { return preprocessor_.current(); }
bool Parser::at_eof() const { return preprocessor_.at_eof(); }

Token Parser::advance() {
    LexResult result = preprocessor_.next();
    if (!result.ok) {
        // Preprocessor already emitted a diagnostic; synthesize a recovery token.
        return Token{TokenKind::Invalid, current().location, "", 0};
    }
    return result.token;
}

bool Parser::expect(TokenKind expected) {
    if (current().kind == expected) {
        advance();
        return true;
    }
    error_expected(expected);
    return false;
}

bool Parser::consume(TokenKind expected) {
    if (current().kind == expected) {
        advance();
        return true;
    }
    return false;
}

void Parser::error_expected(TokenKind expected) {
    diagnostics_.error(kParseErrorExpected,
        "unexpected token; expected token kind " +
            std::to_string(static_cast<int>(expected)),
        current().location);
}

void Parser::error(std::string_view message) {
    diagnostics_.error(kParseErrorExpected, std::string(message),
                       current().location);
}

// --- Declarations -----------------------------------------------------

std::shared_ptr<Declaration> Parser::parse_external_declaration() {
    // Parse declaration specifiers, then a declarator.  If the declarator
    // is a function and is followed by '{', it's a function definition;
    // otherwise it's a declaration ending with ';'.

    StorageClass storage = StorageClass::None;
    Linkage linkage = Linkage::None;
    bool is_typedef = false, is_extern = false, is_static = false;
    bool is_const = false, is_volatile = false, is_restrict = false;
    bool is_inline = false, is_noreturn = false;
    bool is_noexcept = false, is_constexpr = false, has_alignas = false;

    auto base_type = parse_declaration_specifiers(storage, linkage, is_typedef,
                                                  is_extern, is_static, is_const,
                                                  is_volatile, is_restrict,
                                                  is_inline, is_noreturn,
                                                  is_noexcept, is_constexpr, has_alignas);

    if (!base_type) {
        error("missing type specifier");
        return nullptr;
    }

    auto declarator = parse_declarator();
    if (!declarator) {
        // Allow a type definition without a declarator (e.g. `class Foo { ... };`,
        // `struct Bar { ... };`, `enum Baz { ... };`).  The type itself is the
        // declaration; no object is declared.
        if (base_type && (base_type->kind == TypeKind::Struct ||
                          base_type->kind == TypeKind::Union ||
                          base_type->kind == TypeKind::Enum)) {
            auto decl = std::make_shared<Declaration>();
            decl->location = current().location;
            decl->storage = storage;
            decl->linkage = linkage;
            decl->is_typedef = is_typedef;
            decl->is_extern = is_extern;
            decl->is_static = is_static;
            decl->is_const = is_const;
            decl->is_volatile = is_volatile;
            decl->is_restrict = is_restrict;
            decl->is_inline = is_inline;
            decl->is_noreturn = is_noreturn;
            decl->is_noexcept = is_noexcept;
            decl->is_constexpr = is_constexpr;
            decl->has_alignas = has_alignas;
            decl->type = base_type;
            expect(TokenKind::Semicolon);
            return decl;
        }
        error("expected declarator");
        return nullptr;
    }

    // Function definition: declarator (function) followed by '{'.
    // Don't consume '{' here — parse_compound_statement() will do it.
    if (declarator->is_function && current().kind == TokenKind::LBrace) {
        auto func_def = std::make_shared<FunctionDef>();
        auto decl = std::make_shared<Declaration>();
        decl->location = current().location;
        decl->storage = storage;
        decl->linkage = linkage;
        decl->is_typedef = is_typedef;
        decl->is_extern = is_extern;
        decl->is_static = is_static;
        decl->is_const = is_const;
        decl->is_volatile = is_volatile;
        decl->is_restrict = is_restrict;
        decl->is_inline = is_inline;
        decl->is_noreturn = is_noreturn;
        decl->is_noexcept = is_noexcept;
        decl->is_constexpr = is_constexpr;
        decl->has_alignas = has_alignas;
        decl->type = base_type;
        decl->declarators.push_back(declarator);
        decl->function_def = func_def;
        func_def->declaration = decl;

        func_def->body = parse_compound_statement();
        return decl;
    }

    // Otherwise: declaration with init-declarator list.
    auto decl = std::make_shared<Declaration>();
    decl->location = current().location;
    decl->storage = storage;
    decl->linkage = linkage;
    decl->is_typedef = is_typedef;
    decl->is_extern = is_extern;
    decl->is_static = is_static;
    decl->is_const = is_const;
    decl->is_volatile = is_volatile;
    decl->is_restrict = is_restrict;
    decl->is_inline = is_inline;
    decl->is_noreturn = is_noreturn;
    decl->is_noexcept = is_noexcept;
    decl->is_constexpr = is_constexpr;
    decl->has_alignas = has_alignas;
    decl->type = base_type;
    decl->declarators.push_back(declarator);

    if (consume(TokenKind::Eq)) {
        auto init = parse_initializer();
        if (init) {
            decl->initializers.push_back(init);
        }
    }

    while (consume(TokenKind::Comma)) {
        auto next_decl = parse_declarator();
        if (!next_decl) {
            error("expected declarator");
            break;
        }
        decl->declarators.push_back(next_decl);
        if (consume(TokenKind::Eq)) {
            auto init = parse_initializer();
            if (init) {
                decl->initializers.push_back(init);
            }
        }
    }

    expect(TokenKind::Semicolon);
    return decl;
}

std::shared_ptr<Declaration> Parser::parse_namespace() {
    // Parse the namespace name: `namespace Name`.
    if (current().kind != TokenKind::Identifier) {
        error("expected namespace name");
        return nullptr;
    }
    std::string name = std::string(current().text);
    advance();

    auto decl = std::make_shared<Declaration>();
    decl->is_namespace = true;
    decl->namespace_name = std::move(name);
    decl->location = current().location;

    // Expect '{'.
    if (!consume(TokenKind::LBrace)) {
        error("expected '{' after namespace name");
        return nullptr;
    }

    // Parse declarations inside the namespace until '}'.
    while (!at_eof() && current().kind != TokenKind::RBrace) {
        // Nested namespace: `namespace N { namespace M { ... } }`.
        if (consume(TokenKind::KwNamespace)) {
            auto ns_decl = parse_namespace();
            if (!ns_decl) {
                error("invalid namespace declaration");
                break;
            }
            decl->namespace_members.push_back(ns_decl);
            continue;
        }
        // Template declaration inside namespace.
        if (consume(TokenKind::KwTemplate)) {
            if (!consume(TokenKind::Less)) {
                error("expected '<' after 'template'");
                break;
            }
            std::vector<std::shared_ptr<TemplateParam>> params;
            while (!at_eof() && current().kind != TokenKind::Greater) {
                auto param = parse_template_parameter();
                if (!param) {
                    error("invalid template parameter");
                    break;
                }
                params.push_back(param);
                if (consume(TokenKind::Comma))
                    continue;
                break;
            }
            expect(TokenKind::Greater);
            auto member = parse_external_declaration();
            if (!member) {
                error("missing declaration after template parameter list");
                break;
            }
            member->template_params = std::move(params);
            decl->namespace_members.push_back(member);
            continue;
        }
        auto member = parse_external_declaration();
        if (!member) {
            error("invalid declaration in namespace");
            break;
        }
        decl->namespace_members.push_back(member);
    }

    expect(TokenKind::RBrace);
    return decl;
}

// Parse a single template parameter: `typename T`, `class T`, `int N`, etc.
std::shared_ptr<TemplateParam> Parser::parse_template_parameter() {
    auto param = std::make_shared<TemplateParam>();

    // `typename T` or `class T` — type parameter.
    if (consume(TokenKind::KwTypename) || consume(TokenKind::KwClass)) {
        param->kind = TemplateParam::Kind::Type;

        // M10: variadic type parameter pack: `typename... T` or `class... T`.
        if (consume(TokenKind::Ellipsis)) {
            param->is_variadic = true;
        }

        if (current().kind != TokenKind::Identifier) {
            error("expected template parameter name");
            return nullptr;
        }
        param->name = std::string(current().text);
        advance();

        // Optional default type: `typename T = int`.
        if (consume(TokenKind::Eq)) {
            // The default type is parsed as a type specifier.
            StorageClass storage = StorageClass::None;
            Linkage linkage = Linkage::None;
            bool is_typedef = false, is_extern = false, is_static = false;
            bool is_const = false, is_volatile = false, is_restrict = false;
            bool is_inline = false, is_noreturn = false;
            bool is_noexcept = false, is_constexpr = false, has_alignas = false;
            param->default_type = parse_declaration_specifiers(
                storage, linkage, is_typedef, is_extern, is_static,
                is_const, is_volatile, is_restrict, is_inline, is_noreturn,
                is_noexcept, is_constexpr, has_alignas);
        }
        return param;
    }

    // Value parameter: `type N` (e.g. `int N`).
    StorageClass storage = StorageClass::None;
    Linkage linkage = Linkage::None;
    bool is_typedef = false, is_extern = false, is_static = false;
    bool is_const = false, is_volatile = false, is_restrict = false;
    bool is_inline = false, is_noreturn = false;
    bool is_noexcept = false, is_constexpr = false, has_alignas = false;
    auto base_type = parse_declaration_specifiers(
        storage, linkage, is_typedef, is_extern, is_static,
        is_const, is_volatile, is_restrict, is_inline, is_noreturn,
        is_noexcept, is_constexpr, has_alignas);
    if (!base_type) {
        error("expected type or typename in template parameter");
        return nullptr;
    }
    param->kind = TemplateParam::Kind::Value;
    param->param_type = base_type;

    if (current().kind != TokenKind::Identifier) {
        error("expected template parameter name");
        return nullptr;
    }
    param->name = std::string(current().text);
    advance();

    // Optional default value: `int N = 0`.
    if (consume(TokenKind::Eq)) {
        param->default_value = parse_expression();
    }

    return param;
}

std::shared_ptr<Type> Parser::parse_declaration_specifiers(
    StorageClass& storage, Linkage& linkage,
    bool& is_typedef, bool& is_extern, bool& is_static,
    bool& is_const, bool& is_volatile, bool& is_restrict,
    bool& is_inline, bool& is_noreturn, bool& is_noexcept, bool& is_constexpr, bool& has_alignas) {

    std::shared_ptr<Type> type;
    bool saw_type_specifier = false;
    is_constexpr = false;

    while (true) {
        TokenKind kind = current().kind;
        if (is_storage_class_kind(kind)) {
            switch (kind) {
                case TokenKind::KwTypedef:
                    is_typedef = true;
                    storage = StorageClass::Typedef;
                    break;
                case TokenKind::KwExtern:
                    is_extern = true;
                    storage = StorageClass::Extern;
                    linkage = Linkage::External;
                    break;
                case TokenKind::KwStatic:
                    is_static = true;
                    storage = StorageClass::Static;
                    linkage = Linkage::Internal;
                    break;
                case TokenKind::KwAuto:
                    storage = StorageClass::Auto;
                    break;
                case TokenKind::KwRegister:
                    storage = StorageClass::Register;
                    break;
                case TokenKind::KwThreadLocal:
                    storage = StorageClass::ThreadLocal;
                    break;
                default:
                    break;
            }
            advance();
        } else if (is_type_qualifier_kind(kind)) {
            switch (kind) {
                case TokenKind::KwConst:
                    is_const = true;
                    break;
                case TokenKind::KwVolatile:
                    is_volatile = true;
                    break;
                case TokenKind::KwRestrict:
                    is_restrict = true;
                    break;
                default:
                    break;
            }
            advance();
        } else if (is_function_specifier_kind(kind)) {
            if (kind == TokenKind::KwInline)
                is_inline = true;
            if (kind == TokenKind::KwNoreturn)
                is_noreturn = true;
            if (kind == TokenKind::KwNoexcept)
                is_noexcept = true;
            if (kind == TokenKind::KwConstexpr)
                is_constexpr = true;
            advance();
        } else if (kind == TokenKind::KwAlignas) {
            has_alignas = true;
            advance();
            if (current().kind == TokenKind::LParen) {
                advance();
                // alignas(type-id) or alignas(expression) — consume tokens
                // until matching paren.
                int depth = 1;
                while (depth > 0 && current().kind != TokenKind::Eof) {
                    if (current().kind == TokenKind::LParen)
                        depth++;
                    else if (current().kind == TokenKind::RParen)
                        depth--;
                    advance();
                }
            }
        } else if (is_type_specifier_kind(kind)) {
            if (saw_type_specifier) {
                // Combine multiple type specifiers (e.g. "long long",
                // "unsigned int", "signed char").
                auto combined = combine_type_specifiers(kind);
                if (combined) {
                    type = combined;
                } else {
                    auto t2 = parse_type_specifier();
                    if (t2)
                        type = t2;
                }
            } else {
                type = parse_type_specifier();
                saw_type_specifier = true;
            }
        } else {
            break;
        }
    }

    if (!saw_type_specifier) {
        error("missing type specifier; defaulting to int");
        type = int_type();
    }

    return type;
}

std::shared_ptr<Type> Parser::combine_type_specifiers(TokenKind first_kind) {
    auto second = parse_type_specifier();
    if (!second)
        return nullptr;

    switch (first_kind) {
        case TokenKind::KwLong:
            if (second->kind == TypeKind::LongLong)
                return long_long_type();
            if (second->kind == TypeKind::Int)
                return long_type();
            if (second->kind == TypeKind::Double)
                return long_type(); // long double -> long (approximate)
            return second;
        case TokenKind::KwShort:
            if (second->kind == TypeKind::Int)
                return short_type();
            return second;
        case TokenKind::KwSigned:
            if (second->kind == TypeKind::Int)
                return int_type();
            if (second->kind == TypeKind::Char)
                return char_type();
            if (second->kind == TypeKind::Short)
                return short_type();
            if (second->kind == TypeKind::Long)
                return long_type();
            if (second->kind == TypeKind::LongLong)
                return long_long_type();
            return second;
        case TokenKind::KwUnsigned:
            if (second->kind == TypeKind::Int)
                return int_type();
            if (second->kind == TypeKind::Char)
                return char_type();
            if (second->kind == TypeKind::Short)
                return short_type();
            if (second->kind == TypeKind::Long)
                return long_type();
            if (second->kind == TypeKind::LongLong)
                return long_long_type();
            return second;
        default:
            return second;
    }
}

std::shared_ptr<Type> Parser::parse_type_specifier() {
    TokenKind kind = current().kind;
    switch (kind) {
        case TokenKind::KwVoid:
            advance();
            return void_type();
        case TokenKind::KwBool:
            advance();
            return bool_type();
        case TokenKind::KwChar:
            advance();
            return char_type();
        case TokenKind::KwShort:
            advance();
            return short_type();
        case TokenKind::KwInt:
            advance();
            return int_type();
        case TokenKind::KwLong:
            advance();
            return long_type();
        case TokenKind::KwFloat:
            advance();
            return float_type();
        case TokenKind::KwDouble:
            advance();
            return double_type();
        case TokenKind::KwAuto:
            advance();
            return auto_type();
        case TokenKind::KwDecltype: {
            advance();
            if (!expect(TokenKind::LParen)) {
                return nullptr;
            }
            auto expr = parse_expression();
            if (!expr) {
                error("expected expression in decltype");
                return nullptr;
            }
            if (!expect(TokenKind::RParen)) {
                return nullptr;
            }
            auto dt = std::make_shared<Type>();
            dt->kind = TypeKind::Decltype;
            dt->decltype_expr = expr;
            dt->name = "decltype";
            return dt;
        }
        case TokenKind::KwStruct:
            return parse_struct_or_union();
        case TokenKind::KwUnion:
            return parse_struct_or_union();
        case TokenKind::KwEnum:
            return parse_enum_specifier();
        case TokenKind::KwClass:
            return parse_class_spec();
        default:
            if (kind == TokenKind::Identifier) {
                // Typedef name — unknown without symbol table.  Treat as
                // void and report.
                error("unknown type name: " + std::string(current().text));
                advance();
                return void_type();
            }
            error("expected type specifier");
            return nullptr;
    }
}

std::shared_ptr<Type> Parser::parse_struct_or_union() {
    TokenKind kind = current().kind; // KwStruct or KwUnion
    advance();
    TypeKind type_kind = (kind == TokenKind::KwStruct) ? TypeKind::Struct
                                                       : TypeKind::Union;

    auto type = std::make_shared<Type>();
    type->kind = type_kind;

    // Optional tag.
    if (current().kind == TokenKind::Identifier) {
        type->name = current().text; // string_view into source buffer
        advance();
    }

    if (consume(TokenKind::LBrace)) {
        while (!at_eof() && current().kind != TokenKind::RBrace) {
            // struct-declaration: specifier-qualifier-list declarator-list ';'
            StorageClass storage = StorageClass::None;
            Linkage linkage = Linkage::None;
            bool is_typedef = false, is_extern = false, is_static = false;
            bool is_const = false, is_volatile = false, is_restrict = false;
            bool is_inline = false, is_noreturn = false;
            bool is_noexcept = false, is_constexpr = false, has_alignas = false;

            auto member_type = parse_declaration_specifiers(
                storage, linkage, is_typedef, is_extern, is_static, is_const,
                is_volatile, is_restrict, is_inline, is_noreturn,
                is_noexcept, is_constexpr, has_alignas);
            if (!member_type)
                break;

            while (true) {
                auto member_decl = std::make_shared<Declaration>();
                member_decl->type = member_type;
                auto declarator = parse_declarator();
                if (declarator)
                    member_decl->declarators.push_back(declarator);
                // Optional bitfield: ':' constant-expression
                if (consume(TokenKind::Colon)) {
                    (void)parse_assignment_expression();
                }
                type->members.push_back(member_decl);
                if (!consume(TokenKind::Comma))
                    break;
            }

            expect(TokenKind::Semicolon);
        }
        expect(TokenKind::RBrace);
    }

    return type;
}

std::shared_ptr<Type> Parser::parse_class_spec() {
    expect(TokenKind::KwClass);

    auto type = std::make_shared<Type>();
    type->kind = TypeKind::Struct;

    // Optional tag (class name).
    if (current().kind == TokenKind::Identifier) {
        type->name = current().text;
        advance();
    }

    // Inheritance clause: `:` base-specifier-list (single inheritance).
    if (consume(TokenKind::Colon)) {
        // Parse base-specifier-list: [access] base-type (',' [access] base-type)*
        while (!at_eof() && current().kind != TokenKind::LBrace) {
            // Optional access specifier: public / private / protected
            if (current().kind == TokenKind::KwPublic ||
                current().kind == TokenKind::KwPrivate ||
                current().kind == TokenKind::KwProtected) {
                advance();  // skip access specifier
            }
            // Base class name (identifier).
            if (current().kind == TokenKind::Identifier) {
                std::string base_name = std::string(current().text);
                advance();
                // Create an incomplete type for the base class; Sema will
                // resolve it later. We store the name for now.
                auto base_type = std::make_shared<Type>();
                base_type->kind = TypeKind::Struct;
                base_type->name = std::move(base_name);
                type->base_classes.push_back(base_type);
            }
            if (!consume(TokenKind::Comma)) break;
        }
    }

    if (consume(TokenKind::LBrace)) {
        AccessSpecifier access = AccessSpecifier::Public;
        while (!at_eof() && current().kind != TokenKind::RBrace) {
            // Access specifier: public:, private:, protected:
            if (current().kind == TokenKind::KwPublic &&
                peek().kind == TokenKind::Colon) {
                access = AccessSpecifier::Public;
                advance();  // KwPublic
                advance();  // Colon
                continue;
            }
            if (current().kind == TokenKind::KwPrivate &&
                peek().kind == TokenKind::Colon) {
                access = AccessSpecifier::Private;
                advance();
                advance();
                continue;
            }
            if (current().kind == TokenKind::KwProtected &&
                peek().kind == TokenKind::Colon) {
                access = AccessSpecifier::Protected;
                advance();
                advance();
                continue;
            }

            // Member declaration: declaration-specifiers declarator...
            StorageClass m_storage = StorageClass::None;
            Linkage m_linkage = Linkage::None;
            bool m_typedef = false, m_extern = false, m_static = false;
            bool m_const = false, m_volatile = false, m_restrict = false;
            bool m_inline = false, m_noreturn = false;
            bool m_virtual = false;
            bool m_noexcept = false, m_constexpr = false, m_has_alignas = false;

            // C++ virtual keyword before member function declaration.
            if (consume(TokenKind::KwVirtual)) {
                m_virtual = true;
            }

            auto base_type = parse_declaration_specifiers(
                m_storage, m_linkage, m_typedef, m_extern, m_static,
                m_const, m_volatile, m_restrict, m_inline, m_noreturn,
                m_noexcept, m_constexpr, m_has_alignas);
            if (!base_type)
                break;

            while (true) {
                auto member_decl = std::make_shared<Declaration>();
                member_decl->access = access;
                member_decl->type = base_type;
                member_decl->is_virtual = m_virtual;
                member_decl->is_inline = m_inline;
                member_decl->is_noreturn = m_noreturn;
                member_decl->is_noexcept = m_noexcept;
                member_decl->is_constexpr = m_constexpr;
                member_decl->has_alignas = m_has_alignas;
                auto declarator = parse_declarator();
                if (!declarator)
                    break;
                member_decl->declarators.push_back(declarator);

                // Constructor/destructor detection: declarator name matches
                // class tag or starts with '~'.
                if (declarator->name == type->name) {
                    member_decl->is_constructor = true;
                } else if (!declarator->name.empty() &&
                           declarator->name[0] == '~') {
                    member_decl->is_destructor = true;
                    member_decl->is_constructor = false;
                }

                // Optional bitfield: ':' constant-expression
                if (consume(TokenKind::Colon)) {
                    (void)parse_assignment_expression();
                }

                // Function body: declarator is a function followed by '{'.
                if (declarator->is_function &&
                    current().kind == TokenKind::LBrace) {
                    // Function definition inside class body (inline).
                    auto func_def = std::make_shared<FunctionDef>();
                    auto decl2 = std::make_shared<Declaration>();
                    decl2->access = access;
                    decl2->type = base_type;
                    decl2->is_virtual = m_virtual;
                    decl2->is_inline = m_inline;
                    decl2->is_noreturn = m_noreturn;
                    decl2->is_noexcept = m_noexcept;
                    decl2->is_constexpr = m_constexpr;
                    decl2->has_alignas = m_has_alignas;
                    decl2->declarators.push_back(declarator);
                    if (declarator->name == type->name)
                        decl2->is_constructor = true;
                    func_def->declaration = decl2;
                    func_def->body = parse_compound_statement();
                    // Add as a member declaration.
                    type->members.push_back(decl2);
                } else {
                    type->members.push_back(member_decl);
                }

                if (!consume(TokenKind::Comma))
                    break;
            }

            expect(TokenKind::Semicolon);
        }
        expect(TokenKind::RBrace);
    }

    return type;
}

std::shared_ptr<Type> Parser::parse_enum_specifier() {
    expect(TokenKind::KwEnum);
    auto type = std::make_shared<Type>();
    type->kind = TypeKind::Enum;

    if (current().kind == TokenKind::Identifier) {
        type->name = current().text;
        advance();
    }

    if (consume(TokenKind::LBrace)) {
        while (!at_eof() && current().kind != TokenKind::RBrace) {
            (void)parse_assignment_expression(); // enumerator value (placeholder)
            if (!consume(TokenKind::Comma))
                break;
        }
        expect(TokenKind::RBrace);
    }

    return type;
}

void Parser::parse_init_declarator_list(std::shared_ptr<Declaration> decl,
                                        std::shared_ptr<Type> base_type) {
    (void)base_type;
    while (true) {
        auto declarator = parse_declarator();
        if (!declarator)
            break;
        decl->declarators.push_back(declarator);

        if (consume(TokenKind::Eq)) {
            auto init = parse_initializer();
            if (init)
                decl->initializers.push_back(init);
        }

        if (!consume(TokenKind::Comma))
            break;
    }
}

std::shared_ptr<Declarator> Parser::parse_declarator() {
    auto decl = std::make_shared<Declarator>();
    decl->location = current().location;

    // pointer?
    if (consume(TokenKind::Star)) {
        decl->is_pointer = true;
        while (is_type_qualifier_kind(current().kind)) {
            advance();
        }
    }

    // direct-declarator
    if (current().kind == TokenKind::LParen) {
        // '(' declarator ')'
        advance();
        (void)parse_declarator();
        expect(TokenKind::RParen);
    } else if (current().kind == TokenKind::Identifier) {
        decl->name = current().text;
        advance();
    } else {
        // Abstract declarator (no identifier) — return null.
        return nullptr;
    }

    // direct-declarator suffixes: '[' ... ']' | '(' ... ')'
    while (true) {
        if (consume(TokenKind::LBracket)) {
            if (current().kind != TokenKind::RBracket) {
                auto size_expr = parse_assignment_expression();
                if (size_expr)
                    decl->array_sizes.emplace_back(size_expr->int_value, false);
            }
            expect(TokenKind::RBracket);
        } else if (consume(TokenKind::LParen)) {
            decl->is_function = true;
            // Handle "void" parameter list (no parameters).
            if (current().kind == TokenKind::KwVoid) {
                advance();
                expect(TokenKind::RParen);
            } else {
                while (!at_eof() && current().kind != TokenKind::RParen) {
                    if (current().kind == TokenKind::Ellipsis) {
                        decl->is_variadic = true;
                        advance();
                        break;
                    }
                    // Parse parameter type specifiers.
                    StorageClass p_storage = StorageClass::None;
                    Linkage p_linkage = Linkage::None;
                    bool p_typedef = false, p_extern = false, p_static = false;
                    bool p_const = false, p_volatile = false, p_restrict = false;
                    bool p_inline = false, p_noreturn = false;
                    bool p_noexcept = false, p_constexpr = false, p_has_alignas = false;
                    auto param_type = parse_declaration_specifiers(
                        p_storage, p_linkage, p_typedef, p_extern, p_static,
                        p_const, p_volatile, p_restrict, p_inline, p_noreturn,
                        p_noexcept, p_constexpr, p_has_alignas);
                    decl->param_types.push_back(
                        param_type ? param_type : int_type());
                    // Capture the parameter name (identifier).
                    if (!at_eof() && current().kind == TokenKind::Identifier) {
                        decl->param_names.push_back(
                            std::string(current().text));
                        advance();
                    }
                    // Skip the rest of the declarator (array sizes, etc.).
                    while (!at_eof() && current().kind != TokenKind::Comma &&
                           current().kind != TokenKind::RParen &&
                           current().kind != TokenKind::Ellipsis) {
                        advance();
                    }
                    if (!consume(TokenKind::Comma))
                        break;
                }
                expect(TokenKind::RParen);
            }
        } else {
            break;
        }
    }

    return decl;
}

std::shared_ptr<Declaration> Parser::parse_declaration_block_scope() {
    StorageClass storage = StorageClass::None;
    Linkage linkage = Linkage::None;
    bool is_typedef = false, is_extern = false, is_static = false;
    bool is_const = false, is_volatile = false, is_restrict = false;
    bool is_inline = false, is_noreturn = false;
    bool is_noexcept = false, is_constexpr = false, has_alignas = false;

    auto base_type = parse_declaration_specifiers(storage, linkage, is_typedef,
                                                  is_extern, is_static, is_const,
                                                  is_volatile, is_restrict,
                                                  is_inline, is_noreturn,
                                                  is_noexcept, is_constexpr, has_alignas);
    if (!base_type)
        return nullptr;

    // Check for structured binding: type [name1, name2, ...] = initializer
    if (current().kind == TokenKind::LBracket) {
        auto stmt = std::make_shared<Statement>();
        stmt->kind = StatementKind::StructuredBinding;
        stmt->is_structured_binding = true;
        stmt->bound_type = base_type;
        stmt->location = current().location;

        advance(); // consume '['
        while (current().kind != TokenKind::RBracket && current().kind != TokenKind::Eof) {
            if (current().kind == TokenKind::Identifier) {
                stmt->bound_names.push_back(std::string(current().text));
                advance();
            } else {
                error("expected bound name in structured binding");
                return nullptr;
            }
            if (consume(TokenKind::Comma))
                continue;
            break;
        }
        expect(TokenKind::RBracket);

        if (consume(TokenKind::Eq)) {
            stmt->bound_initializer = parse_initializer_expression();
        } else {
            error("expected '=' after structured binding");
            return nullptr;
        }

        expect(TokenKind::Semicolon);
        pending_structured_binding_ = stmt;
        return nullptr;
    }

    auto decl = std::make_shared<Declaration>();
    decl->location = current().location;
    decl->storage = storage;
    decl->linkage = linkage;
    decl->is_typedef = is_typedef;
    decl->is_extern = is_extern;
    decl->is_static = is_static;
    decl->is_const = is_const;
    decl->is_volatile = is_volatile;
    decl->is_restrict = is_restrict;
    decl->is_inline = is_inline;
    decl->is_noreturn = is_noreturn;
    decl->is_noexcept = is_noexcept;
    decl->is_constexpr = is_constexpr;
    decl->has_alignas = has_alignas;
    decl->type = base_type;

    parse_init_declarator_list(decl, base_type);
    expect(TokenKind::Semicolon);
    return decl;
}

// --- Statements -------------------------------------------------------

std::shared_ptr<Statement> Parser::parse_statement() {
    TokenKind kind = current().kind;

    switch (kind) {
        case TokenKind::LBrace:
            return parse_compound_statement();
        case TokenKind::KwIf:
        case TokenKind::KwSwitch:
            return parse_selection_statement();
        case TokenKind::KwWhile:
        case TokenKind::KwDo:
        case TokenKind::KwFor:
            return parse_iteration_statement();
        case TokenKind::KwGoto:
        case TokenKind::KwContinue:
        case TokenKind::KwBreak:
        case TokenKind::KwReturn:
            return parse_jump_statement();
        default:
            break;
    }

    // Declaration statement.
    if (is_declaration_specifier_kind(kind)) {
        auto decl = parse_declaration_block_scope();
        if (pending_structured_binding_) {
            auto stmt = pending_structured_binding_;
            pending_structured_binding_ = nullptr;
            return stmt;
        }
        if (!decl)
            return nullptr;
        auto stmt = std::make_shared<Statement>();
        stmt->kind = StatementKind::Declaration;
        stmt->declaration = decl;
        stmt->location = decl->location;
        return stmt;
    }

    return parse_expression_statement();
}

std::shared_ptr<Statement> Parser::parse_expression_statement() {
    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::Expression;
    stmt->location = current().location;

    if (current().kind != TokenKind::Semicolon) {
        stmt->expression = parse_expression();
    }

    expect(TokenKind::Semicolon);
    return stmt;
}

std::shared_ptr<Statement> Parser::parse_compound_statement() {
    expect(TokenKind::LBrace);
    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::Compound;
    stmt->location = current().location;

    while (!at_eof() && current().kind != TokenKind::RBrace) {
        auto inner = parse_statement();
        if (!inner)
            break;
        stmt->block_items.push_back(inner);
    }

    expect(TokenKind::RBrace);
    return stmt;
}

std::shared_ptr<Statement> Parser::parse_selection_statement() {
    if (consume(TokenKind::KwIf)) {
        auto stmt = std::make_shared<Statement>();
        stmt->kind = StatementKind::If;
        stmt->location = current().location;
        // C++17: if constexpr ( condition ) ...
        if (consume(TokenKind::KwConstexpr))
            stmt->is_constexpr_if = true;
        expect(TokenKind::LParen);
        stmt->condition = parse_expression();
        expect(TokenKind::RParen);
        stmt->then_branch = parse_statement();
        if (consume(TokenKind::KwElse)) {
            stmt->else_branch = parse_statement();
        }
        return stmt;
    }

    // switch
    expect(TokenKind::KwSwitch);
    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::Switch;
    stmt->location = current().location;
    expect(TokenKind::LParen);
    stmt->condition = parse_expression();
    expect(TokenKind::RParen);
    stmt->loop_body = parse_statement();
    return stmt;
}

std::shared_ptr<Statement> Parser::parse_iteration_statement() {
    if (consume(TokenKind::KwWhile)) {
        auto stmt = std::make_shared<Statement>();
        stmt->kind = StatementKind::While;
        stmt->location = current().location;
        expect(TokenKind::LParen);
        stmt->condition = parse_expression();
        expect(TokenKind::RParen);
        stmt->loop_body = parse_statement();
        return stmt;
    }

    if (consume(TokenKind::KwDo)) {
        auto stmt = std::make_shared<Statement>();
        stmt->kind = StatementKind::DoWhile;
        stmt->location = current().location;
        stmt->loop_body = parse_statement();
        expect(TokenKind::KwWhile);
        expect(TokenKind::LParen);
        stmt->for_condition = parse_expression();
        expect(TokenKind::RParen);
        expect(TokenKind::Semicolon);
        return stmt;
    }

    // for
    expect(TokenKind::KwFor);
    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::For;
    stmt->location = current().location;
    expect(TokenKind::LParen);

    // for-init
    if (current().kind != TokenKind::Semicolon) {
        if (is_declaration_specifier_kind(current().kind)) {
            auto decl = parse_declaration_block_scope();
            // Check for range-based for: for ( decl : range_expr ) body
            if (consume(TokenKind::Colon)) {
                stmt->is_range_for = true;
                stmt->declaration = decl;
                stmt->for_range_expr = parse_expression();
                expect(TokenKind::RParen);
                stmt->loop_body = parse_statement();
                return stmt;
            }
            stmt->for_init = std::make_shared<Statement>();
            stmt->for_init->kind = StatementKind::Declaration;
            stmt->for_init->declaration = decl;
        } else {
            stmt->expression = parse_expression();
            expect(TokenKind::Semicolon);
        }
    } else {
        advance();
    }

    // for-condition
    if (current().kind != TokenKind::Semicolon) {
        stmt->for_condition = parse_expression();
    }
    expect(TokenKind::Semicolon);

    // for-increment (shared_ptr<Expression>, not Statement)
    if (current().kind != TokenKind::RParen) {
        stmt->for_increment = parse_expression();
    }
    expect(TokenKind::RParen);

    stmt->loop_body = parse_statement();
    return stmt;
}

std::shared_ptr<Statement> Parser::parse_jump_statement() {
    if (consume(TokenKind::KwGoto)) {
        auto stmt = std::make_shared<Statement>();
        stmt->kind = StatementKind::Goto;
        stmt->location = current().location;
        if (current().kind == TokenKind::Identifier) {
            stmt->goto_target = std::string(current().text);
            advance();
        } else {
            error("expected identifier after goto");
        }
        expect(TokenKind::Semicolon);
        return stmt;
    }

    if (consume(TokenKind::KwContinue)) {
        auto stmt = std::make_shared<Statement>();
        stmt->kind = StatementKind::Continue;
        stmt->location = current().location;
        expect(TokenKind::Semicolon);
        return stmt;
    }

    if (consume(TokenKind::KwBreak)) {
        auto stmt = std::make_shared<Statement>();
        stmt->kind = StatementKind::Break;
        stmt->location = current().location;
        expect(TokenKind::Semicolon);
        return stmt;
    }

    // return
    expect(TokenKind::KwReturn);
    auto stmt = std::make_shared<Statement>();
    stmt->kind = StatementKind::Return;
    stmt->location = current().location;
    if (current().kind != TokenKind::Semicolon) {
        stmt->return_value = parse_expression();
    }
    expect(TokenKind::Semicolon);
    return stmt;
}

std::shared_ptr<Statement> Parser::parse_labeled_statement() {
    (void)0;
    return nullptr;
}

// --- Expressions ------------------------------------------------------

std::shared_ptr<Expression> Parser::parse_expression() {
    auto left = parse_assignment_expression();
    if (!left)
        return nullptr;
    while (consume(TokenKind::Comma)) {
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = BinaryOp::Comma;
        expr->binary_left = left;
        expr->binary_right = parse_assignment_expression();
        if (!expr->binary_right) {
            error("expected expression after ','");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_assignment_expression() {
    auto left = parse_conditional_expression();
    if (!left)
        return nullptr;

    TokenKind kind = current().kind;
    switch (kind) {
        case TokenKind::Eq:
        case TokenKind::StarEq:
        case TokenKind::SlashEq:
        case TokenKind::PercentEq:
        case TokenKind::PlusEq:
        case TokenKind::MinusEq:
        case TokenKind::LessLessEq:
        case TokenKind::GreaterGreaterEq:
        case TokenKind::AmpEq:
        case TokenKind::CaretEq:
        case TokenKind::PipeEq: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Assignment;
            switch (kind) {
                case TokenKind::Eq:
                    expr->assign_op = AssignmentOp::Assign;
                    break;
                case TokenKind::StarEq:
                    expr->assign_op = AssignmentOp::MulAssign;
                    break;
                case TokenKind::SlashEq:
                    expr->assign_op = AssignmentOp::DivAssign;
                    break;
                case TokenKind::PercentEq:
                    expr->assign_op = AssignmentOp::RemAssign;
                    break;
                case TokenKind::PlusEq:
                    expr->assign_op = AssignmentOp::AddAssign;
                    break;
                case TokenKind::MinusEq:
                    expr->assign_op = AssignmentOp::SubAssign;
                    break;
                case TokenKind::LessLessEq:
                    expr->assign_op = AssignmentOp::ShlAssign;
                    break;
                case TokenKind::GreaterGreaterEq:
                    expr->assign_op = AssignmentOp::ShrAssign;
                    break;
                case TokenKind::AmpEq:
                    expr->assign_op = AssignmentOp::AndAssign;
                    break;
                case TokenKind::CaretEq:
                    expr->assign_op = AssignmentOp::XorAssign;
                    break;
                case TokenKind::PipeEq:
                    expr->assign_op = AssignmentOp::OrAssign;
                    break;
                default:
                    expr->assign_op = AssignmentOp::Assign;
                    break;
            }
            expr->assign_left = left;
            expr->assign_right = parse_assignment_expression();
            if (!expr->assign_right) {
                error("expected expression after assignment operator");
                return left;
            }
            return expr;
        }
        default:
            return left;
    }
}

std::shared_ptr<Expression> Parser::parse_conditional_expression() {
    auto cond = parse_logical_or_expression();
    if (!cond)
        return nullptr;
    if (consume(TokenKind::Question)) {
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Conditional;
        expr->cond_condition = cond;
        expr->cond_true = parse_expression();
        expect(TokenKind::Colon);
        expr->cond_false = parse_conditional_expression();
        return expr;
    }
    return cond;
}

std::shared_ptr<Expression> Parser::parse_logical_or_expression() {
    auto left = parse_logical_and_expression();
    if (!left)
        return nullptr;
    while (consume(TokenKind::PipePipe)) {
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = BinaryOp::LogicalOr;
        expr->binary_left = left;
        expr->binary_right = parse_logical_and_expression();
        if (!expr->binary_right) {
            error("expected expression after '||'");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_logical_and_expression() {
    auto left = parse_inclusive_or_expression();
    if (!left)
        return nullptr;
    while (consume(TokenKind::AmpAmp)) {
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = BinaryOp::LogicalAnd;
        expr->binary_left = left;
        expr->binary_right = parse_inclusive_or_expression();
        if (!expr->binary_right) {
            error("expected expression after '&&'");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_inclusive_or_expression() {
    auto left = parse_exclusive_or_expression();
    if (!left)
        return nullptr;
    while (consume(TokenKind::Pipe)) {
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = BinaryOp::BitOr;
        expr->binary_left = left;
        expr->binary_right = parse_exclusive_or_expression();
        if (!expr->binary_right) {
            error("expected expression after '|'");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_exclusive_or_expression() {
    auto left = parse_and_expression();
    if (!left)
        return nullptr;
    while (consume(TokenKind::Caret)) {
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = BinaryOp::BitXor;
        expr->binary_left = left;
        expr->binary_right = parse_and_expression();
        if (!expr->binary_right) {
            error("expected expression after '^'");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_and_expression() {
    auto left = parse_equality_expression();
    if (!left)
        return nullptr;
    while (consume(TokenKind::Amp)) {
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = BinaryOp::BitAnd;
        expr->binary_left = left;
        expr->binary_right = parse_equality_expression();
        if (!expr->binary_right) {
            error("expected expression after '&'");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_equality_expression() {
    auto left = parse_relational_expression();
    if (!left)
        return nullptr;
    while (true) {
        BinaryOp op = BinaryOp::Add;
        if (consume(TokenKind::EqEq))
            op = BinaryOp::Eq;
        else if (consume(TokenKind::ExclaimEq))
            op = BinaryOp::Ne;
        else
            break;
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = op;
        expr->binary_left = left;
        expr->binary_right = parse_relational_expression();
        if (!expr->binary_right) {
            error("expected expression after relational operator");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_relational_expression() {
    auto left = parse_shift_expression();
    if (!left)
        return nullptr;
    while (true) {
        BinaryOp op = BinaryOp::Add;
        if (consume(TokenKind::Less))
            op = BinaryOp::Lt;
        else if (consume(TokenKind::Greater))
            op = BinaryOp::Gt;
        else if (consume(TokenKind::LessEq))
            op = BinaryOp::Le;
        else if (consume(TokenKind::GreaterEq))
            op = BinaryOp::Ge;
        else
            break;
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = op;
        expr->binary_left = left;
        expr->binary_right = parse_shift_expression();
        if (!expr->binary_right) {
            error("expected expression after relational operator");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_shift_expression() {
    auto left = parse_additive_expression();
    if (!left)
        return nullptr;
    while (true) {
        BinaryOp op = BinaryOp::Add;
        if (consume(TokenKind::LessLess))
            op = BinaryOp::Shl;
        else if (consume(TokenKind::GreaterGreater))
            op = BinaryOp::Shr;
        else
            break;
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = op;
        expr->binary_left = left;
        expr->binary_right = parse_additive_expression();
        if (!expr->binary_right) {
            error("expected expression after shift operator");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_additive_expression() {
    auto left = parse_multiplicative_expression();
    if (!left)
        return nullptr;
    while (true) {
        BinaryOp op = BinaryOp::Add;
        if (consume(TokenKind::Plus))
            op = BinaryOp::Add;
        else if (consume(TokenKind::Minus))
            op = BinaryOp::Sub;
        else
            break;
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = op;
        expr->binary_left = left;
        expr->binary_right = parse_multiplicative_expression();
        if (!expr->binary_right) {
            error("expected expression after additive operator");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_multiplicative_expression() {
    auto left = parse_cast_expression();
    if (!left)
        return nullptr;
    while (true) {
        BinaryOp op = BinaryOp::Mul;
        if (consume(TokenKind::Star))
            op = BinaryOp::Mul;
        else if (consume(TokenKind::Slash))
            op = BinaryOp::Div;
        else if (consume(TokenKind::Percent))
            op = BinaryOp::Rem;
        else
            break;
        auto expr = std::make_shared<Expression>();
        expr->kind = ExpressionKind::Binary;
        expr->binary_op = op;
        expr->binary_left = left;
        expr->binary_right = parse_cast_expression();
        if (!expr->binary_right) {
            error("expected expression after multiplicative operator");
            return left;
        }
        left = expr;
    }
    return left;
}

std::shared_ptr<Expression> Parser::parse_cast_expression() {
    // Try to parse a cast: '(' type-name ')' cast-expression.
    // Heuristic: if after '(' we see something that could start a type
    // name, attempt a cast parse; if ')' doesn't follow, fall back to
    // parenthesized expression.
    if (consume(TokenKind::LParen)) {
        if (is_cast_start(current().kind)) {
            StorageClass storage = StorageClass::None;
            Linkage linkage = Linkage::None;
            bool is_typedef = false, is_extern = false, is_static = false;
            bool is_const = false, is_volatile = false, is_restrict = false;
            bool is_inline = false, is_noreturn = false;
            bool is_noexcept = false, is_constexpr = false, has_alignas = false;

            auto type = parse_declaration_specifiers(
                storage, linkage, is_typedef, is_extern, is_static, is_const,
                is_volatile, is_restrict, is_inline, is_noreturn,
                is_noexcept, is_constexpr, has_alignas);
            if (type && consume(TokenKind::RParen)) {
                auto expr = std::make_shared<Expression>();
                expr->kind = ExpressionKind::Cast;
                expr->cast_type = type;
                expr->cast_operand = parse_cast_expression();
                if (!expr->cast_operand) {
                    error("expected expression after cast");
                    return nullptr;
                }
                return expr;
            }
        }
        // Not a cast — parenthesized expression.
        auto inner = parse_expression();
        expect(TokenKind::RParen);
        return inner;
    }
    return parse_unary_expression();
}

bool Parser::is_cast_start(TokenKind kind) const {
    return is_type_specifier_kind(kind) || is_type_qualifier_kind(kind) ||
           kind == TokenKind::Star || kind == TokenKind::LParen;
}

std::shared_ptr<Expression> Parser::parse_unary_expression() {
    TokenKind kind = current().kind;
    switch (kind) {
        case TokenKind::PlusPlus: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Unary;
            expr->unary_op = UnaryOp::PreInc;
            expr->unary_operand = parse_cast_expression();
            return expr;
        }
        case TokenKind::MinusMinus: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Unary;
            expr->unary_op = UnaryOp::PreDec;
            expr->unary_operand = parse_cast_expression();
            return expr;
        }
        case TokenKind::Amp: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Unary;
            expr->unary_op = UnaryOp::AddressOf;
            expr->unary_operand = parse_cast_expression();
            return expr;
        }
        case TokenKind::Star: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Unary;
            expr->unary_op = UnaryOp::Deref;
            expr->unary_operand = parse_cast_expression();
            return expr;
        }
        case TokenKind::Plus: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Unary;
            expr->unary_op = UnaryOp::UnaryPlus;
            expr->unary_operand = parse_cast_expression();
            return expr;
        }
        case TokenKind::Minus: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Unary;
            expr->unary_op = UnaryOp::UnaryMinus;
            expr->unary_operand = parse_cast_expression();
            return expr;
        }
        case TokenKind::Exclaim: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Unary;
            expr->unary_op = UnaryOp::LogicalNot;
            expr->unary_operand = parse_cast_expression();
            return expr;
        }
        case TokenKind::Tilde: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Unary;
            expr->unary_op = UnaryOp::BitwiseNot;
            expr->unary_operand = parse_cast_expression();
            return expr;
        }
        case TokenKind::KwSizeof: {
            advance();
            if (consume(TokenKind::LParen)) {
                if (is_cast_start(current().kind)) {
                    StorageClass storage = StorageClass::None;
                    Linkage linkage = Linkage::None;
                    bool is_typedef = false, is_extern = false,
                         is_static = false;
                    bool is_const = false, is_volatile = false,
                         is_restrict = false;
                    bool is_inline = false, is_noreturn = false;
                    bool is_noexcept = false, is_constexpr = false, has_alignas = false;
                    auto type = parse_declaration_specifiers(
                        storage, linkage, is_typedef, is_extern, is_static,
                        is_const, is_volatile, is_restrict, is_inline,
                        is_noreturn, is_noexcept, is_constexpr, has_alignas);
                    if (type && consume(TokenKind::RParen)) {
                        auto expr = std::make_shared<Expression>();
                        expr->kind = ExpressionKind::SizeofType;
                        expr->sizeof_type = type;
                        return expr;
                    }
                }
                auto expr = parse_expression();
                expect(TokenKind::RParen);
                auto sizeof_expr = std::make_shared<Expression>();
                sizeof_expr->kind = ExpressionKind::SizeofExpr;
                sizeof_expr->sizeof_operand = expr;
                return sizeof_expr;
            }
            auto operand = parse_cast_expression();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::SizeofExpr;
            expr->sizeof_operand = operand;
            return expr;
        }
        case TokenKind::KwNew: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::New;
            // Parse the type being allocated.
            StorageClass storage = StorageClass::None;
            Linkage linkage = Linkage::None;
            bool is_typedef = false, is_extern = false, is_static = false;
            bool is_const = false, is_volatile = false, is_restrict = false;
            bool is_inline = false, is_noreturn = false;
            bool is_noexcept = false, is_constexpr = false, has_alignas = false;
            expr->new_type = parse_declaration_specifiers(
                storage, linkage, is_typedef, is_extern, is_static,
                is_const, is_volatile, is_restrict, is_inline, is_noreturn,
                is_noexcept, is_constexpr, has_alignas);
            if (!expr->new_type) {
                error("expected type after 'new'");
                return nullptr;
            }
            // Optional array size for `new[]`.
            if (consume(TokenKind::LBracket)) {
                if (current().kind != TokenKind::RBracket) {
                    expr->new_alloc_expr = parse_expression();
                }
                expect(TokenKind::RBracket);
            }
            return expr;
        }
        case TokenKind::KwDelete: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Delete;
            expr->delete_operand = parse_cast_expression();
            if (!expr->delete_operand) {
                error("expected expression after 'delete'");
                return nullptr;
            }
            return expr;
        }
        case TokenKind::KwMove: {
            advance();
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Move;
            expr->move_operand = parse_cast_expression();
            if (!expr->move_operand) {
                error("expected expression after 'move'");
                return nullptr;
            }
            return expr;
        }
        default:
            return parse_postfix_expression();
    }
}

std::shared_ptr<Expression> Parser::parse_postfix_expression() {
    auto base = parse_primary_expression();
    if (!base)
        return nullptr;

    while (true) {
        if (consume(TokenKind::LBracket)) {
            auto sub = std::make_shared<Expression>();
            sub->kind = ExpressionKind::ArraySubscript;
            sub->array_base = base;
            if (current().kind != TokenKind::RBracket) {
                sub->array_index = parse_expression();
            }
            expect(TokenKind::RBracket);
            base = sub;
        } else if (consume(TokenKind::LParen)) {
            auto call = std::make_shared<Expression>();
            call->kind = ExpressionKind::FunctionCall;
            call->callee = base;
            while (!at_eof() && current().kind != TokenKind::RParen) {
                call->call_args.push_back(parse_assignment_expression());
                if (!consume(TokenKind::Comma))
                    break;
            }
            expect(TokenKind::RParen);
            base = call;
        } else if (consume(TokenKind::Period)) {
            auto member = std::make_shared<Expression>();
            member->kind = ExpressionKind::MemberAccess;
            member->is_ptr_member_access = false;
            if (current().kind == TokenKind::Identifier) {
                member->member_name = current().text;
                advance();
            } else {
                error("expected identifier after '.'");
            }
            member->member_base = base;
            base = member;
        } else if (consume(TokenKind::Arrow)) {
            auto member = std::make_shared<Expression>();
            member->kind = ExpressionKind::MemberAccess;
            member->is_ptr_member_access = true;
            if (current().kind == TokenKind::Identifier) {
                member->member_name = current().text;
                advance();
            } else {
                error("expected identifier after '->'");
            }
            member->member_base = base;
            base = member;
        } else if (is_fold_op(current().kind)) {
            // M10: fold expression. Check if operator is followed by '...'.
            // Consume the operator, peek at next token, push back if not a fold.
            Token op_token = current();
            advance();  // consume the operator

            if (current().kind == TokenKind::Ellipsis) {
                // Fold expression! Both operator and '...' consumed.
                advance();  // consume '...'

                auto fold = std::make_shared<Expression>();
                fold->kind = ExpressionKind::FoldExpression;
                fold->fold_operand = base;
                fold->fold_op = parse_fold_op(op_token.kind);

                if (current().kind == TokenKind::RParen) {
                    // Unary right fold: (pack op ...)
                    fold->fold_kind = 1;
                } else {
                    // Binary fold: (pack op ... op init)
                    // Expect another operator after '...'.
                    Token op2_token = current();
                    advance();  // consume the second operator
                    fold->fold_op = parse_fold_op(op2_token.kind);
                    fold->fold_init = parse_assignment_expression();
                    if (!fold->fold_init) {
                        error("expected expression after operator in fold expression");
                        return base;
                    }
                    fold->fold_kind = 2;
                }
                base = fold;
            } else {
                // Not a fold expression — restore both tokens.
                // Push back in reverse order so operator comes out first.
                preprocessor_.push_back(current());      // token after operator
                preprocessor_.push_back(op_token);        // the operator itself
                break;  // let the caller handle the binary operator
            }
        } else if (current().kind == TokenKind::Ellipsis) {
            // M10: unary left fold: (... op pack)
            // We are at '...'. The next token is the operator, then the pack.
            advance();  // consume '...'
            auto fold = std::make_shared<Expression>();
            fold->kind = ExpressionKind::FoldExpression;
            fold->fold_kind = 0;  // unary left

            // Expect an operator after '...'.
            Token op_token = current();
            fold->fold_op = parse_fold_op(op_token.kind);
            if (fold->fold_op == FoldOp::Plus && op_token.kind != TokenKind::Plus) {
                error("expected operator after '...' in fold expression");
                return base;
            }
            advance();  // consume the operator

            // The pack is the next expression.
            auto pack = parse_cast_expression();
            if (!pack) {
                error("expected pack expression after operator in fold expression");
                return base;
            }
            fold->fold_operand = pack;

            base = fold;
        } else {
            break;
        }
    }
    return base;
}

std::shared_ptr<Expression> Parser::parse_primary_expression() {
    TokenKind kind = current().kind;
    switch (kind) {
        case TokenKind::IntegerLiteral: {
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::IntegerLiteral;
            expr->int_literal_value = std::stoll(std::string(current().text));
            expr->int_value = expr->int_literal_value;
            advance();
            return expr;
        }
        case TokenKind::FloatingLiteral: {
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::FloatingLiteral;
            expr->float_literal_value =
                std::stod(std::string(current().text));
            advance();
            return expr;
        }
        case TokenKind::CharLiteral: {
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::CharLiteral;
            expr->char_value =
                current().text.empty() ? 0 : current().text[0];
            advance();
            return expr;
        }
        case TokenKind::StringLiteral: {
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::StringLiteral;
            expr->string_value = current().text;
            advance();
            return expr;
        }
        case TokenKind::Identifier: {
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::Identifier;
            expr->identifier_name = current().text;
            advance();
            return expr;
        }
        case TokenKind::KwThis: {
            auto expr = std::make_shared<Expression>();
            expr->kind = ExpressionKind::This;
            advance();
            return expr;
        }
        case TokenKind::LParen: {
            advance();
            auto expr = parse_expression();
            expect(TokenKind::RParen);
            return expr;
        }
        case TokenKind::LBracket: {
            return parse_lambda_expression();
        }
        default:
            error("expected primary expression");
            return nullptr;
    }
}

std::shared_ptr<Expression> Parser::parse_lambda_expression() {
    // We are at '['. Consume it.
    advance();

    auto expr = std::make_shared<Expression>();
    expr->kind = ExpressionKind::Lambda;

    // Parse capture list: [ captures ]
    while (current().kind != TokenKind::RBracket && current().kind != TokenKind::Eof) {
        if (consume(TokenKind::Ellipsis)) {
            expr->lambda_captures.push_back("...");
            break;
        }
        bool capture_by_ref = false;
        if (consume(TokenKind::Amp)) {
            capture_by_ref = true;
        }
        if (current().kind == TokenKind::Identifier) {
            std::string cap = std::string(current().text);
            expr->lambda_captures.push_back(capture_by_ref ? "&" + cap : cap);
            advance();
        } else if (consume(TokenKind::KwThis)) {
            expr->lambda_captures.push_back("this");
        } else {
            error("expected capture in lambda capture list");
            return nullptr;
        }
        if (current().kind == TokenKind::Comma) {
            advance();
            continue;
        }
        break;
    }
    expect(TokenKind::RBracket);

    // Optional parameter list: ( params )
    if (consume(TokenKind::LParen)) {
        while (current().kind != TokenKind::RParen && current().kind != TokenKind::Eof) {
            auto param = std::make_shared<Parameter>();
            // Parse parameter type
            // Simple type parsing: just type specifiers for now
            StorageClass storage = StorageClass::None;
            Linkage linkage = Linkage::None;
            bool is_typedef = false, is_extern = false, is_static = false;
            bool is_const = false, is_volatile = false, is_restrict = false;
            bool is_inline = false, is_noreturn = false;
            bool is_noexcept = false, is_constexpr = false, has_alignas = false;
            auto param_type = parse_declaration_specifiers(
                storage, linkage, is_typedef, is_extern, is_static,
                is_const, is_volatile, is_restrict, is_inline, is_noreturn,
                is_noexcept, is_constexpr, has_alignas);
            if (!param_type) {
                error("expected parameter type in lambda");
                return nullptr;
            }
            param->type = param_type;
            if (current().kind == TokenKind::Identifier) {
                param->declarator = std::make_shared<Declarator>();
                param->declarator->name = current().text;
                advance();
            }
            expr->lambda_params.push_back(param);
            if (current().kind == TokenKind::Comma) {
                advance();
                continue;
            }
            break;
        }
        expect(TokenKind::RParen);
    }

    // Optional trailing return type: -> Type
    if (consume(TokenKind::Minus)) {
        if (consume(TokenKind::Greater)) {
            // Parse return type
            StorageClass storage = StorageClass::None;
            Linkage linkage = Linkage::None;
            bool is_typedef = false, is_extern = false, is_static = false;
            bool is_const = false, is_volatile = false, is_restrict = false;
            bool is_inline = false, is_noreturn = false;
            bool is_noexcept = false, is_constexpr = false, has_alignas = false;
            expr->lambda_return_type = parse_declaration_specifiers(
                storage, linkage, is_typedef, is_extern, is_static,
                is_const, is_volatile, is_restrict, is_inline, is_noreturn,
                is_noexcept, is_constexpr, has_alignas);
            if (!expr->lambda_return_type) {
                error("expected return type after '->'");
                return nullptr;
            }
        }
    }

    // Lambda body: compound statement
    if (current().kind != TokenKind::LBrace) {
        error("expected '{' for lambda body");
        return nullptr;
    }
    advance(); // consume '{'

    auto body = std::make_shared<Statement>();
    body->kind = StatementKind::Compound;

    // Parse statements until '}'
    while (current().kind != TokenKind::RBrace && current().kind != TokenKind::Eof) {
        auto stmt = parse_statement();
        if (!stmt) {
            error("expected statement in lambda body");
            return nullptr;
        }
        body->block_items.push_back(stmt);
    }
    expect(TokenKind::RBrace);

    expr->lambda_body = body;
    return expr;
}

// --- Type name / abstract declarator ------------------------------------

std::shared_ptr<Type> Parser::parse_type_name() {
    StorageClass storage = StorageClass::None;
    Linkage linkage = Linkage::None;
    bool is_typedef = false, is_extern = false, is_static = false;
    bool is_const = false, is_volatile = false, is_restrict = false;
    bool is_inline = false, is_noreturn = false;
    bool is_noexcept = false, is_constexpr = false, has_alignas = false;

    auto type = parse_declaration_specifiers(storage, linkage, is_typedef,
                                             is_extern, is_static, is_const,
                                             is_volatile, is_restrict,
                                             is_inline, is_noreturn,
                                             is_noexcept, is_constexpr, has_alignas);
    return type;
}

std::shared_ptr<Declarator> Parser::parse_abstract_declarator() {
    auto decl = std::make_shared<Declarator>();
    if (consume(TokenKind::Star)) {
        decl->is_pointer = true;
    }
    if (consume(TokenKind::LParen)) {
        parse_abstract_declarator();
        expect(TokenKind::RParen);
    }
    while (consume(TokenKind::LBracket)) {
        expect(TokenKind::RBracket);
    }
    return decl;
}

// --- Initializers -----------------------------------------------------

std::shared_ptr<Initializer> Parser::parse_initializer() {
    if (consume(TokenKind::LBrace)) {
        auto init = std::make_shared<Initializer>();
        init->kind = Initializer::Kind::List;
        parse_initializer_list(init);
        expect(TokenKind::RBrace);
        return init;
    }
    auto init = std::make_shared<Initializer>();
    init->kind = Initializer::Kind::Expression;
    init->expr = parse_assignment_expression();
    return init;
}

std::shared_ptr<Expression> Parser::parse_initializer_expression() {
    return parse_assignment_expression();
}

void Parser::parse_initializer_list(std::shared_ptr<Initializer> init) {
    while (!at_eof() && current().kind != TokenKind::RBrace) {
        if (current().kind == TokenKind::Period ||
            current().kind == TokenKind::LBracket) {
            parse_designation(init);
        }
        auto sub = parse_initializer();
        if (sub) {
            init->subinitializers.push_back(sub);
        }
        if (!consume(TokenKind::Comma))
            break;
    }
}

void Parser::parse_designation(std::shared_ptr<Initializer> init) {
    (void)init;
    if (consume(TokenKind::Period)) {
        if (current().kind == TokenKind::Identifier)
            advance();
        expect(TokenKind::Eq);
    } else if (consume(TokenKind::LBracket)) {
        parse_assignment_expression();
        expect(TokenKind::RBracket);
        expect(TokenKind::Eq);
    }
}

// --- Helpers ----------------------------------------------------------

bool Parser::is_type_specifier_kind(TokenKind kind) const {
    switch (kind) {
        case TokenKind::KwVoid:
        case TokenKind::KwBool:
        case TokenKind::KwChar:
        case TokenKind::KwShort:
        case TokenKind::KwInt:
        case TokenKind::KwLong:
        case TokenKind::KwFloat:
        case TokenKind::KwDouble:
        case TokenKind::KwStruct:
        case TokenKind::KwUnion:
        case TokenKind::KwEnum:
        case TokenKind::KwClass:
        case TokenKind::KwAuto:
        case TokenKind::KwDecltype:
            return true;
        case TokenKind::KwUnsigned:
            return true;
        default:
            return false;
    }
}

bool Parser::is_declaration_specifier_kind(TokenKind kind) const {
    return is_storage_class_kind(kind) || is_type_specifier_kind(kind) ||
           is_type_qualifier_kind(kind) || is_function_specifier_kind(kind);
}

bool Parser::is_storage_class_kind(TokenKind kind) const {
    switch (kind) {
        case TokenKind::KwTypedef:
        case TokenKind::KwExtern:
        case TokenKind::KwStatic:
        case TokenKind::KwAuto:
        case TokenKind::KwRegister:
        case TokenKind::KwThreadLocal:
            return true;
        default:
            return false;
    }
}

bool Parser::is_type_qualifier_kind(TokenKind kind) const {
    switch (kind) {
        case TokenKind::KwConst:
        case TokenKind::KwVolatile:
        case TokenKind::KwRestrict:
            return true;
        default:
            return false;
    }
}

bool Parser::is_function_specifier_kind(TokenKind kind) const {
    switch (kind) {
        case TokenKind::KwInline:
        case TokenKind::KwNoreturn:
        case TokenKind::KwNoexcept:
        case TokenKind::KwConstexpr:
            return true;
        default:
            return false;
    }
}

// M10: fold expression operator recognition.
bool Parser::is_fold_op(TokenKind kind) const {
    switch (kind) {
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent:
        case TokenKind::LessLess:
        case TokenKind::GreaterGreater:
        case TokenKind::Amp:
        case TokenKind::Pipe:
        case TokenKind::Caret:
        case TokenKind::AmpAmp:
        case TokenKind::PipePipe:
        case TokenKind::EqEq:
        case TokenKind::ExclaimEq:
        case TokenKind::Less:
        case TokenKind::Greater:
        case TokenKind::LessEq:
        case TokenKind::GreaterEq:
        case TokenKind::Comma:
        case TokenKind::Eq:
        case TokenKind::PlusEq:
        case TokenKind::MinusEq:
        case TokenKind::StarEq:
        case TokenKind::SlashEq:
        case TokenKind::PercentEq:
        case TokenKind::LessLessEq:
        case TokenKind::GreaterGreaterEq:
        case TokenKind::AmpEq:
        case TokenKind::CaretEq:
        case TokenKind::PipeEq:
            return true;
        default:
            return false;
    }
}

FoldOp Parser::parse_fold_op(TokenKind kind) const {
    switch (kind) {
        case TokenKind::Plus: return FoldOp::Plus;
        case TokenKind::Minus: return FoldOp::Minus;
        case TokenKind::Star: return FoldOp::Mul;
        case TokenKind::Slash: return FoldOp::Div;
        case TokenKind::Percent: return FoldOp::Rem;
        case TokenKind::LessLess: return FoldOp::Shl;
        case TokenKind::GreaterGreater: return FoldOp::Shr;
        case TokenKind::Amp: return FoldOp::BitAnd;
        case TokenKind::Pipe: return FoldOp::BitOr;
        case TokenKind::Caret: return FoldOp::BitXor;
        case TokenKind::AmpAmp: return FoldOp::LogicalAnd;
        case TokenKind::PipePipe: return FoldOp::LogicalOr;
        case TokenKind::Comma: return FoldOp::Comma;
        case TokenKind::Eq: return FoldOp::Assign;
        case TokenKind::PlusEq: return FoldOp::AddAssign;
        case TokenKind::MinusEq: return FoldOp::SubAssign;
        case TokenKind::StarEq: return FoldOp::MulAssign;
        case TokenKind::SlashEq: return FoldOp::DivAssign;
        case TokenKind::PercentEq: return FoldOp::RemAssign;
        case TokenKind::LessLessEq: return FoldOp::ShlAssign;
        case TokenKind::GreaterGreaterEq: return FoldOp::ShrAssign;
        case TokenKind::AmpEq: return FoldOp::AndAssign;
        case TokenKind::CaretEq: return FoldOp::XorAssign;
        case TokenKind::PipeEq: return FoldOp::OrAssign;
        case TokenKind::EqEq: return FoldOp::Eq;
        case TokenKind::ExclaimEq: return FoldOp::Ne;
        case TokenKind::Less: return FoldOp::Lt;
        case TokenKind::Greater: return FoldOp::Gt;
        case TokenKind::LessEq: return FoldOp::Le;
        case TokenKind::GreaterEq: return FoldOp::Ge;
        default: return FoldOp::Plus;
    }
}

} // namespace krc
