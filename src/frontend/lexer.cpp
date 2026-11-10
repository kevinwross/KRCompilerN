#include "lexer.h"

#include <cctype>
#include <cstring>

namespace krc {

namespace {

bool is_ident_start(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool is_ident_cont(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

bool is_digit(char ch) {
    return std::isdigit(static_cast<unsigned char>(ch));
}

bool is_hex_digit(char ch) {
    return std::isxdigit(static_cast<unsigned char>(ch));
}

TokenKind punctuator_kind(std::string_view text) {
    static const struct { const char* text; TokenKind kind; } table[] = {
        {"<<=", TokenKind::LessLessEq},
        {">>=", TokenKind::GreaterGreaterEq},
        {"&&", TokenKind::AmpAmp},
        {"||", TokenKind::PipePipe},
        {"<=", TokenKind::LessEq},
        {">=", TokenKind::GreaterEq},
        {"==", TokenKind::EqEq},
        {"!=", TokenKind::ExclaimEq},
        {"->", TokenKind::Arrow},
        {"++", TokenKind::PlusPlus},
        {"--", TokenKind::MinusMinus},
        {"+=", TokenKind::PlusEq},
        {"-=", TokenKind::MinusEq},
        {"*=", TokenKind::StarEq},
        {"/=", TokenKind::SlashEq},
        {"%=", TokenKind::PercentEq},
        {"^=", TokenKind::CaretEq},
        {"&=", TokenKind::AmpEq},
        {"|=", TokenKind::PipeEq},
        {"<<", TokenKind::LessLess},
        {">>", TokenKind::GreaterGreater},
        {"::", TokenKind::ColonColon},
        {"..", TokenKind::Invalid},
        {".*", TokenKind::Invalid},
        {"%", TokenKind::Percent},
        {"^", TokenKind::Caret},
        {"&", TokenKind::Amp},
        {"|", TokenKind::Pipe},
        {"+", TokenKind::Plus},
        {"-", TokenKind::Minus},
        {"*", TokenKind::Star},
        {"/", TokenKind::Slash},
        {"<", TokenKind::Less},
        {">", TokenKind::Greater},
        {"=", TokenKind::Eq},
        {"!", TokenKind::Exclaim},
        {"~", TokenKind::Tilde},
        {"?", TokenKind::Question},
        {":", TokenKind::Colon},
        {";", TokenKind::Semicolon},
        {",", TokenKind::Comma},
        {".", TokenKind::Period},
        {"#", TokenKind::Hash},
        {"##", TokenKind::HashHash},
        {"%:", TokenKind::PercentColon},
        {"%:%:", TokenKind::PercentColonColon},
        {"(", TokenKind::LParen},
        {")", TokenKind::RParen},
        {"[", TokenKind::LBracket},
        {"]", TokenKind::RBracket},
        {"{", TokenKind::LBrace},
        {"}", TokenKind::RBrace},
        {"...", TokenKind::Ellipsis},
    };
    for (const auto& entry : table) {
        if (text == entry.text) {
            return entry.kind;
        }
    }
    return TokenKind::Invalid;
}

} // namespace

TokenKind keyword_kind(std::string_view spelling) {
    static const struct { const char* text; TokenKind kind; } table[] = {
        {"auto", TokenKind::KwAuto},
        {"break", TokenKind::KwBreak},
        {"case", TokenKind::KwCase},
        {"char", TokenKind::KwChar},
        {"const", TokenKind::KwConst},
        {"continue", TokenKind::KwContinue},
        {"default", TokenKind::KwDefault},
        {"do", TokenKind::KwDo},
        {"double", TokenKind::KwDouble},
        {"else", TokenKind::KwElse},
        {"enum", TokenKind::KwEnum},
        {"extern", TokenKind::KwExtern},
        {"float", TokenKind::KwFloat},
        {"for", TokenKind::KwFor},
        {"goto", TokenKind::KwGoto},
        {"if", TokenKind::KwIf},
        {"inline", TokenKind::KwInline},
        {"int", TokenKind::KwInt},
        {"long", TokenKind::KwLong},
        {"register", TokenKind::KwRegister},
        {"restrict", TokenKind::KwRestrict},
        {"return", TokenKind::KwReturn},
        {"short", TokenKind::KwShort},
        {"signed", TokenKind::KwSigned},
        {"sizeof", TokenKind::KwSizeof},
        {"static", TokenKind::KwStatic},
        {"struct", TokenKind::KwStruct},
        {"switch", TokenKind::KwSwitch},
        {"typedef", TokenKind::KwTypedef},
        {"union", TokenKind::KwUnion},
        {"unsigned", TokenKind::KwUnsigned},
        {"void", TokenKind::KwVoid},
        {"volatile", TokenKind::KwVolatile},
        {"while", TokenKind::KwWhile},
        {"_Alignas", TokenKind::KwAlignas},
        {"_Alignof", TokenKind::KwAlignof},
        {"_Atomic", TokenKind::KwAtomic},
        {"_Bool", TokenKind::KwBool},
        {"_Complex", TokenKind::KwComplex},
        {"_Generic", TokenKind::KwGeneric},
        {"_Imaginary", TokenKind::KwImaginary},
        {"_Noreturn", TokenKind::KwNoreturn},
        {"_Static_assert", TokenKind::KwStaticAssert},
        {"_Thread_local", TokenKind::KwThreadLocal},
        {"asm", TokenKind::KwAsm},
        {"bool", TokenKind::KwBoolCpp},
        {"catch", TokenKind::KwCatch},
        {"class", TokenKind::KwClass},
        {"const_cast", TokenKind::KwConstCast},
        {"delete", TokenKind::KwDelete},
        {"dynamic_cast", TokenKind::KwDynamicCast},
        {"explicit", TokenKind::KwExplicit},
        {"export", TokenKind::KwExport},
        {"false", TokenKind::KwFalse},
        {"friend", TokenKind::KwFriend},
        {"mutable", TokenKind::KwMutable},
        {"namespace", TokenKind::KwNamespace},
        {"new", TokenKind::KwNew},
        {"operator", TokenKind::KwOperator},
        {"private", TokenKind::KwPrivate},
        {"protected", TokenKind::KwProtected},
        {"public", TokenKind::KwPublic},
        {"reinterpret_cast", TokenKind::KwReinterpretCast},
        {"static_cast", TokenKind::KwStaticCast},
        {"template", TokenKind::KwTemplate},
        {"this", TokenKind::KwThis},
        {"throw", TokenKind::KwThrow},
        {"true", TokenKind::KwTrue},
        {"try", TokenKind::KwTry},
        {"typeid", TokenKind::KwTypeid},
        {"typename", TokenKind::KwTypename},
        {"virtual", TokenKind::KwVirtual},
        {"wchar_t", TokenKind::KwWcharT},
        {"alignof", TokenKind::KwAlignofCpp},
        {"constexpr", TokenKind::KwConstexpr},
        {"decltype", TokenKind::KwDecltype},
        {"noexcept", TokenKind::KwNoexcept},
        {"nullptr", TokenKind::KwNullptr},
        {"static_assert", TokenKind::KwStaticAssertCpp},
        {"thread_local", TokenKind::KwThreadLocalCpp},
        {"and", TokenKind::KwAnd},
        {"and_eq", TokenKind::KwAndEq},
        {"bitand", TokenKind::KwBitAnd},
        {"bitor", TokenKind::KwBitOr},
        {"compl", TokenKind::KwCompl},
        {"not", TokenKind::KwNot},
        {"not_eq", TokenKind::KwNotEq},
        {"or", TokenKind::KwOr},
        {"or_eq", TokenKind::KwOrEq},
        {"xor", TokenKind::KwXor},
        {"xor_eq", TokenKind::KwXorEq},
    };
    for (const auto& entry : table) {
        if (spelling == entry.text) {
            return entry.kind;
        }
    }
    return TokenKind::Identifier;
}

bool is_c11_keyword(TokenKind kind) {
    return (kind >= TokenKind::KwAuto && kind <= TokenKind::KwWhile) ||
           (kind >= TokenKind::KwAlignas && kind <= TokenKind::KwThreadLocal);
}

bool is_cpp_keyword(TokenKind kind) {
    return (kind >= TokenKind::KwAsm && kind <= TokenKind::KwWcharT) ||
           (kind >= TokenKind::KwAlignofCpp && kind <= TokenKind::KwThreadLocalCpp) ||
           (kind >= TokenKind::KwAnd && kind <= TokenKind::KwXorEq);
}

Lexer::Lexer(const SourceManager& source_manager, int file_id)
    : source_manager_(source_manager),
      buffer_(source_manager.get_buffer(file_id)),
      file_id_(file_id) {
    if (buffer_) {
        contents_ = buffer_->contents();
    } else {
        contents_ = "";
    }
}

char Lexer::peek_char(std::size_t offset) const noexcept {
    std::size_t index = pos_ + offset;
    if (index >= contents_.size()) {
        return '\0';
    }
    return contents_[index];
}

char Lexer::next_char() {
    if (pos_ >= contents_.size()) {
        return '\0';
    }
    char ch = contents_[pos_];
    ++pos_;
    if (ch == '\n') {
        ++line_;
        line_start_ = pos_;
    }
    return ch;
}

bool Lexer::match(char expected) {
    if (peek_char() == expected) {
        next_char();
        return true;
    }
    return false;
}

void Lexer::skip_whitespace_and_comments() {
    while (true) {
        while (peek_char() == ' ' || peek_char() == '\t' ||
               peek_char() == '\r' || peek_char() == '\n' ||
               peek_char() == '\v' || peek_char() == '\f') {
            next_char();
        }
        if (peek_char() == '/' && peek_char(1) == '/') {
            next_char();
            next_char();
            while (peek_char() != '\n' && peek_char() != '\0') {
                next_char();
            }
            continue;
        }
        if (peek_char() == '/' && peek_char(1) == '*') {
            next_char();
            next_char();
            while (peek_char() != '\0') {
                if (peek_char() == '*' && peek_char(1) == '/') {
                    next_char();
                    next_char();
                    break;
                }
                next_char();
            }
            continue;
        }
        break;
    }
}

LexResult Lexer::lex_error(std::string_view message) {
    LexResult result;
    result.ok = false;
    result.token.kind = TokenKind::Invalid;
    result.token.location = source_manager_.make_location(
        file_id_, /*offset=*/pos_);
    result.token.text = message;
    result.token.length = message.size();
    return result;
}

LexResult Lexer::lex_identifier_or_keyword(std::size_t start) {
    while (is_ident_cont(peek_char())) {
        next_char();
    }
    std::string_view spelling(contents_.data() + start, pos_ - start);
    TokenKind kind = keyword_kind(spelling);
    Token token;
    token.kind = kind;
    token.location = source_manager_.make_location(
        file_id_, /*offset=*/start);
    token.text = spelling;
    token.length = pos_ - start;
    LexResult result;
    result.token = token;
    result.ok = true;
    return result;
}

LexResult Lexer::lex_number(std::size_t start, char first) {
    bool is_hex = false;
    bool is_octal = false;

    if (first == '0' && (peek_char() == 'x' || peek_char() == 'X')) {
        is_hex = true;
        next_char();
    } else if (first == '0' && (peek_char() == 'b' || peek_char() == 'B')) {
        next_char();
    } else if (first == '0' && is_digit(peek_char())) {
        is_octal = true;
    }

    while (true) {
        char ch = peek_char();
        if (is_hex) {
            if (!is_hex_digit(ch)) break;
        } else if (is_octal) {
            if (ch < '0' || ch > '7') break;
        } else {
            if (!is_digit(ch)) break;
        }
        next_char();
    }

    bool is_float = false;
    if (!is_hex && !is_octal && peek_char() == '.' && is_digit(peek_char(1))) {
        is_float = true;
        next_char();
        while (is_digit(peek_char())) {
            next_char();
        }
    }
    if (!is_hex && (peek_char() == 'e' || peek_char() == 'E')) {
        is_float = true;
        next_char();
        if (peek_char() == '+' || peek_char() == '-') {
            next_char();
        }
        while (is_digit(peek_char())) {
            next_char();
        }
    }
    if (is_hex && (peek_char() == 'p' || peek_char() == 'P')) {
        is_float = true;
        next_char();
        if (peek_char() == '+' || peek_char() == '-') {
            next_char();
        }
        while (is_digit(peek_char())) {
            next_char();
        }
    }

    while (true) {
        char ch = peek_char();
        if (ch == 'u' || ch == 'U' || ch == 'l' || ch == 'L' ||
            ch == 'f' || ch == 'F') {
            next_char();
        } else {
            break;
        }
    }

    std::string_view spelling(contents_.data() + start, pos_ - start);
    Token token;
    token.kind = is_float ? TokenKind::FloatingLiteral
                          : TokenKind::IntegerLiteral;
    token.location = source_manager_.make_location(file_id_, start);
    token.text = spelling;
    token.length = pos_ - start;
    LexResult result;
    result.token = token;
    result.ok = true;
    return result;
}

LexResult Lexer::lex_char_literal() {
    std::size_t start = pos_ - 1;
    while (true) {
        char ch = peek_char();
        if (ch == '\0') {
            return lex_error("unterminated character literal");
        }
        if (ch == '\n') {
            return lex_error("unterminated character literal");
        }
        next_char();
        if (ch == '\\') {
            if (peek_char() == '\0') {
                return lex_error("unterminated escape in character literal");
            }
            next_char();
            continue;
        }
        if (ch == '\'') {
            break;
        }
    }
    std::string_view spelling(contents_.data() + start, pos_ - start);
    Token token;
    token.kind = TokenKind::CharLiteral;
    token.location = source_manager_.make_location(file_id_, start);
    token.text = spelling;
    token.length = pos_ - start;
    LexResult result;
    result.token = token;
    result.ok = true;
    return result;
}

LexResult Lexer::lex_string_literal() {
    std::size_t start = pos_ - 1;
    while (true) {
        char ch = peek_char();
        if (ch == '\0') {
            return lex_error("unterminated string literal");
        }
        if (ch == '\n') {
            return lex_error("unterminated string literal");
        }
        next_char();
        if (ch == '\\') {
            if (peek_char() == '\0') {
                return lex_error("unterminated escape in string literal");
            }
            next_char();
            continue;
        }
        if (ch == '"') {
            break;
        }
    }
    std::string_view spelling(contents_.data() + start, pos_ - start);
    Token token;
    token.kind = TokenKind::StringLiteral;
    token.location = source_manager_.make_location(file_id_, start);
    token.text = spelling;
    token.length = pos_ - start;
    LexResult result;
    result.token = token;
    result.ok = true;
    return result;
}

LexResult Lexer::lex_punctuator() {
    std::size_t max_len = 0;
    TokenKind best = TokenKind::Invalid;
    for (std::size_t len = 3; len >= 1; --len) {
        if (pos_ + len > contents_.size()) continue;
        std::string_view text(contents_.data() + pos_, len);
        TokenKind kind = punctuator_kind(text);
        if (kind != TokenKind::Invalid) {
            max_len = len;
            best = kind;
            break;
        }
    }
    if (max_len == 0) {
        char ch = peek_char();
        return lex_error(std::string("unknown character: ") + ch);
    }
    std::string_view spelling(contents_.data() + pos_, max_len);
    pos_ += max_len;
    Token token;
    token.kind = best;
    token.location = source_manager_.make_location(
        file_id_, /*offset=*/pos_ - max_len);
    token.text = spelling;
    token.length = max_len;
    LexResult result;
    result.token = token;
    result.ok = true;
    return result;
}

LexResult Lexer::next() {
    skip_whitespace_and_comments();

    std::size_t start = pos_;
    char ch = peek_char();
    if (ch == '\0') {
        Token token;
        token.kind = TokenKind::Eof;
        token.location = source_manager_.make_location(
            file_id_, /*offset=*/pos_);
        token.text = "";
        token.length = 0;
        current_ = token;
        LexResult result;
        result.token = token;
        result.ok = true;
        return result;
    }

    next_char();

    if (is_ident_start(ch)) {
        auto result = lex_identifier_or_keyword(start);
        current_ = result.token;
        return result;
    }
    if (is_digit(ch)) {
        auto result = lex_number(start, ch);
        current_ = result.token;
        return result;
    }
    if (ch == '\'') {
        auto result = lex_char_literal();
        current_ = result.token;
        return result;
    }
    if (ch == '"') {
        auto result = lex_string_literal();
        current_ = result.token;
        return result;
    }
    pos_ = start;
    auto result = lex_punctuator();
    current_ = result.token;
    return result;
}

std::uint32_t Lexer::line_of(std::string_view contents, std::size_t offset) {
    std::uint32_t line = 1;
    for (std::size_t i = 0; i < offset && i < contents.size(); ++i) {
        if (contents[i] == '\n') {
            ++line;
        }
    }
    return line;
}

} // namespace krc
