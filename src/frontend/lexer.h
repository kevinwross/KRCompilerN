#ifndef KRC_LEXER_H
#define KRC_LEXER_H

#include "source_manager.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace krc {

// Token kind. C11 keywords, literals, punctuators, and operators.
// Dialect-specific keywords (C++ only) are marked with a comment.
enum class TokenKind : std::uint16_t {
    // End of file / invalid
    Eof = 0,
    Invalid,

    // Identifiers
    Identifier,

    // Literals
    IntegerLiteral,
    FloatingLiteral,
    CharLiteral,
    StringLiteral,

    // C11 keywords
    KwAuto,
    KwBreak,
    KwCase,
    KwChar,
    KwConst,
    KwContinue,
    KwDefault,
    KwDo,
    KwDouble,
    KwElse,
    KwEnum,
    KwExtern,
    KwFloat,
    KwFor,
    KwGoto,
    KwIf,
    KwInline,
    KwInt,
    KwLong,
    KwRegister,
    KwRestrict,
    KwReturn,
    KwShort,
    KwSigned,
    KwSizeof,
    KwStatic,
    KwStruct,
    KwSwitch,
    KwTypedef,
    KwUnion,
    KwUnsigned,
    KwVoid,
    KwVolatile,
    KwWhile,

    // C11 standard keywords (underscore-capital)
    KwAlignas,
    KwAlignof,
    KwAtomic,
    KwBool,
    KwComplex,
    KwGeneric,
    KwImaginary,
    KwNoreturn,
    KwStaticAssert,
    KwThreadLocal,

    // C++ keywords (dialect-specific; accepted as identifiers in C mode)
    KwAsm,
    KwBoolCpp,
    KwCatch,
    KwClass,
    KwConstCast,
    KwDelete,
    KwDynamicCast,
    KwExplicit,
    KwExport,
    KwFalse,
    KwFriend,
    KwMutable,
    KwNamespace,
    KwNew,
    KwOperator,
    KwPrivate,
    KwProtected,
    KwPublic,
    KwReinterpretCast,
    KwStaticCast,
    KwTemplate,
    KwThis,
    KwThrow,
    KwTrue,
    KwTry,
    KwTypeid,
    KwTypename,
    KwVirtual,
    KwWcharT,

    // C++11 keywords
    KwAlignofCpp,
    KwConstexpr,
    KwDecltype,
    KwMove,
    KwNoexcept,
    KwNullptr,
    KwStaticAssertCpp,
    KwThreadLocalCpp,

    // Punctuators and operators
    LParen,       // (
    RParen,       // )
    LBracket,     // [
    RBracket,     // ]
    LBrace,       // {
    RBrace,       // }
    Period,       // .
    Arrow,        // ->
    Amp,          // &
    AmpAmp,       // &&
    AmpEq,        // &=
    Star,         // *
    StarEq,       // *=
    Plus,         // +
    PlusPlus,     // ++
    PlusEq,       // +=
    Minus,        // -
    MinusMinus,   // --
    MinusEq,      // -=
    Tilde,        // ~
    Exclaim,      // !
    ExclaimEq,    // !=
    Percent,      // %
    PercentEq,    // %=
    Caret,        // ^
    CaretEq,      // ^=
    Pipe,         // |
    PipePipe,     // ||
    PipeEq,       // |=
    Eq,           // =
    EqEq,         // ==
    Less,         // <
    LessLess,     // <<
    LessLessEq,   // <<=
    LessEq,       // <=
    Greater,      // >
    GreaterGreater,  // >>
    GreaterGreaterEq, // >>=
    GreaterEq,    // >=
    Slash,        // /
    SlashEq,      // /=
    Colon,        // :
    ColonColon,   // ::
    Semicolon,    // ;
    Question,     // ?
    Comma,        // ,
    Ellipsis,     // ...
    Hash,         // #
    HashHash,     // ##
    PercentColon, // %:  (alternative token for #)
    PercentColonColon, // %:%: (alternative token for ##)

    // Alternative tokens (C++ / C99 <iso646.h>)
    KwAnd,
    KwAndEq,
    KwBitAnd,
    KwBitOr,
    KwCompl,
    KwNot,
    KwNotEq,
    KwOr,
    KwOrEq,
    KwXor,
    KwXorEq,

    Last = KwXorEq,
};

// A single token produced by the lexer.
struct Token {
    TokenKind kind = TokenKind::Invalid;
    SourceLocation location;  // start of the token
    std::string_view text;    // spelling, points into the source buffer
    std::uint64_t length = 0; // byte length of the token text
};

// Result of lexing one token: the token, or a diagnostic on failure.
struct LexResult {
    Token token;
    bool ok = true;
};

// Lexer over a single source buffer.
class Lexer {
public:
    Lexer(const SourceManager& source_manager, int file_id);

    // Lex the next token. Returns ok=false on unterminated string/char
    // literal, invalid escape, or illegal character.
    LexResult next();

    // Peek at the next token without consuming it.
    const Token& peek() const { return current_; }

    // Return the current (last-returned) token.
    const Token& current() const { return current_; }

    // True if the last token returned was EOF.
    bool at_eof() const { return current_.kind == TokenKind::Eof; }

    // Adjust the current line number (used by #line directive).
    void set_line(std::uint32_t line) { line_ = line; }

private:
    const SourceManager& source_manager_;
    const SourceBuffer* buffer_;
    int file_id_;             // file id for source locations
    std::string_view contents_;
    std::size_t pos_ = 0;        // current scan position
    std::size_t line_start_ = 0; // byte offset of current line start
    std::uint32_t line_ = 1;     // current line number
    Token current_;

    // Helpers
    char peek_char(std::size_t offset = 0) const noexcept;
    char next_char();
    void skip_whitespace_and_comments();
    bool match(char expected);
    LexResult lex_identifier_or_keyword(std::size_t start);
    LexResult lex_number(std::size_t start, char first);
    LexResult lex_char_literal();
    LexResult lex_string_literal();
    LexResult lex_punctuator();
    LexResult lex_error(std::string_view message);
    static std::uint32_t line_of(std::string_view contents, std::size_t offset);
};

// Keyword lookup: returns the keyword kind for a given spelling, or
// TokenKind::Identifier if it is not a keyword.
TokenKind keyword_kind(std::string_view spelling);

// Returns true if the kind is a C11 keyword.
bool is_c11_keyword(TokenKind kind);

// Returns true if the kind is a C++-only keyword (an identifier in C mode).
bool is_cpp_keyword(TokenKind kind);

} // namespace krc

#endif // KRC_LEXER_H
