// kevinc.cpp — a single-translation-unit C89 compiler for the KR32 virtual CPU.
//
// Purpose
// -------
// This file is a deliberately broad C++17 program used as a parser stress
// fixture for KRCompilerN. It is a complete pipeline in miniature:
//
//     source -> lexer -> preprocessor -> C89 parser -> semantic analysis
//            -> KR32 assembly
//
// It is not part of the krc build and has no build-system integration. The
// emitted assembly is assembled for syntax validation and is not executed. See
// plan.md for the supported C89 subset and the KR32 mapping.
//
// C++17 surface used on purpose: std::optional, std::variant with std::visit,
// std::string_view, std::filesystem, structured bindings, if constexpr, fold
// expressions, generic lambdas, [[nodiscard]], [[noreturn]], and the standard
// containers.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace kevinc {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Small utilities
// ---------------------------------------------------------------------------

template <typename... Args>
[[nodiscard]] std::string concat(Args&&... args) {
    std::ostringstream stream;
    (stream << ... << args);
    return stream.str();
}

template <typename T>
[[nodiscard]] constexpr T alignUp(T value, T alignment) noexcept {
    return (value + alignment - 1) / alignment * alignment;
}

[[nodiscard]] constexpr bool isAsciiDigit(char ch) noexcept {
    return ch >= '0' && ch <= '9';
}

[[nodiscard]] constexpr bool isAsciiOctalDigit(char ch) noexcept {
    return ch >= '0' && ch <= '7';
}

[[nodiscard]] constexpr bool isAsciiHexDigit(char ch) noexcept {
    return isAsciiDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

[[nodiscard]] constexpr bool isIdentifierStart(char ch) noexcept {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

[[nodiscard]] constexpr bool isIdentifierContinue(char ch) noexcept {
    return isIdentifierStart(ch) || isAsciiDigit(ch);
}

[[nodiscard]] constexpr int hexValue(char ch) noexcept {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] inline std::string_view trimView(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

// ---------------------------------------------------------------------------
// Source management and diagnostics
// ---------------------------------------------------------------------------

struct SourceLocation {
    std::uint32_t file = 0;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
};

struct SourceFile {
    fs::path path;
    std::string displayName;
    std::string text;
    std::vector<std::uint32_t> lineStarts;
};

class SourceManager {
public:
    [[nodiscard]] std::uint32_t addVirtualFile(std::string name, std::string text) {
        SourceFile file;
        file.displayName = std::move(name);
        file.text = std::move(text);
        buildLineStarts(file);
        files_.push_back(std::move(file));
        return static_cast<std::uint32_t>(files_.size() - 1);
    }

    [[nodiscard]] std::optional<std::uint32_t> addFile(const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            return std::nullopt;
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        SourceFile file;
        file.path = path;
        file.displayName = path.string();
        file.text = buffer.str();
        buildLineStarts(file);
        files_.push_back(std::move(file));
        return static_cast<std::uint32_t>(files_.size() - 1);
    }

    [[nodiscard]] const SourceFile& file(std::uint32_t index) const {
        return files_.at(index);
    }

    [[nodiscard]] std::string displayName(std::uint32_t index) const {
        if (index >= files_.size()) {
            return "<unknown>";
        }
        return files_[index].displayName;
    }

    [[nodiscard]] std::string_view lineText(SourceLocation location) const {
        if (location.file >= files_.size() || location.line == 0) {
            return {};
        }
        const SourceFile& source = files_[location.file];
        if (location.line > source.lineStarts.size()) {
            return {};
        }
        const std::size_t begin = source.lineStarts[location.line - 1];
        const std::size_t end = location.line < source.lineStarts.size()
                                    ? source.lineStarts[location.line]
                                    : source.text.size();
        std::size_t trimmed = end;
        while (trimmed > begin && (source.text[trimmed - 1] == '\n' || source.text[trimmed - 1] == '\r')) {
            --trimmed;
        }
        return std::string_view(source.text).substr(begin, trimmed - begin);
    }

private:
    static void buildLineStarts(SourceFile& file) {
        file.lineStarts.clear();
        file.lineStarts.push_back(0);
        for (std::size_t index = 0; index < file.text.size(); ++index) {
            if (file.text[index] == '\n') {
                file.lineStarts.push_back(static_cast<std::uint32_t>(index + 1));
            }
        }
    }

    std::vector<SourceFile> files_;
};

enum class Severity {
    Note,
    Warning,
    Error
};

[[nodiscard]] constexpr std::string_view severityName(Severity severity) noexcept {
    switch (severity) {
    case Severity::Note:
        return "note";
    case Severity::Warning:
        return "warning";
    case Severity::Error:
        return "error";
    }
    return "diagnostic";
}

class Diagnostics {
public:
    Diagnostics(SourceManager& sources, std::ostream& stream)
        : sources_(sources), stream_(stream) {}

    void report(Severity severity, SourceLocation location, std::string_view message) {
        switch (severity) {
        case Severity::Note:
            ++notes_;
            break;
        case Severity::Warning:
            ++warnings_;
            break;
        case Severity::Error:
            ++errors_;
            break;
        }

        stream_ << sources_.displayName(location.file) << ':' << location.line << ':'
                << location.column << ": " << severityName(severity) << ": " << message << '\n';

        const std::string_view line = sources_.lineText(location);
        if (!line.empty()) {
            stream_ << "    " << line << '\n';
            const std::size_t caret = location.column > 0
                                          ? static_cast<std::size_t>(location.column - 1)
                                          : 0U;
            stream_ << "    " << std::string(caret, ' ') << "^\n";
        }
        stream_.flush();
    }

    void note(SourceLocation location, std::string_view message) {
        report(Severity::Note, location, message);
    }

    void warning(SourceLocation location, std::string_view message) {
        report(Severity::Warning, location, message);
    }

    void error(SourceLocation location, std::string_view message) {
        report(Severity::Error, location, message);
    }

    [[nodiscard]] int errorCount() const noexcept { return errors_; }
    [[nodiscard]] int warningCount() const noexcept { return warnings_; }
    [[nodiscard]] int noteCount() const noexcept { return notes_; }
    [[nodiscard]] bool hadError() const noexcept { return errors_ != 0; }

    [[noreturn]] void fatal(SourceLocation location, std::string_view message) {
        error(location, message);
        throw std::runtime_error(std::string(message));
    }

private:
    SourceManager& sources_;
    std::ostream& stream_;
    int notes_ = 0;
    int warnings_ = 0;
    int errors_ = 0;
};

// ---------------------------------------------------------------------------
// Tokens and lexer
// ---------------------------------------------------------------------------

enum class TokenKind {
    End,
    Newline,
    Identifier,
    Number,
    CharLiteral,
    StringLiteral,
    Punctuator
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    SourceLocation location;
    bool beginningOfLine = false;
    bool precededBySpace = false;

    [[nodiscard]] bool isPunct(std::string_view punctuator) const noexcept {
        return kind == TokenKind::Punctuator && text == punctuator;
    }

    [[nodiscard]] bool isIdentifier(std::string_view name) const noexcept {
        return kind == TokenKind::Identifier && text == name;
    }
};

class Lexer {
public:
    Lexer(std::uint32_t fileIndex, const SourceFile& file, Diagnostics& diagnostics)
        : fileIndex_(fileIndex), diagnostics_(diagnostics) {
        normalize(file.text);
        if (locations_.empty()) {
            locations_.push_back(SourceLocation{fileIndex_, 1, 1});
        }
    }

    [[nodiscard]] Token next() {
        for (;;) {
            if (position_ >= text_.size()) {
                return Token{TokenKind::End, std::string{}, locationAt(position_), true, true};
            }

            const char ch = text_[position_];
            if (ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r') {
                ++position_;
                spaced_ = true;
                continue;
            }
            if (ch == '\n') {
                const SourceLocation location = locationAt(position_);
                ++position_;
                beginningOfLine_ = true;
                spaced_ = true;
                return Token{TokenKind::Newline, "\n", location, true, true};
            }
            if (ch == '/' && peek(1) == '/') {
                position_ += 2;
                while (position_ < text_.size() && text_[position_] != '\n') {
                    ++position_;
                }
                spaced_ = true;
                continue;
            }
            if (ch == '/' && peek(1) == '*') {
                const SourceLocation start = locationAt(position_);
                position_ += 2;
                bool closed = false;
                while (position_ + 1 < text_.size()) {
                    if (text_[position_] == '*' && text_[position_ + 1] == '/') {
                        position_ += 2;
                        closed = true;
                        break;
                    }
                    ++position_;
                }
                if (!closed) {
                    diagnostics_.error(start, "unterminated block comment");
                } else {
                    spaced_ = true;
                    continue;
                }
            }
            break;
        }

        const SourceLocation location = locationAt(position_);
        const bool beginning = beginningOfLine_;
        const bool spaced = spaced_;
        beginningOfLine_ = false;
        spaced_ = false;

        const char ch = text_[position_];

        if (isIdentifierStart(ch)) {
            // Wide character/string literals: the C89 spelling only.
            if ((ch == 'L') && (peek(1) == '\'' || peek(1) == '"')) {
                ++position_;
                return scanQuoted(peek(0), location, beginning, spaced);
            }
            const std::size_t begin = position_;
            ++position_;
            while (position_ < text_.size() && isIdentifierContinue(text_[position_])) {
                ++position_;
            }
            return Token{TokenKind::Identifier, text_.substr(begin, position_ - begin), location,
                         beginning, spaced};
        }

        if (isAsciiDigit(ch) || (ch == '.' && isAsciiDigit(peek(1)))) {
            return scanNumber(location, beginning, spaced);
        }

        if (ch == '\'' || ch == '"') {
            return scanQuoted(ch, location, beginning, spaced);
        }

        return scanPunctuator(location, beginning, spaced);
    }

private:
    static constexpr std::array<std::string_view, 48> kPunctuators = {
        ">>=", "<<=", "...", "->", "++", "--", "<<", ">>", "<=", ">=",
        "==",  "!=",  "&&",  "||", "+=", "-=", "*=", "/=", "%=", "&=",
        "|=",  "^=",  "##",  "[",  "]",  "(",  ")",  "{",  "}",  ".",
        "&",   "*",   "+",   "-",  "~",  "!",  "/",  "%",  "<",  ">",
        "^",   "|",   "?",   ":",  ";",  "=",  ",",  "#"
    };

    void normalize(std::string_view source) {
        std::uint32_t line = 1;
        std::uint32_t column = 1;
        std::size_t index = 0;
        while (index < source.size()) {
            const char ch = source[index];
            if (ch == '\\' && index + 1 < source.size() &&
                (source[index + 1] == '\n' || source[index + 1] == '\r')) {
                if (source[index + 1] == '\r' && index + 2 < source.size() &&
                    source[index + 2] == '\n') {
                    index += 3;
                } else {
                    index += 2;
                }
                ++line;
                column = 1;
                continue;
            }
            if (ch == '\r') {
                if (index + 1 < source.size() && source[index + 1] == '\n') {
                    ++index;
                }
                text_.push_back('\n');
                locations_.push_back(SourceLocation{fileIndex_, line, column});
                ++index;
                ++line;
                column = 1;
                continue;
            }
            text_.push_back(ch);
            locations_.push_back(SourceLocation{fileIndex_, line, column});
            ++index;
            if (ch == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        locations_.push_back(SourceLocation{fileIndex_, line, column});
    }

    [[nodiscard]] char peek(std::size_t ahead) const noexcept {
        const std::size_t at = position_ + ahead;
        return at < text_.size() ? text_[at] : '\0';
    }

    [[nodiscard]] SourceLocation locationAt(std::size_t index) const noexcept {
        if (index >= locations_.size()) {
            return locations_.empty() ? SourceLocation{fileIndex_, 1, 1} : locations_.back();
        }
        return locations_[index];
    }

    [[nodiscard]] Token scanNumber(SourceLocation location, bool beginning, bool spaced) {
        const std::size_t begin = position_;
        bool previousWasExponent = false;
        while (position_ < text_.size()) {
            const char ch = text_[position_];
            if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '.') {
                previousWasExponent = ch == 'e' || ch == 'E' || ch == 'p' || ch == 'P';
                ++position_;
                continue;
            }
            if ((ch == '+' || ch == '-') && previousWasExponent) {
                previousWasExponent = false;
                ++position_;
                continue;
            }
            break;
        }
        return Token{TokenKind::Number, text_.substr(begin, position_ - begin), location, beginning,
                     spaced};
    }

    [[nodiscard]] Token scanQuoted(char quote, SourceLocation location, bool beginning,
                                   bool spaced) {
        const std::size_t begin = position_ - 0;
        // The opening quote is the current character for normal literals; for a
        // wide literal the caller has already consumed the 'L' prefix.
        if (position_ < text_.size() && text_[position_] == quote) {
            ++position_;
        }
        bool closed = false;
        while (position_ < text_.size()) {
            const char ch = text_[position_];
            if (ch == '\\') {
                position_ += 2;
                continue;
            }
            if (ch == quote) {
                ++position_;
                closed = true;
                break;
            }
            if (ch == '\n') {
                break;
            }
            ++position_;
        }
        if (!closed) {
            diagnostics_.error(location, quote == '"' ? "unterminated string literal"
                                                      : "unterminated character constant");
        }
        return Token{quote == '"' ? TokenKind::StringLiteral : TokenKind::CharLiteral,
                     text_.substr(begin, position_ - begin), location, beginning, spaced};
    }

    [[nodiscard]] Token scanPunctuator(SourceLocation location, bool beginning, bool spaced) {
        for (const std::string_view punctuator : kPunctuators) {
            if (punctuator.size() <= text_.size() - position_ &&
                std::string_view(text_).substr(position_, punctuator.size()) == punctuator) {
                position_ += punctuator.size();
                return Token{TokenKind::Punctuator, std::string(punctuator), location, beginning,
                             spaced};
            }
        }
        const char ch = text_[position_];
        ++position_;
        diagnostics_.error(location, concat("unexpected character '", ch, "'"));
        return Token{TokenKind::Punctuator, std::string(1, ch), location, beginning, spaced};
    }

    std::uint32_t fileIndex_ = 0;
    Diagnostics& diagnostics_;
    std::string text_;
    std::vector<SourceLocation> locations_;
    std::size_t position_ = 0;
    bool beginningOfLine_ = true;
    bool spaced_ = true;
};

// ---------------------------------------------------------------------------
// Literal decoding (shared by the preprocessor and the parser)
// ---------------------------------------------------------------------------

namespace literals {

[[nodiscard]] inline std::optional<std::uint32_t> decodeEscape(std::string_view body,
                                                               std::size_t& index) {
    if (index >= body.size() || body[index] != '\\') {
        return std::nullopt;
    }
    ++index;
    if (index >= body.size()) {
        return std::nullopt;
    }
    const char ch = body[index++];
    switch (ch) {
    case 'a':
        return 0x07U;
    case 'b':
        return 0x08U;
    case 'f':
        return 0x0CU;
    case 'n':
        return 0x0AU;
    case 'r':
        return 0x0DU;
    case 't':
        return 0x09U;
    case 'v':
        return 0x0BU;
    case '\\':
    case '\'':
    case '\"':
    case '?':
        return static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
    case 'x': {
        std::uint32_t value = 0;
        bool any = false;
        while (index < body.size()) {
            const int digit = hexValue(body[index]);
            if (digit < 0) {
                break;
            }
            value = value * 16U + static_cast<std::uint32_t>(digit);
            any = true;
            ++index;
        }
        return any ? std::optional<std::uint32_t>(value) : std::nullopt;
    }
    default:
        break;
    }
    if (isAsciiOctalDigit(ch)) {
        std::uint32_t value = static_cast<std::uint32_t>(ch - '0');
        int count = 1;
        while (count < 3 && index < body.size() && isAsciiOctalDigit(body[index])) {
            value = value * 8U + static_cast<std::uint32_t>(body[index] - '0');
            ++index;
            ++count;
        }
        return value;
    }
    // Unknown escapes are preserved as the escaped character, a common
    // extension that keeps otherwise-valid fixtures compiling.
    return static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
}

[[nodiscard]] inline std::optional<std::int64_t> charValue(const Token& token,
                                                           Diagnostics& diagnostics) {
    std::string_view text = token.text;
    if (text.size() >= 2 && text.front() == 'L') {
        text.remove_prefix(1);
    }
    if (text.size() < 2 || text.front() != '\'' || text.back() != '\'') {
        diagnostics.error(token.location, "malformed character constant");
        return std::nullopt;
    }
    const std::string_view body = text.substr(1, text.size() - 2);
    if (body.empty()) {
        diagnostics.error(token.location, "empty character constant");
        return std::nullopt;
    }
    std::uint32_t value = 0;
    std::size_t index = 0;
    bool any = false;
    while (index < body.size()) {
        std::uint32_t character = 0;
        if (body[index] == '\\') {
            const auto escaped = decodeEscape(body, index);
            if (!escaped.has_value()) {
                diagnostics.error(token.location, "invalid escape in character constant");
                return std::nullopt;
            }
            character = *escaped;
        } else {
            character = static_cast<std::uint32_t>(static_cast<unsigned char>(body[index]));
            ++index;
        }
        value = (value << 8U) | (character & 0xFFU);
        any = true;
    }
    if (!any) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(static_cast<std::int32_t>(value));
}

[[nodiscard]] inline std::optional<std::string> stringValue(const Token& token,
                                                            Diagnostics& diagnostics) {
    std::string_view text = token.text;
    if (text.size() >= 2 && text.front() == 'L') {
        text.remove_prefix(1);
    }
    if (text.size() < 2 || text.front() != '\"') {
        diagnostics.error(token.location, "malformed string literal");
        return std::nullopt;
    }
    std::size_t end = text.size();
    while (end > 1 && text[end - 1] != '\"') {
        --end;
    }
    if (end < 2) {
        diagnostics.error(token.location, "unterminated string literal");
        return std::nullopt;
    }
    const std::string_view body = text.substr(1, end - 2);
    std::string result;
    std::size_t index = 0;
    while (index < body.size()) {
        if (body[index] == '\\') {
            const auto escaped = decodeEscape(body, index);
            if (!escaped.has_value()) {
                diagnostics.error(token.location, "invalid escape in string literal");
                return std::nullopt;
            }
            result.push_back(static_cast<char>(*escaped & 0xFFU));
        } else {
            result.push_back(body[index]);
            ++index;
        }
    }
    return result;
}

} // namespace literals

// ---------------------------------------------------------------------------
// Preprocessor
// ---------------------------------------------------------------------------

struct Macro {
    std::string name;
    bool functionLike = false;
    bool variadic = false;
    std::vector<std::string> parameters;
    std::vector<Token> replacement;
    SourceLocation location;
};

[[nodiscard]] inline TokenKind classifyTokenText(std::string_view text) {
    if (text.empty()) {
        return TokenKind::Punctuator;
    }
    if (isIdentifierStart(text.front())) {
        bool identifier = true;
        for (const char ch : text) {
            if (!isIdentifierContinue(ch)) {
                identifier = false;
                break;
            }
        }
        if (identifier) {
            return TokenKind::Identifier;
        }
    }
    if (isAsciiDigit(text.front()) || (text.front() == '.' && text.size() > 1 &&
                                       isAsciiDigit(text[1]))) {
        return TokenKind::Number;
    }
    return TokenKind::Punctuator;
}

[[nodiscard]] inline Token makeToken(TokenKind kind, std::string text, SourceLocation location,
                                     bool beginningOfLine = false, bool precededBySpace = false) {
    Token token;
    token.kind = kind;
    token.text = std::move(text);
    token.location = location;
    token.beginningOfLine = beginningOfLine;
    token.precededBySpace = precededBySpace;
    return token;
}

[[nodiscard]] inline Token makeIdentifierToken(std::string text, SourceLocation location) {
    return makeToken(TokenKind::Identifier, std::move(text), location);
}

[[nodiscard]] inline Token makeNumberToken(std::string text, SourceLocation location) {
    return makeToken(TokenKind::Number, std::move(text), location);
}

class Preprocessor {
public:
    Preprocessor(SourceManager& sources, Diagnostics& diagnostics)
        : sources_(sources), diagnostics_(diagnostics) {
        defineObjectMacro("__STDC__", "1", {});
        defineObjectMacro("__STDC_VERSION__", "199409L", {});
        defineObjectMacro("__KEVINC__", "1", {});
        defineObjectMacro("__DATE__", quoteForMacro(currentDate()), {});
        defineObjectMacro("__TIME__", quoteForMacro(currentTime()), {});
    }

    void addIncludeDirectory(fs::path directory) {
        includeDirectories_.push_back(std::move(directory));
    }

    void defineFromCommandLine(std::string_view definition) {
        const std::size_t equals = definition.find('=');
        std::string name(definition.substr(0, equals));
        std::string value = equals == std::string_view::npos
                                ? std::string("1")
                                : std::string(definition.substr(equals + 1));
        defineObjectMacro(std::move(name), std::move(value), {});
    }

    void undefineFromCommandLine(std::string_view name) {
        macros_.erase(std::string(name));
    }

    [[nodiscard]] std::vector<Token> run(const fs::path& mainFile) {
        std::vector<Token> tokens = processFile(mainFile, 0, SourceLocation{});
        SourceLocation endLocation;
        if (!tokens.empty()) {
            endLocation = tokens.back().location;
        } else {
            endLocation = SourceLocation{0, 1, 1};
        }
        tokens.push_back(makeToken(TokenKind::End, std::string{}, endLocation));
        return tokens;
    }

    [[nodiscard]] const std::map<std::string, Macro>& macros() const noexcept { return macros_; }

private:
    struct Conditional {
        bool enclosingActive = true;
        bool active = true;
        bool taken = false;
        bool sawElse = false;
        SourceLocation location;
    };

    struct ArgumentList {
        bool ok = false;
        std::size_t closeIndex = 0;
        std::vector<std::vector<Token>> arguments;
    };

    [[nodiscard]] static std::string currentDate() {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#if defined(_MSC_VER)
        localtime_s(&local, &now);
#else
        if (const std::tm* value = std::localtime(&now); value != nullptr) {
            local = *value;
        }
#endif
        char buffer[32] = {};
        std::strftime(buffer, sizeof(buffer), "%b %e %Y", &local);
        return buffer;
    }

    [[nodiscard]] static std::string currentTime() {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#if defined(_MSC_VER)
        localtime_s(&local, &now);
#else
        if (const std::tm* value = std::localtime(&now); value != nullptr) {
            local = *value;
        }
#endif
        char buffer[16] = {};
        std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &local);
        return buffer;
    }

    [[nodiscard]] static std::string quoteForMacro(std::string_view text) {
        std::string result = "\"";
        for (const char ch : text) {
            if (ch == '\\' || ch == '\"') {
                result.push_back('\\');
            }
            result.push_back(ch);
        }
        result.push_back('\"');
        return result;
    }

    void defineObjectMacro(std::string name, std::string value, SourceLocation location) {
        const std::uint32_t file = sources_.addVirtualFile("<command line>", std::move(value));
        Lexer lexer(file, sources_.file(file), diagnostics_);
        Macro macro;
        macro.name = std::move(name);
        macro.location = location;
        for (;;) {
            Token token = lexer.next();
            if (token.kind == TokenKind::End || token.kind == TokenKind::Newline) {
                break;
            }
            macro.replacement.push_back(std::move(token));
        }
        macros_[macro.name] = std::move(macro);
    }

    [[nodiscard]] std::vector<Token> processFile(const fs::path& path, int depth,
                                                 SourceLocation includeLocation) {
        if (depth > 32) {
            diagnostics_.error(includeLocation, "include depth limit exceeded");
            return {};
        }
        std::error_code error;
        const fs::path canonical = fs::weakly_canonical(path, error);
        const fs::path identity = error ? path : canonical;
        if (onceFiles_.count(identity) != 0) {
            return {};
        }
        if (std::find(activeFiles_.begin(), activeFiles_.end(), identity) != activeFiles_.end()) {
            // Include cycle; the include guard in the file, if any, handles it.
            return {};
        }
        const auto fileIndex = sources_.addFile(path);
        if (!fileIndex.has_value()) {
            diagnostics_.error(includeLocation, concat("cannot open source file '", path.string(),
                                                       "'"));
            return {};
        }
        const SourceFile& file = sources_.file(*fileIndex);
        const fs::path directory = path.has_parent_path() ? path.parent_path() : fs::path(".");
        Lexer lexer(*fileIndex, file, diagnostics_);
        std::vector<Token> raw;
        for (;;) {
            Token token = lexer.next();
            const bool end = token.kind == TokenKind::End;
            raw.push_back(std::move(token));
            if (end) {
                break;
            }
        }
        activeFiles_.push_back(identity);
        std::vector<Token> result = processTokenList(std::move(raw), directory, depth, path);
        activeFiles_.pop_back();
        return result;
    }

    [[nodiscard]] std::vector<Token> processTokenList(std::vector<Token> raw, const fs::path& directory,
                                                      int depth, const fs::path& sourcePath) {
        std::vector<Token> output;
        std::vector<Token> segment;
        std::vector<Conditional> conditionals;

        const auto active = [&conditionals]() -> bool {
            return conditionals.empty() ||
                   (conditionals.back().enclosingActive && conditionals.back().active);
        };

        const auto flushSegment = [&]() {
            if (segment.empty()) {
                return;
            }
            std::vector<Token> expanded = expand(std::move(segment));
            segment.clear();
            for (Token& token : expanded) {
                output.push_back(std::move(token));
            }
        };

        std::size_t index = 0;
        while (index < raw.size()) {
            const Token& token = raw[index];
            if (token.kind == TokenKind::End) {
                break;
            }
            if (token.beginningOfLine && token.isPunct("#")) {
                flushSegment();
                std::vector<Token> line;
                std::size_t end = index + 1;
                while (end < raw.size() && raw[end].kind != TokenKind::Newline &&
                       raw[end].kind != TokenKind::End) {
                    line.push_back(raw[end]);
                    ++end;
                }
                handleDirective(line, directory, depth, conditionals, output, sourcePath);
                index = end;
                if (index < raw.size() && raw[index].kind == TokenKind::Newline) {
                    ++index;
                }
                continue;
            }
            if (token.kind == TokenKind::Newline) {
                ++index;
                continue;
            }
            if (active()) {
                segment.push_back(token);
            }
            ++index;
        }

        flushSegment();
        if (!conditionals.empty()) {
            diagnostics_.error(conditionals.back().location, "unterminated conditional directive");
        }
        return output;
    }

    void handleDirective(const std::vector<Token>& line, const fs::path& directory, int depth,
                         std::vector<Conditional>& conditionals, std::vector<Token>& output,
                         const fs::path& sourcePath) {
        if (line.empty()) {
            return;
        }
        const Token& head = line.front();
        const std::string& directive = head.text;

        const auto currentActive = [&conditionals]() -> bool {
            return conditionals.empty() ||
                   (conditionals.back().enclosingActive && conditionals.back().active);
        };

        if (directive == "if" || directive == "ifdef" || directive == "ifndef") {
            const bool enclosing = currentActive();
            bool value = false;
            if (directive == "if") {
                value = evaluateCondition(line, 1);
            } else {
                if (line.size() < 2 || line[1].kind != TokenKind::Identifier) {
                    diagnostics_.error(head.location, concat("#", directive, " requires a macro name"));
                } else {
                    value = macros_.count(line[1].text) != 0;
                    if (directive == "ifndef") {
                        value = !value;
                    }
                }
            }
            Conditional conditional;
            conditional.enclosingActive = enclosing;
            conditional.active = enclosing && value;
            conditional.taken = conditional.active;
            conditional.location = head.location;
            conditionals.push_back(conditional);
            return;
        }

        if (directive == "elif") {
            if (conditionals.empty()) {
                diagnostics_.error(head.location, "#elif without matching #if");
                return;
            }
            Conditional& conditional = conditionals.back();
            if (conditional.sawElse) {
                diagnostics_.error(head.location, "#elif after #else");
                return;
            }
            if (!conditional.enclosingActive || conditional.taken) {
                conditional.active = false;
            } else {
                const bool value = evaluateCondition(line, 1);
                conditional.active = value;
                conditional.taken = value;
            }
            return;
        }

        if (directive == "else") {
            if (conditionals.empty()) {
                diagnostics_.error(head.location, "#else without matching #if");
                return;
            }
            Conditional& conditional = conditionals.back();
            if (conditional.sawElse) {
                diagnostics_.error(head.location, "duplicate #else");
                return;
            }
            conditional.sawElse = true;
            conditional.active = conditional.enclosingActive && !conditional.taken;
            conditional.taken = true;
            return;
        }

        if (directive == "endif") {
            if (conditionals.empty()) {
                diagnostics_.error(head.location, "#endif without matching #if");
                return;
            }
            conditionals.pop_back();
            return;
        }

        if (!currentActive()) {
            return;
        }

        if (directive == "include") {
            includeFile(line, directory, depth, output);
            return;
        }
        if (directive == "define") {
            defineMacro(line);
            return;
        }
        if (directive == "undef") {
            if (line.size() < 2 || line[1].kind != TokenKind::Identifier) {
                diagnostics_.error(head.location, "#undef requires a macro name");
            } else {
                macros_.erase(line[1].text);
            }
            return;
        }
        if (directive == "error") {
            diagnostics_.error(head.location, concat("preprocessor error: ", joinTokenText(line, 1)));
            return;
        }
        if (directive == "warning") {
            diagnostics_.warning(head.location,
                                 concat("preprocessor warning: ", joinTokenText(line, 1)));
            return;
        }
        if (directive == "line") {
            // The directive is accepted; token locations keep their physical
            // line numbers, which keeps diagnostics tied to the real file.
            return;
        }
        if (directive == "pragma") {
            if (line.size() >= 2 && line[1].kind == TokenKind::Identifier &&
                line[1].text == "once") {
                std::error_code error;
                const fs::path canonical = fs::weakly_canonical(sourcePath, error);
                onceFiles_.insert(error ? sourcePath : canonical);
            }
            return;
        }
        if (line.front().kind == TokenKind::Number) {
            // A linemarker emitted by another preprocessor; accepted.
            return;
        }
        diagnostics_.warning(head.location, concat("unknown preprocessor directive '#", directive, "'"));
    }

    [[nodiscard]] static std::string joinTokenText(const std::vector<Token>& line,
                                                   std::size_t start) {
        std::string result;
        for (std::size_t index = start; index < line.size(); ++index) {
            if (!result.empty()) {
                result.push_back(' ');
            }
            result += line[index].text;
        }
        return result;
    }

    void defineMacro(const std::vector<Token>& line) {
        if (line.size() < 2 || line[1].kind != TokenKind::Identifier) {
            diagnostics_.error(line.front().location, "#define requires a macro name");
            return;
        }
        Macro macro;
        macro.name = line[1].text;
        macro.location = line[1].location;
        std::size_t index = 2;
        if (index < line.size() && line[index].isPunct("(") && !line[index].precededBySpace) {
            macro.functionLike = true;
            ++index;
            bool expectParameter = true;
            while (index < line.size()) {
                const Token& token = line[index];
                if (token.isPunct(")")) {
                    ++index;
                    expectParameter = false;
                    break;
                }
                if (token.isPunct(",")) {
                    if (expectParameter) {
                        diagnostics_.error(token.location, "expected macro parameter before ','");
                    }
                    expectParameter = true;
                    ++index;
                    continue;
                }
                if (token.isPunct("...")) {
                    macro.variadic = true;
                    ++index;
                    if (index < line.size() && !line[index].isPunct(")")) {
                        diagnostics_.error(line[index].location,
                                           "'...' must be the last macro parameter");
                    }
                    continue;
                }
                if (token.kind != TokenKind::Identifier) {
                    diagnostics_.error(token.location, "invalid macro parameter");
                    ++index;
                    continue;
                }
                if (std::find(macro.parameters.begin(), macro.parameters.end(), token.text) !=
                    macro.parameters.end()) {
                    diagnostics_.error(token.location, concat("duplicate macro parameter '",
                                                              token.text, "'"));
                } else {
                    macro.parameters.push_back(token.text);
                }
                expectParameter = false;
                ++index;
            }
            if (expectParameter && !macro.parameters.empty()) {
                diagnostics_.warning(line.front().location, "trailing comma in macro parameter list");
            }
        }
        while (index < line.size()) {
            macro.replacement.push_back(line[index]);
            ++index;
        }
        macros_[macro.name] = std::move(macro);
    }

    void includeFile(const std::vector<Token>& line, const fs::path& directory, int depth,
                     std::vector<Token>& output) {
        if (line.size() < 2) {
            diagnostics_.error(line.front().location, "#include requires a file name");
            return;
        }
        std::vector<Token> rest(line.begin() + 1, line.end());
        rest = expand(std::move(rest));
        if (rest.empty()) {
            diagnostics_.error(line.front().location, "empty #include");
            return;
        }
        std::string name;
        bool quoted = false;
        if (rest.front().kind == TokenKind::StringLiteral) {
            const auto decoded = literals::stringValue(rest.front(), diagnostics_);
            if (!decoded.has_value()) {
                return;
            }
            name = *decoded;
            quoted = true;
        } else if (rest.front().isPunct("<")) {
            bool closed = false;
            for (std::size_t index = 1; index < rest.size(); ++index) {
                if (rest[index].isPunct(">")) {
                    closed = true;
                    break;
                }
                name += rest[index].text;
            }
            if (!closed) {
                diagnostics_.error(rest.front().location, "unterminated #include <...>");
                return;
            }
        } else {
            diagnostics_.error(rest.front().location, "invalid #include spelling");
            return;
        }

        const std::optional<fs::path> resolved = resolveInclude(name, quoted, directory);
        if (!resolved.has_value()) {
            diagnostics_.error(line.front().location,
                               concat("cannot find include file '", name, "'"));
            return;
        }
        std::vector<Token> included = processFile(*resolved, depth + 1, line.front().location);
        for (Token& token : included) {
            output.push_back(std::move(token));
        }
    }

    [[nodiscard]] std::optional<fs::path> resolveInclude(std::string_view name, bool quoted,
                                                         const fs::path& directory) const {
        std::vector<fs::path> candidates;
        if (quoted) {
            candidates.push_back(directory / name);
        }
        for (const fs::path& includeDirectory : includeDirectories_) {
            candidates.push_back(includeDirectory / name);
        }
        if (fs::path direct(name); direct.is_absolute()) {
            candidates.insert(candidates.begin(), direct);
        }
        for (const fs::path& candidate : candidates) {
            std::error_code error;
            if (fs::is_regular_file(candidate, error) && !error) {
                return candidate;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool evaluateCondition(const std::vector<Token>& line, std::size_t start) {
        std::vector<Token> expression;
        for (std::size_t index = start; index < line.size(); ++index) {
            const Token& token = line[index];
            if (token.kind == TokenKind::Identifier && token.text == "defined") {
                std::size_t next = index + 1;
                bool parenthesized = false;
                if (next < line.size() && line[next].isPunct("(")) {
                    parenthesized = true;
                    ++next;
                }
                if (next >= line.size() || line[next].kind != TokenKind::Identifier) {
                    diagnostics_.error(token.location, "defined requires a macro name");
                    expression.push_back(makeNumberToken("0", token.location));
                    index = next == 0 ? 0 : next - 1;
                    continue;
                }
                const bool defined = macros_.count(line[next].text) != 0;
                expression.push_back(makeNumberToken(defined ? "1" : "0", token.location));
                ++next;
                if (parenthesized) {
                    if (next < line.size() && line[next].isPunct(")")) {
                        ++next;
                    } else {
                        diagnostics_.error(token.location, "expected ')' after defined");
                    }
                }
                index = next - 1;
                continue;
            }
            expression.push_back(token);
        }

        expression = expand(std::move(expression));
        for (Token& token : expression) {
            if (token.kind == TokenKind::Identifier) {
                token.kind = TokenKind::Number;
                token.text = "0";
            } else if (token.kind == TokenKind::CharLiteral) {
                const auto value = literals::charValue(token, diagnostics_);
                token.kind = TokenKind::Number;
                token.text = value.has_value() ? std::to_string(*value) : "0";
            } else if (token.kind == TokenKind::StringLiteral) {
                diagnostics_.error(token.location, "string literal in #if expression");
                token.kind = TokenKind::Number;
                token.text = "0";
            }
        }

        PPExpressionParser parser(std::move(expression), diagnostics_);
        return parser.parse() != 0;
    }

    [[nodiscard]] static int precedenceOf(std::string_view op) noexcept {
        if (op == "||") {
            return 1;
        }
        if (op == "&&") {
            return 2;
        }
        if (op == "|") {
            return 3;
        }
        if (op == "^") {
            return 4;
        }
        if (op == "&") {
            return 5;
        }
        if (op == "==" || op == "!=") {
            return 6;
        }
        if (op == "<" || op == ">" || op == "<=" || op == ">=") {
            return 7;
        }
        if (op == "<<" || op == ">>") {
            return 8;
        }
        if (op == "+" || op == "-") {
            return 9;
        }
        if (op == "*" || op == "/" || op == "%") {
            return 10;
        }
        return 0;
    }

    class PPExpressionParser {
    public:
        PPExpressionParser(std::vector<Token> tokens, Diagnostics& diagnostics)
            : tokens_(std::move(tokens)), diagnostics_(diagnostics) {}

        [[nodiscard]] std::int64_t parse() {
            const std::int64_t value = parseConditional();
            if (!atEnd()) {
                diagnostics_.error(current().location,
                                   concat("unexpected token '", current().text, "' in #if"));
            }
            return value;
        }

    private:
        [[nodiscard]] bool atEnd() const noexcept { return index_ >= tokens_.size(); }
        [[nodiscard]] const Token& current() const noexcept { return tokens_[index_]; }

        [[nodiscard]] bool match(std::string_view op) {
            if (!atEnd() && current().isPunct(op)) {
                ++index_;
                return true;
            }
            return false;
        }

        void expect(std::string_view op) {
            if (!match(op)) {
                diagnostics_.error(atEnd() ? SourceLocation{} : current().location,
                                   concat("expected '", op, "' in #if expression"));
            }
        }

        [[nodiscard]] std::int64_t parseConditional() {
            const std::int64_t condition = parseBinary(1);
            if (match("?")) {
                const std::int64_t whenTrue = parseConditional();
                expect(":");
                const std::int64_t whenFalse = parseConditional();
                return condition != 0 ? whenTrue : whenFalse;
            }
            return condition;
        }

        [[nodiscard]] std::int64_t parseBinary(int minimumPrecedence) {
            std::int64_t left = parseUnary();
            while (!atEnd() && current().kind == TokenKind::Punctuator) {
                const std::string op = current().text;
                const int precedence = precedenceOf(op);
                if (precedence == 0 || precedence < minimumPrecedence) {
                    break;
                }
                const SourceLocation location = current().location;
                ++index_;
                const std::int64_t right = parseBinary(precedence + 1);
                left = apply(op, left, right, location);
            }
            return left;
        }

        [[nodiscard]] std::int64_t parseUnary() {
            if (match("+")) {
                return parseUnary();
            }
            if (match("-")) {
                return -parseUnary();
            }
            if (match("!")) {
                return parseUnary() == 0 ? 1 : 0;
            }
            if (match("~")) {
                return ~parseUnary();
            }
            if (match("(")) {
                const std::int64_t value = parseConditional();
                expect(")");
                return value;
            }
            if (atEnd()) {
                diagnostics_.error(SourceLocation{}, "unexpected end of #if expression");
                return 0;
            }
            if (current().kind != TokenKind::Number) {
                diagnostics_.error(current().location,
                                   concat("expected integer in #if, got '", current().text, "'"));
                ++index_;
                return 0;
            }
            const std::string text = current().text;
            const SourceLocation literalLocation = current().location;
            ++index_;
            std::string_view view = text;
            while (!view.empty() && (view.back() == 'u' || view.back() == 'U' || view.back() == 'l' ||
                                     view.back() == 'L')) {
                view.remove_suffix(1);
            }
            int base = 10;
            if (view.size() > 1 && view[0] == '0' && (view[1] == 'x' || view[1] == 'X')) {
                base = 16;
                view.remove_prefix(2);
            } else if (view.size() > 1 && view[0] == '0') {
                base = 8;
                view.remove_prefix(1);
            }
            std::int64_t value = 0;
            if (view.empty()) {
                return 0;
            }
            for (const char ch : view) {
                int digit = -1;
                if (base == 16) {
                    digit = hexValue(ch);
                } else if (ch >= '0' && ch <= '9') {
                    digit = ch - '0';
                }
                if (digit < 0 || digit >= base) {
                    diagnostics_.error(literalLocation,
                                       concat("invalid integer literal in #if: ", text));
                    return 0;
                }
                value = value * base + digit;
            }
            return value;
        }

        [[nodiscard]] std::int64_t apply(std::string_view op, std::int64_t left, std::int64_t right,
                                         SourceLocation location) {
            if (op == "||") {
                return (left != 0 || right != 0) ? 1 : 0;
            }
            if (op == "&&") {
                return (left != 0 && right != 0) ? 1 : 0;
            }
            if (op == "|") {
                return left | right;
            }
            if (op == "^") {
                return left ^ right;
            }
            if (op == "&") {
                return left & right;
            }
            if (op == "==") {
                return left == right ? 1 : 0;
            }
            if (op == "!=") {
                return left != right ? 1 : 0;
            }
            if (op == "<") {
                return left < right ? 1 : 0;
            }
            if (op == ">") {
                return left > right ? 1 : 0;
            }
            if (op == "<=") {
                return left <= right ? 1 : 0;
            }
            if (op == ">=") {
                return left >= right ? 1 : 0;
            }
            if (op == "<<") {
                return left << (right & 63);
            }
            if (op == ">>") {
                return left >> (right & 63);
            }
            if (op == "+") {
                return left + right;
            }
            if (op == "-") {
                return left - right;
            }
            if (op == "*") {
                return left * right;
            }
            if (op == "/" || op == "%") {
                if (right == 0) {
                    diagnostics_.error(location, "division by zero in #if expression");
                    return 0;
                }
                return op == "/" ? left / right : left % right;
            }
            diagnostics_.error(location, concat("unsupported operator '", op, "' in #if"));
            return 0;
        }

        std::vector<Token> tokens_;
        Diagnostics& diagnostics_;
        std::size_t index_ = 0;
    };

    [[nodiscard]] std::vector<Token> expand(std::vector<Token> input) {
        std::vector<Token> current = std::move(input);
        for (int pass = 0; pass < 32; ++pass) {
            bool changed = false;
            std::vector<Token> next;
            for (std::size_t index = 0; index < current.size(); ++index) {
                Token& token = current[index];
                if (token.kind == TokenKind::Identifier) {
                    if (token.text == "__LINE__") {
                        next.push_back(makeNumberToken(std::to_string(token.location.line),
                                                       token.location));
                        changed = true;
                        continue;
                    }
                    if (token.text == "__FILE__") {
                        next.push_back(makeToken(TokenKind::StringLiteral,
                                                 quoteForMacro(sources_.displayName(token.location.file)),
                                                 token.location));
                        changed = true;
                        continue;
                    }
                    const auto found = macros_.find(token.text);
                    if (found != macros_.end()) {
                        const Macro& macro = found->second;
                        if (!macro.functionLike) {
                            std::vector<Token> replacement = substitute(macro, {});
                            next.insert(next.end(), replacement.begin(), replacement.end());
                            changed = true;
                            continue;
                        }
                        std::size_t open = index + 1;
                        while (open < current.size() && current[open].kind == TokenKind::Newline) {
                            ++open;
                        }
                        if (open < current.size() && current[open].isPunct("(")) {
                            ArgumentList arguments = parseArguments(current, open, macro, token.location);
                            if (arguments.ok) {
                                std::vector<Token> replacement =
                                    substitute(macro, arguments.arguments);
                                next.insert(next.end(), replacement.begin(), replacement.end());
                                index = arguments.closeIndex;
                                changed = true;
                                continue;
                            }
                        }
                    }
                }
                next.push_back(std::move(token));
            }
            current = std::move(next);
            if (!changed) {
                break;
            }
        }
        return current;
    }

    [[nodiscard]] ArgumentList parseArguments(const std::vector<Token>& tokens, std::size_t openIndex,
                                              const Macro& macro, SourceLocation location) {
        ArgumentList result;
        if (!tokens[openIndex].isPunct("(")) {
            return result;
        }
        std::vector<std::vector<Token>> arguments;
        std::vector<Token> currentArgument;
        int depth = 0;
        std::size_t index = openIndex + 1;
        bool closed = false;
        for (; index < tokens.size(); ++index) {
            const Token& token = tokens[index];
            if (token.kind == TokenKind::Newline) {
                continue;
            }
            if (token.isPunct("(")) {
                ++depth;
                currentArgument.push_back(token);
                continue;
            }
            if (token.isPunct(")")) {
                if (depth == 0) {
                    closed = true;
                    break;
                }
                --depth;
                currentArgument.push_back(token);
                continue;
            }
            if (token.isPunct(",") && depth == 0) {
                arguments.push_back(std::move(currentArgument));
                currentArgument.clear();
                continue;
            }
            currentArgument.push_back(token);
        }
        if (!closed) {
            diagnostics_.error(location, "unterminated macro argument list");
            return result;
        }
        arguments.push_back(std::move(currentArgument));

        if (macro.parameters.empty() && !macro.variadic && arguments.size() == 1 &&
            arguments.front().empty()) {
            arguments.clear();
        }

        if (macro.variadic) {
            const std::size_t fixed = macro.parameters.size();
            if (arguments.size() < fixed) {
                arguments.resize(fixed);
            }
            if (arguments.size() == fixed) {
                arguments.emplace_back();
            } else if (arguments.size() > fixed + 1) {
                std::vector<Token> combined;
                for (std::size_t part = fixed; part < arguments.size(); ++part) {
                    if (!combined.empty()) {
                        combined.push_back(makeToken(TokenKind::Punctuator, ",", location));
                    }
                    combined.insert(combined.end(), arguments[part].begin(), arguments[part].end());
                }
                arguments.resize(fixed);
                arguments.push_back(std::move(combined));
            }
        } else if (arguments.size() < macro.parameters.size()) {
            while (arguments.size() < macro.parameters.size()) {
                arguments.emplace_back();
            }
            diagnostics_.error(location, concat("macro '", macro.name, "' requires ",
                                                macro.parameters.size(), " arguments"));
        } else if (arguments.size() > macro.parameters.size()) {
            diagnostics_.error(location, concat("macro '", macro.name, "' has too many arguments"));
            arguments.resize(macro.parameters.size());
        }

        result.ok = true;
        result.closeIndex = index;
        result.arguments = std::move(arguments);
        return result;
    }

    [[nodiscard]] int parameterIndex(const Macro& macro, std::string_view name) const {
        for (std::size_t index = 0; index < macro.parameters.size(); ++index) {
            if (macro.parameters[index] == name) {
                return static_cast<int>(index);
            }
        }
        if (macro.variadic && name == "__VA_ARGS__") {
            return static_cast<int>(macro.parameters.size());
        }
        return -1;
    }

    [[nodiscard]] std::vector<Token> replacementFor(
        const Macro& macro, const Token& token,
        const std::vector<std::vector<Token>>& arguments) const {
        const int parameter = parameterIndex(macro, token.text);
        if (parameter >= 0 && static_cast<std::size_t>(parameter) < arguments.size()) {
            return arguments[static_cast<std::size_t>(parameter)];
        }
        return {token};
    }

    [[nodiscard]] Token pasteTokens(const Token& left, const Token& right,
                                    SourceLocation location) const {
        const std::string text = left.text + right.text;
        return makeToken(classifyTokenText(text), text, location);
    }

    [[nodiscard]] Token stringize(const std::vector<Token>& argument,
                                  SourceLocation location) const {
        std::string text = "\"";
        for (std::size_t index = 0; index < argument.size(); ++index) {
            if (index != 0 && argument[index].precededBySpace) {
                text.push_back(' ');
            }
            for (const char ch : argument[index].text) {
                if (ch == '\\' || ch == '\"') {
                    text.push_back('\\');
                }
                text.push_back(ch);
            }
        }
        text.push_back('\"');
        return makeToken(TokenKind::StringLiteral, text, location);
    }

    [[nodiscard]] std::vector<Token> substitute(
        const Macro& macro, const std::vector<std::vector<Token>>& arguments) const {
        std::vector<Token> result;
        for (std::size_t index = 0; index < macro.replacement.size(); ++index) {
            const Token& token = macro.replacement[index];
            if (macro.functionLike && token.isPunct("#") && index + 1 < macro.replacement.size()) {
                const int parameter = parameterIndex(macro, macro.replacement[index + 1].text);
                if (parameter >= 0 && static_cast<std::size_t>(parameter) < arguments.size()) {
                    result.push_back(stringize(arguments[static_cast<std::size_t>(parameter)],
                                               token.location));
                    ++index;
                    continue;
                }
            }
            if (token.isPunct("##")) {
                if (index + 1 >= macro.replacement.size()) {
                    diagnostics_.error(token.location, "'##' at end of macro replacement");
                    continue;
                }
                const Token& right = macro.replacement[++index];
                std::vector<Token> rightTokens = replacementFor(macro, right, arguments);
                if (result.empty()) {
                    result = std::move(rightTokens);
                } else if (!rightTokens.empty()) {
                    Token pasted = pasteTokens(result.back(), rightTokens.front(), token.location);
                    result.pop_back();
                    result.push_back(std::move(pasted));
                    result.insert(result.end(), rightTokens.begin() + 1, rightTokens.end());
                }
                continue;
            }
            const int parameter = parameterIndex(macro, token.text);
            if (parameter >= 0 && static_cast<std::size_t>(parameter) < arguments.size()) {
                const auto& argument = arguments[static_cast<std::size_t>(parameter)];
                result.insert(result.end(), argument.begin(), argument.end());
            } else {
                result.push_back(token);
            }
        }
        return result;
    }

    SourceManager& sources_;
    Diagnostics& diagnostics_;
    std::map<std::string, Macro> macros_;
    std::vector<fs::path> includeDirectories_;
    std::vector<fs::path> activeFiles_;
    std::set<fs::path> onceFiles_;
};

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

enum class TypeKind {
    Void,
    Bool,
    Char,
    SChar,
    UChar,
    Short,
    UShort,
    Int,
    UInt,
    Long,
    ULong,
    LongLong,
    ULongLong,
    Float,
    Double,
    LongDouble,
    Pointer,
    Array,
    Function,
    Struct,
    Union,
    Enum
};

struct Type;
using TypePtr = std::shared_ptr<Type>;

struct RecordMember {
    std::string name;
    TypePtr type;
    int offset = 0;
    int bitWidth = -1;
    SourceLocation location;
};

struct RecordInfo {
    std::string tag;
    bool isUnion = false;
    bool complete = false;
    std::vector<RecordMember> members;
    int size = 0;
    int alignment = 1;
    SourceLocation location;
};

struct EnumConstant {
    std::string name;
    std::int64_t value = 0;
    SourceLocation location;
};

struct EnumInfo {
    std::string tag;
    bool complete = false;
    std::vector<EnumConstant> constants;
    SourceLocation location;
};

struct Type {
    TypeKind kind = TypeKind::Void;
    TypePtr element;                    // pointer target or array element
    std::int64_t arrayLength = -1;      // -1 means an incomplete array
    TypePtr returnType;                 // function return type
    std::vector<TypePtr> parameterTypes;
    std::vector<std::string> parameterNames;
    bool variadic = false;
    bool hasPrototype = true;
    std::shared_ptr<RecordInfo> record;
    std::shared_ptr<EnumInfo> enumeration;
    bool isConst = false;
    bool isVolatile = false;

    [[nodiscard]] bool isVoid() const noexcept { return kind == TypeKind::Void; }
    [[nodiscard]] bool isBool() const noexcept { return kind == TypeKind::Bool; }
    [[nodiscard]] bool isInteger() const noexcept {
        switch (kind) {
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::SChar:
        case TypeKind::UChar:
        case TypeKind::Short:
        case TypeKind::UShort:
        case TypeKind::Int:
        case TypeKind::UInt:
        case TypeKind::Long:
        case TypeKind::ULong:
        case TypeKind::LongLong:
        case TypeKind::ULongLong:
        case TypeKind::Enum:
            return true;
        default:
            return false;
        }
    }
    [[nodiscard]] bool isFloating() const noexcept {
        return kind == TypeKind::Float || kind == TypeKind::Double || kind == TypeKind::LongDouble;
    }
    [[nodiscard]] bool isArithmetic() const noexcept { return isInteger() || isFloating(); }
    [[nodiscard]] bool isPointer() const noexcept { return kind == TypeKind::Pointer; }
    [[nodiscard]] bool isArray() const noexcept { return kind == TypeKind::Array; }
    [[nodiscard]] bool isFunction() const noexcept { return kind == TypeKind::Function; }
    [[nodiscard]] bool isRecord() const noexcept {
        return kind == TypeKind::Struct || kind == TypeKind::Union;
    }
    [[nodiscard]] bool isEnum() const noexcept { return kind == TypeKind::Enum; }
    [[nodiscard]] bool isScalar() const noexcept { return isArithmetic() || isPointer(); }
    [[nodiscard]] bool isSignedInteger() const noexcept {
        switch (kind) {
        case TypeKind::Char:
        case TypeKind::SChar:
        case TypeKind::Short:
        case TypeKind::Int:
        case TypeKind::Long:
        case TypeKind::LongLong:
            return true;
        default:
            return false;
        }
    }
    [[nodiscard]] bool isUnsignedInteger() const noexcept {
        return isInteger() && !isSignedInteger() && !isBool();
    }
};

[[nodiscard]] inline int typeRank(TypeKind kind) noexcept {
    switch (kind) {
    case TypeKind::Bool:
        return 0;
    case TypeKind::Char:
    case TypeKind::SChar:
    case TypeKind::UChar:
        return 1;
    case TypeKind::Short:
    case TypeKind::UShort:
        return 2;
    case TypeKind::Int:
    case TypeKind::UInt:
    case TypeKind::Enum:
        return 3;
    case TypeKind::Long:
    case TypeKind::ULong:
        return 4;
    case TypeKind::LongLong:
    case TypeKind::ULongLong:
        return 5;
    default:
        return 6;
    }
}

class TypeContext {
public:
    TypeContext() {
        voidType_ = make(TypeKind::Void);
        boolType_ = make(TypeKind::Bool);
        charType_ = make(TypeKind::Char);
        signedCharType_ = make(TypeKind::SChar);
        unsignedCharType_ = make(TypeKind::UChar);
        shortType_ = make(TypeKind::Short);
        unsignedShortType_ = make(TypeKind::UShort);
        intType_ = make(TypeKind::Int);
        unsignedIntType_ = make(TypeKind::UInt);
        longType_ = make(TypeKind::Long);
        unsignedLongType_ = make(TypeKind::ULong);
        longLongType_ = make(TypeKind::LongLong);
        unsignedLongLongType_ = make(TypeKind::ULongLong);
        floatType_ = make(TypeKind::Float);
        doubleType_ = make(TypeKind::Double);
        longDoubleType_ = make(TypeKind::LongDouble);
    }

    [[nodiscard]] TypePtr voidType() const noexcept { return voidType_; }
    [[nodiscard]] TypePtr boolType() const noexcept { return boolType_; }
    [[nodiscard]] TypePtr charType() const noexcept { return charType_; }
    [[nodiscard]] TypePtr signedCharType() const noexcept { return signedCharType_; }
    [[nodiscard]] TypePtr unsignedCharType() const noexcept { return unsignedCharType_; }
    [[nodiscard]] TypePtr shortType() const noexcept { return shortType_; }
    [[nodiscard]] TypePtr unsignedShortType() const noexcept { return unsignedShortType_; }
    [[nodiscard]] TypePtr intType() const noexcept { return intType_; }
    [[nodiscard]] TypePtr unsignedIntType() const noexcept { return unsignedIntType_; }
    [[nodiscard]] TypePtr longType() const noexcept { return longType_; }
    [[nodiscard]] TypePtr unsignedLongType() const noexcept { return unsignedLongType_; }
    [[nodiscard]] TypePtr longLongType() const noexcept { return longLongType_; }
    [[nodiscard]] TypePtr unsignedLongLongType() const noexcept { return unsignedLongLongType_; }
    [[nodiscard]] TypePtr floatType() const noexcept { return floatType_; }
    [[nodiscard]] TypePtr doubleType() const noexcept { return doubleType_; }
    [[nodiscard]] TypePtr longDoubleType() const noexcept { return longDoubleType_; }

    [[nodiscard]] TypePtr pointerTo(TypePtr target) const {
        TypePtr type = make(TypeKind::Pointer);
        type->element = std::move(target);
        return type;
    }

    [[nodiscard]] TypePtr arrayOf(TypePtr element, std::int64_t length) const {
        TypePtr type = make(TypeKind::Array);
        type->element = std::move(element);
        type->arrayLength = length;
        return type;
    }

    [[nodiscard]] TypePtr functionOf(TypePtr result, std::vector<TypePtr> parameters,
                                     std::vector<std::string> names, bool variadic,
                                     bool hasPrototype) const {
        TypePtr type = make(TypeKind::Function);
        type->returnType = std::move(result);
        type->parameterTypes = std::move(parameters);
        type->parameterNames = std::move(names);
        type->variadic = variadic;
        type->hasPrototype = hasPrototype;
        return type;
    }

    [[nodiscard]] TypePtr recordType(const std::shared_ptr<RecordInfo>& record) const {
        TypePtr type = make(record->isUnion ? TypeKind::Union : TypeKind::Struct);
        type->record = record;
        return type;
    }

    [[nodiscard]] TypePtr enumType(const std::shared_ptr<EnumInfo>& enumeration) const {
        TypePtr type = make(TypeKind::Enum);
        type->enumeration = enumeration;
        return type;
    }

    [[nodiscard]] TypePtr qualified(TypePtr base, bool makeConst, bool makeVolatile) const {
        if (!makeConst && !makeVolatile) {
            return base;
        }
        TypePtr copy = std::make_shared<Type>(*base);
        copy->isConst = copy->isConst || makeConst;
        copy->isVolatile = copy->isVolatile || makeVolatile;
        return copy;
    }

    [[nodiscard]] std::size_t sizeOf(const TypePtr& type) const;
    [[nodiscard]] std::size_t alignOf(const TypePtr& type) const;
    [[nodiscard]] bool isComplete(const TypePtr& type) const;
    [[nodiscard]] std::string describe(const TypePtr& type) const;
    [[nodiscard]] TypePtr decay(const TypePtr& type) const;

    [[nodiscard]] static bool sameType(const TypePtr& left, const TypePtr& right);
    [[nodiscard]] static bool compatibleType(const TypePtr& left, const TypePtr& right);

private:
    [[nodiscard]] static TypePtr make(TypeKind kind) {
        TypePtr type = std::make_shared<Type>();
        type->kind = kind;
        return type;
    }

    TypePtr voidType_;
    TypePtr boolType_;
    TypePtr charType_;
    TypePtr signedCharType_;
    TypePtr unsignedCharType_;
    TypePtr shortType_;
    TypePtr unsignedShortType_;
    TypePtr intType_;
    TypePtr unsignedIntType_;
    TypePtr longType_;
    TypePtr unsignedLongType_;
    TypePtr longLongType_;
    TypePtr unsignedLongLongType_;
    TypePtr floatType_;
    TypePtr doubleType_;
    TypePtr longDoubleType_;
};

[[nodiscard]] inline std::size_t TypeContext::sizeOf(const TypePtr& type) const {
    if (!type) {
        return 0;
    }
    switch (type->kind) {
    case TypeKind::Void:
        return 1;
    case TypeKind::Bool:
    case TypeKind::Char:
    case TypeKind::SChar:
    case TypeKind::UChar:
        return 1;
    case TypeKind::Short:
    case TypeKind::UShort:
        return 2;
    case TypeKind::Int:
    case TypeKind::UInt:
    case TypeKind::Long:
    case TypeKind::ULong:
    case TypeKind::Float:
    case TypeKind::Pointer:
    case TypeKind::Enum:
        return 4;
    case TypeKind::LongLong:
    case TypeKind::ULongLong:
    case TypeKind::Double:
    case TypeKind::LongDouble:
        return 8;
    case TypeKind::Array:
        return type->arrayLength < 0 ? 0U
                                     : static_cast<std::size_t>(type->arrayLength) * sizeOf(type->element);
    case TypeKind::Function:
        return 1;
    case TypeKind::Struct:
    case TypeKind::Union:
        return type->record ? static_cast<std::size_t>(type->record->size) : 0U;
    }
    return 0;
}

[[nodiscard]] inline std::size_t TypeContext::alignOf(const TypePtr& type) const {
    if (!type) {
        return 1;
    }
    switch (type->kind) {
    case TypeKind::Array:
        return alignOf(type->element);
    case TypeKind::Struct:
    case TypeKind::Union:
        return type->record ? static_cast<std::size_t>(type->record->alignment) : 1U;
    default:
        return std::max<std::size_t>(1U, std::min<std::size_t>(sizeOf(type), 8U));
    }
}

[[nodiscard]] inline bool TypeContext::isComplete(const TypePtr& type) const {
    if (!type) {
        return false;
    }
    switch (type->kind) {
    case TypeKind::Void:
        return false;
    case TypeKind::Struct:
    case TypeKind::Union:
        return type->record && type->record->complete;
    case TypeKind::Enum:
        return true;
    case TypeKind::Array:
        return type->arrayLength >= 0 && isComplete(type->element);
    default:
        return true;
    }
}

[[nodiscard]] inline TypePtr TypeContext::decay(const TypePtr& type) const {
    if (!type) {
        return type;
    }
    if (type->kind == TypeKind::Array) {
        return pointerTo(type->element);
    }
    if (type->kind == TypeKind::Function) {
        return pointerTo(type);
    }
    return type;
}

[[nodiscard]] inline bool TypeContext::sameType(const TypePtr& left, const TypePtr& right) {
    if (left == right) {
        return true;
    }
    if (!left || !right || left->kind != right->kind) {
        return false;
    }
    switch (left->kind) {
    case TypeKind::Pointer:
        return sameType(left->element, right->element);
    case TypeKind::Array:
        return left->arrayLength == right->arrayLength && sameType(left->element, right->element);
    case TypeKind::Function: {
        if (!sameType(left->returnType, right->returnType)) {
            return false;
        }
        if (left->variadic != right->variadic ||
            left->parameterTypes.size() != right->parameterTypes.size()) {
            return false;
        }
        for (std::size_t index = 0; index < left->parameterTypes.size(); ++index) {
            if (!sameType(left->parameterTypes[index], right->parameterTypes[index])) {
                return false;
            }
        }
        return true;
    }
    case TypeKind::Struct:
    case TypeKind::Union:
        return left->record == right->record;
    case TypeKind::Enum:
        return left->enumeration == right->enumeration;
    default:
        return true;
    }
}

[[nodiscard]] inline bool TypeContext::compatibleType(const TypePtr& left, const TypePtr& right) {
    if (sameType(left, right)) {
        return true;
    }
    if (!left || !right) {
        return false;
    }
    if (left->kind == TypeKind::Pointer && right->kind == TypeKind::Pointer) {
        const bool leftVoid = left->element && left->element->kind == TypeKind::Void;
        const bool rightVoid = right->element && right->element->kind == TypeKind::Void;
        return leftVoid || rightVoid || sameType(left->element, right->element);
    }
    return false;
}

[[nodiscard]] inline std::string TypeContext::describe(const TypePtr& type) const {
    if (!type) {
        return "<none>";
    }
    switch (type->kind) {
    case TypeKind::Void:
        return "void";
    case TypeKind::Bool:
        return "_Bool";
    case TypeKind::Char:
        return "char";
    case TypeKind::SChar:
        return "signed char";
    case TypeKind::UChar:
        return "unsigned char";
    case TypeKind::Short:
        return "short";
    case TypeKind::UShort:
        return "unsigned short";
    case TypeKind::Int:
        return "int";
    case TypeKind::UInt:
        return "unsigned int";
    case TypeKind::Long:
        return "long";
    case TypeKind::ULong:
        return "unsigned long";
    case TypeKind::LongLong:
        return "long long";
    case TypeKind::ULongLong:
        return "unsigned long long";
    case TypeKind::Float:
        return "float";
    case TypeKind::Double:
        return "double";
    case TypeKind::LongDouble:
        return "long double";
    case TypeKind::Pointer:
        return describe(type->element) + " *";
    case TypeKind::Array:
        return describe(type->element) + "[" +
               (type->arrayLength < 0 ? std::string{} : std::to_string(type->arrayLength)) + "]";
    case TypeKind::Function:
        return describe(type->returnType) + "(...)";
    case TypeKind::Struct:
        return "struct " + (type->record ? type->record->tag : std::string("<anonymous>"));
    case TypeKind::Union:
        return "union " + (type->record ? type->record->tag : std::string("<anonymous>"));
    case TypeKind::Enum:
        return "enum " + (type->enumeration ? type->enumeration->tag : std::string("<anonymous>"));
    }
    return "<unknown>";
}

// ---------------------------------------------------------------------------
// AST
// ---------------------------------------------------------------------------

struct Symbol;

enum class UnaryOp {
    Plus,
    Minus,
    LogicalNot,
    BitwiseNot,
    Dereference,
    AddressOf,
    PreIncrement,
    PreDecrement,
    PostIncrement,
    PostDecrement
};

enum class BinaryOp {
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    ShiftLeft,
    ShiftRight,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    Equal,
    NotEqual,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    LogicalAnd,
    LogicalOr
};

enum class AssignOp {
    Assign,
    AddAssign,
    SubtractAssign,
    MultiplyAssign,
    DivideAssign,
    ModuloAssign,
    ShiftLeftAssign,
    ShiftRightAssign,
    AndAssign,
    OrAssign,
    XorAssign
};

struct Initializer;
struct Declaration;

struct Expr {
    enum class Kind {
        IntegerLiteral,
        FloatingLiteral,
        CharacterLiteral,
        StringLiteral,
        Identifier,
        Unary,
        Binary,
        Assignment,
        Conditional,
        Call,
        Member,
        Index,
        Cast,
        SizeOf,
        Comma
    };

    Kind kind = Kind::IntegerLiteral;
    SourceLocation location;

    TypePtr type;
    bool lvalue = false;
    bool modifiableLvalue = false;

    std::int64_t integerValue = 0;
    bool integerUnsigned = false;
    long double floatingValue = 0.0L;
    std::string stringValue;
    std::string name;

    UnaryOp unaryOp = UnaryOp::Plus;
    BinaryOp binaryOp = BinaryOp::Add;
    AssignOp assignOp = AssignOp::Assign;

    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    std::unique_ptr<Expr> third;
    std::vector<std::unique_ptr<Expr>> arguments;

    RecordMember* member = nullptr;
    std::string memberName;
    bool isArrow = false;
    TypePtr sizeofType;
    TypePtr castType;

    Symbol* symbol = nullptr;

    std::optional<std::int64_t> constantValue;

    [[nodiscard]] Expr* clone() const;
};

struct Stmt {
    enum class Kind {
        Empty,
        Expression,
        Compound,
        Declaration,
        If,
        While,
        DoWhile,
        For,
        Switch,
        Case,
        Default,
        Break,
        Continue,
        Return,
        Goto,
        Label
    };

    Kind kind = Kind::Empty;
    SourceLocation location;

    std::unique_ptr<Expr> expression;
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> increment;
    std::unique_ptr<Stmt> body;
    std::unique_ptr<Stmt> elseBody;
    std::unique_ptr<Stmt> initStatement;
    std::unique_ptr<Declaration> declaration;
    std::vector<std::unique_ptr<Stmt>> children;

    std::string label;
    std::optional<std::int64_t> caseValue;
    std::string breakLabel;
    std::string continueLabel;
};

struct Initializer {
    bool isList = false;
    std::unique_ptr<Expr> expression;
    std::vector<std::unique_ptr<Initializer>> elements;
    SourceLocation location;
};

enum class StorageClass {
    None,
    Auto,
    Register,
    Static,
    Extern,
    Typedef
};

struct Parameter {
    std::string name;
    TypePtr type;
    SourceLocation location;
    Symbol* symbol = nullptr;
};

struct DeclaratorInfo {
    std::string name;
    TypePtr type;
    SourceLocation location;
    StorageClass storage = StorageClass::None;
    std::unique_ptr<Initializer> initializer;
    std::unique_ptr<Stmt> body;
    std::vector<Parameter> parameters;
    Symbol* symbol = nullptr;
};

struct Declaration {
    StorageClass storage = StorageClass::None;
    TypePtr baseType;
    bool isTypedef = false;
    SourceLocation location;
    std::vector<DeclaratorInfo> declarators;
};

struct TranslationUnit {
    std::vector<std::unique_ptr<Declaration>> declarations;
};

// A shallow structural clone used when semantic analysis needs to duplicate
// a scalar literal (for example, an enum value used as an expression).
[[nodiscard]] inline Expr* Expr::clone() const {
    auto copy = std::make_unique<Expr>();
    copy->kind = kind;
    copy->location = location;
    copy->type = type;
    copy->lvalue = lvalue;
    copy->modifiableLvalue = modifiableLvalue;
    copy->integerValue = integerValue;
    copy->integerUnsigned = integerUnsigned;
    copy->floatingValue = floatingValue;
    copy->stringValue = stringValue;
    copy->name = name;
    copy->unaryOp = unaryOp;
    copy->binaryOp = binaryOp;
    copy->assignOp = assignOp;
    copy->member = member;
    copy->memberName = memberName;
    copy->isArrow = isArrow;
    copy->sizeofType = sizeofType;
    copy->castType = castType;
    copy->symbol = symbol;
    copy->constantValue = constantValue;
    copy->left = left ? std::unique_ptr<Expr>(left->clone()) : nullptr;
    copy->right = right ? std::unique_ptr<Expr>(right->clone()) : nullptr;
    copy->third = third ? std::unique_ptr<Expr>(third->clone()) : nullptr;
    for (const auto& argument : arguments) {
        copy->arguments.emplace_back(argument->clone());
    }
    return copy.release();
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

class Parser {
public:
    Parser(std::vector<Token> tokens, TypeContext& types, Diagnostics& diagnostics)
        : tokens_(std::move(tokens)), types_(types), diagnostics_(diagnostics) {
        scopes_.emplace_back();
    }

    [[nodiscard]] std::unique_ptr<TranslationUnit> parse() {
        auto unit = std::make_unique<TranslationUnit>();
        while (!atEnd()) {
            const std::size_t before = index_;
            std::unique_ptr<Declaration> declaration = parseExternalDeclaration();
            if (declaration) {
                unit->declarations.push_back(std::move(declaration));
            }
            if (index_ == before) {
                diagnostics_.error(peek().location, "parser made no progress; skipping token");
                advance();
            }
        }
        return unit;
    }

private:
    struct DeclSpec {
        StorageClass storage = StorageClass::None;
        TypePtr type;
        TypePtr directType;
        bool isConst = false;
        bool isVolatile = false;
        bool isTypedef = false;
        bool hasBuiltin = false;
        bool sawVoid = false;
        bool sawBool = false;
        bool sawChar = false;
        bool sawShort = false;
        bool sawInt = false;
        bool sawFloat = false;
        bool sawDouble = false;
        bool sawSigned = false;
        bool sawUnsigned = false;
        int longCount = 0;
        SourceLocation location;
    };

    struct DeclaratorNode {
        enum class Kind { Name, Pointer, Array, Function };
        Kind kind = Kind::Name;
        std::string name;
        std::unique_ptr<DeclaratorNode> inner;
        std::int64_t arrayLength = -1;
        std::vector<Parameter> parameters;
        std::vector<TypePtr> parameterTypes;
        std::vector<std::string> parameterNames;
        bool variadic = false;
        bool hasPrototype = true;
        SourceLocation location;
    };

    struct ParameterList {
        std::vector<Parameter> parameters;
        bool variadic = false;
        bool hasPrototype = true;
        bool knr = false;
    };

    struct ParserScope {
        std::map<std::string, TypePtr> typedefs;
        std::map<std::string, TypePtr> tags;
        std::map<std::string, std::int64_t> enumConstants;
        std::set<std::string> ordinaryNames;
    };

    // -- token helpers ------------------------------------------------------

    [[nodiscard]] bool atEnd() const noexcept {
        return index_ >= tokens_.size() || tokens_[index_].kind == TokenKind::End;
    }

    [[nodiscard]] const Token& peek(std::size_t ahead = 0) const noexcept {
        const std::size_t at = index_ + ahead;
        if (at < tokens_.size()) {
            return tokens_[at];
        }
        return tokens_.back();
    }

    const Token& advance() {
        const Token& token = peek();
        if (!atEnd()) {
            ++index_;
        }
        return token;
    }

    [[nodiscard]] bool check(std::string_view punctuator) const noexcept {
        return !atEnd() && peek().isPunct(punctuator);
    }

    [[nodiscard]] bool match(std::string_view punctuator) {
        if (check(punctuator)) {
            ++index_;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool checkKeyword(std::string_view keyword) const noexcept {
        return !atEnd() && peek().kind == TokenKind::Identifier && peek().text == keyword;
    }

    [[nodiscard]] bool matchKeyword(std::string_view keyword) {
        if (checkKeyword(keyword)) {
            ++index_;
            return true;
        }
        return false;
    }

    bool expect(std::string_view punctuator, std::string_view context = {}) {
        if (match(punctuator)) {
            return true;
        }
        diagnostics_.error(peek().location,
                           concat("expected '", punctuator, "'",
                                  context.empty() ? std::string{} : concat(" ", context)));
        return false;
    }

    bool expectKeyword(std::string_view keyword, std::string_view context = {}) {
        if (matchKeyword(keyword)) {
            return true;
        }
        diagnostics_.error(peek().location,
                           concat("expected '", keyword, "'",
                                  context.empty() ? std::string{} : concat(" ", context)));
        return false;
    }

    void synchronize() {
        while (!atEnd() && !check(";") && !check("}")) {
            advance();
        }
        if (check(";")) {
            advance();
        }
    }

    [[nodiscard]] static bool isKeyword(std::string_view text) noexcept {
        static constexpr std::array<std::string_view, 39> keywords = {
            "auto",     "break",    "case",     "char",     "const",    "continue",
            "default",  "do",       "double",   "else",     "enum",     "extern",
            "float",    "for",      "goto",     "if",       "int",      "long",
            "register", "return",   "short",    "signed",   "sizeof",   "static",
            "struct",   "switch",   "typedef",  "union",    "unsigned", "void",
            "volatile", "while",    "_Bool",    "inline",   "restrict", "_Alignas",
            "_Alignof", "_Static_assert", "_Noreturn"
        };
        return std::find(keywords.begin(), keywords.end(), text) != keywords.end();
    }

    [[nodiscard]] static bool isBuiltinTypeKeyword(std::string_view text) noexcept {
        static constexpr std::array<std::string_view, 16> keywords = {
            "void",  "char",   "short",    "int",    "long",   "float", "double", "signed",
            "unsigned", "_Bool", "struct", "union",  "enum",   "const", "volatile", "typedef"
        };
        return std::find(keywords.begin(), keywords.end(), text) != keywords.end();
    }

    [[nodiscard]] bool looksLikeTypeName(std::size_t ahead = 0) const {
        const Token& token = peek(ahead);
        if (token.kind != TokenKind::Identifier) {
            return false;
        }
        if (isBuiltinTypeKeyword(token.text)) {
            return true;
        }
        return lookupTypedef(token.text) != nullptr;
    }

    [[nodiscard]] bool isDeclarationStart() const {
        if (atEnd() || peek().kind != TokenKind::Identifier) {
            return false;
        }
        const std::string_view text = peek().text;
        if (text == "auto" || text == "register" || text == "static" || text == "extern" ||
            text == "typedef" || text == "const" || text == "volatile" || text == "struct" ||
            text == "union" || text == "enum") {
            return true;
        }
        if (isBuiltinTypeKeyword(text)) {
            return true;
        }
        return lookupTypedef(text) != nullptr;
    }

    // -- scopes -------------------------------------------------------------

    void pushScope() { scopes_.emplace_back(); }
    void popScope() {
        if (scopes_.size() > 1) {
            scopes_.pop_back();
        }
    }

    [[nodiscard]] TypePtr lookupTypedef(std::string_view name) const {
        for (auto iterator = scopes_.rbegin(); iterator != scopes_.rend(); ++iterator) {
            if (iterator->ordinaryNames.count(std::string(name)) != 0) {
                return nullptr;
            }
            const auto found = iterator->typedefs.find(std::string(name));
            if (found != iterator->typedefs.end()) {
                return found->second;
            }
        }
        return nullptr;
    }

    [[nodiscard]] TypePtr lookupTag(std::string_view name) const {
        for (auto iterator = scopes_.rbegin(); iterator != scopes_.rend(); ++iterator) {
            const auto found = iterator->tags.find(std::string(name));
            if (found != iterator->tags.end()) {
                return found->second;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::optional<std::int64_t> lookupEnumConstant(std::string_view name) const {
        for (auto iterator = scopes_.rbegin(); iterator != scopes_.rend(); ++iterator) {
            const auto found = iterator->enumConstants.find(std::string(name));
            if (found != iterator->enumConstants.end()) {
                return found->second;
            }
        }
        return std::nullopt;
    }

    void registerName(const std::string& name, TypePtr type, bool isTypedefName) {
        if (name.empty()) {
            return;
        }
        if (isTypedefName) {
            scopes_.back().typedefs[name] = std::move(type);
        } else {
            scopes_.back().ordinaryNames.insert(name);
        }
    }

    void registerDeclarationNames(const Declaration& declaration) {
        for (const DeclaratorInfo& declarator : declaration.declarators) {
            registerName(declarator.name, declarator.type, declaration.isTypedef);
        }
    }

    // -- declarations -------------------------------------------------------

    [[nodiscard]] std::unique_ptr<Declaration> parseExternalDeclaration() {
        const SourceLocation location = peek().location;
        const std::size_t startIndex = index_;
        DeclSpec spec = parseDeclSpecifiers();
        auto declaration = std::make_unique<Declaration>();
        declaration->storage = spec.storage;
        declaration->baseType = spec.type;
        declaration->isTypedef = spec.isTypedef;
        declaration->location = location;

        if (match(";")) {
            return declaration;
        }

        bool sawDefinition = false;
        for (;;) {
            std::unique_ptr<DeclaratorNode> node = parseDeclaratorNode(false);
            if (!node) {
                synchronize();
                return declaration;
            }
            DeclaratorInfo info;
            info.storage = spec.storage;
            info.location = node->location;
            info.type = applyDeclaratorNode(*node, spec.type, info);

            if (info.type && info.type->isFunction() && !sawDefinition) {
                collectDefinitionParameters(*node, info);
                if (check("{")) {
                    info.body = parseFunctionBody(info);
                    sawDefinition = true;
                    declaration->declarators.push_back(std::move(info));
                    break;
                }
                if (!info.type->hasPrototype && isDeclarationStart()) {
                    parseKnRParameterDeclarations(info);
                    info.body = parseFunctionBody(info);
                    sawDefinition = true;
                    declaration->declarators.push_back(std::move(info));
                    break;
                }
            }

            if (match("=")) {
                info.initializer = parseInitializer();
            }
            declaration->declarators.push_back(std::move(info));

            if (match(",")) {
                continue;
            }
            expect(";");
            break;
        }

        registerDeclarationNames(*declaration);
        if (sawDefinition && declaration->declarators.size() != 1) {
            diagnostics_.warning(location, "function definition must be the only declarator");
        }
        if (index_ == startIndex) {
            advance();
        }
        return declaration;
    }

    [[nodiscard]] std::unique_ptr<Declaration> parseDeclaration(bool allowFunctionDefinition,
                                                                bool registerNames) {
        const SourceLocation location = peek().location;
        const std::size_t startIndex = index_;
        DeclSpec spec = parseDeclSpecifiers();
        auto declaration = std::make_unique<Declaration>();
        declaration->storage = spec.storage;
        declaration->baseType = spec.type;
        declaration->isTypedef = spec.isTypedef;
        declaration->location = location;

        if (match(";")) {
            return declaration;
        }

        for (;;) {
            std::unique_ptr<DeclaratorNode> node = parseDeclaratorNode(false);
            if (!node) {
                synchronize();
                return declaration;
            }
            DeclaratorInfo info;
            info.storage = spec.storage;
            info.location = node->location;
            info.type = applyDeclaratorNode(*node, spec.type, info);

            if (allowFunctionDefinition && info.type && info.type->isFunction()) {
                collectDefinitionParameters(*node, info);
                if (check("{")) {
                    info.body = parseFunctionBody(info);
                    declaration->declarators.push_back(std::move(info));
                    break;
                }
            }

            if (match("=")) {
                info.initializer = parseInitializer();
            }
            declaration->declarators.push_back(std::move(info));

            if (match(",")) {
                continue;
            }
            expect(";");
            break;
        }

        if (registerNames) {
            registerDeclarationNames(*declaration);
        }
        if (index_ == startIndex) {
            advance();
        }
        return declaration;
    }

    [[nodiscard]] DeclSpec parseDeclSpecifiers() {
        DeclSpec spec;
        spec.location = peek().location;
        bool sawStorage = false;

        const auto setStorage = [&](StorageClass storage) {
            if (sawStorage && spec.storage != storage) {
                diagnostics_.error(peek().location, "multiple storage classes");
            }
            sawStorage = true;
            spec.storage = storage;
            if (storage == StorageClass::Typedef) {
                spec.isTypedef = true;
            }
        };

        for (;;) {
            if (matchKeyword("typedef")) {
                setStorage(StorageClass::Typedef);
                continue;
            }
            if (matchKeyword("extern")) {
                setStorage(StorageClass::Extern);
                continue;
            }
            if (matchKeyword("static")) {
                setStorage(StorageClass::Static);
                continue;
            }
            if (matchKeyword("auto")) {
                setStorage(StorageClass::Auto);
                continue;
            }
            if (matchKeyword("register")) {
                setStorage(StorageClass::Register);
                continue;
            }
            if (matchKeyword("const")) {
                spec.isConst = true;
                continue;
            }
            if (matchKeyword("volatile")) {
                spec.isVolatile = true;
                continue;
            }
            if (matchKeyword("struct")) {
                spec.directType = parseRecordSpecifier(false);
                continue;
            }
            if (matchKeyword("union")) {
                spec.directType = parseRecordSpecifier(true);
                continue;
            }
            if (matchKeyword("enum")) {
                spec.directType = parseEnumSpecifier();
                continue;
            }
            if (matchKeyword("void")) {
                spec.sawVoid = true;
                spec.hasBuiltin = true;
                continue;
            }
            if (matchKeyword("_Bool")) {
                spec.sawBool = true;
                spec.hasBuiltin = true;
                continue;
            }
            if (matchKeyword("char")) {
                spec.sawChar = true;
                spec.hasBuiltin = true;
                continue;
            }
            if (matchKeyword("short")) {
                spec.sawShort = true;
                spec.hasBuiltin = true;
                continue;
            }
            if (matchKeyword("int")) {
                spec.sawInt = true;
                spec.hasBuiltin = true;
                continue;
            }
            if (matchKeyword("long")) {
                ++spec.longCount;
                spec.hasBuiltin = true;
                continue;
            }
            if (matchKeyword("float")) {
                spec.sawFloat = true;
                spec.hasBuiltin = true;
                continue;
            }
            if (matchKeyword("double")) {
                spec.sawDouble = true;
                spec.hasBuiltin = true;
                continue;
            }
            if (matchKeyword("signed")) {
                spec.sawSigned = true;
                spec.hasBuiltin = true;
                continue;
            }
            if (matchKeyword("unsigned")) {
                spec.sawUnsigned = true;
                spec.hasBuiltin = true;
                continue;
            }
            if (matchKeyword("inline") || matchKeyword("restrict")) {
                continue;
            }
            if (peek().kind == TokenKind::Identifier) {
                if (TypePtr alias = lookupTypedef(peek().text); alias != nullptr) {
                    if (spec.directType || spec.hasBuiltin) {
                        diagnostics_.error(peek().location, "type specifier combined with typedef name");
                    }
                    spec.directType = alias;
                    advance();
                    continue;
                }
            }
            break;
        }

        if (spec.directType && spec.hasBuiltin) {
            diagnostics_.error(spec.location, "invalid combination of type specifiers");
        }

        TypePtr type;
        if (spec.directType) {
            type = spec.directType;
        } else if (spec.hasBuiltin) {
            type = buildBuiltinType(spec);
        } else {
            diagnostics_.warning(spec.location, "declaration has no type; assuming int");
            type = types_.intType();
        }
        spec.type = types_.qualified(type, spec.isConst, spec.isVolatile);
        return spec;
    }

    [[nodiscard]] TypePtr buildBuiltinType(const DeclSpec& spec) {
        if (spec.sawVoid) {
            return types_.voidType();
        }
        if (spec.sawBool) {
            return types_.boolType();
        }
        if (spec.sawFloat) {
            if (spec.longCount > 0) {
                diagnostics_.error(spec.location, "'long float' is not a C89 type");
            }
            return types_.floatType();
        }
        if (spec.sawDouble) {
            return spec.longCount > 0 ? types_.longDoubleType() : types_.doubleType();
        }
        if (spec.sawChar) {
            if (spec.sawSigned) {
                return types_.signedCharType();
            }
            if (spec.sawUnsigned) {
                return types_.unsignedCharType();
            }
            return types_.charType();
        }
        if (spec.sawShort) {
            return spec.sawUnsigned ? types_.unsignedShortType() : types_.shortType();
        }
        if (spec.longCount >= 2) {
            return spec.sawUnsigned ? types_.unsignedLongLongType() : types_.longLongType();
        }
        if (spec.longCount == 1) {
            return spec.sawUnsigned ? types_.unsignedLongType() : types_.longType();
        }
        return spec.sawUnsigned ? types_.unsignedIntType() : types_.intType();
    }

    [[nodiscard]] TypePtr parseRecordSpecifier(bool isUnion) {
        const SourceLocation location = tokens_[index_ - 1].location;
        std::string tag;
        if (peek().kind == TokenKind::Identifier && !isKeyword(peek().text)) {
            tag = advance().text;
        }

        if (!check("{")) {
            if (tag.empty()) {
                diagnostics_.error(location, "expected struct or union tag");
                return types_.intType();
            }
            if (TypePtr existing = lookupTag(tag); existing != nullptr) {
                if ((isUnion && existing->kind != TypeKind::Union) ||
                    (!isUnion && existing->kind != TypeKind::Struct)) {
                    diagnostics_.error(location, concat("tag '", tag, "' used with the wrong kind"));
                }
                return existing;
            }
            auto record = std::make_shared<RecordInfo>();
            record->tag = tag;
            record->isUnion = isUnion;
            record->location = location;
            TypePtr type = types_.recordType(record);
            scopes_.back().tags[tag] = type;
            return type;
        }

        expect("{");
        std::shared_ptr<RecordInfo> record;
        TypePtr type;
        if (!tag.empty()) {
            const auto existing = scopes_.back().tags.find(tag);
            if (existing != scopes_.back().tags.end()) {
                TypePtr previous = existing->second;
                if ((isUnion && previous->kind != TypeKind::Union) ||
                    (!isUnion && previous->kind != TypeKind::Struct)) {
                    diagnostics_.error(location, concat("tag '", tag, "' used with the wrong kind"));
                }
                if (previous->record && previous->record->complete) {
                    diagnostics_.error(location, concat("redefinition of struct/union '", tag, "'"));
                }
                record = std::make_shared<RecordInfo>();
                record->tag = tag;
                record->isUnion = isUnion;
                record->location = location;
                previous->record = record;
                type = previous;
            } else {
                record = std::make_shared<RecordInfo>();
                record->tag = tag;
                record->isUnion = isUnion;
                record->location = location;
                type = types_.recordType(record);
                scopes_.back().tags[tag] = type;
            }
        } else {
            record = std::make_shared<RecordInfo>();
            record->isUnion = isUnion;
            record->location = location;
            type = types_.recordType(record);
        }

        record->isUnion = isUnion;
        record->complete = false;
        record->members.clear();

        while (!check("}") && !atEnd()) {
            if (match(";")) {
                continue;
            }
            const std::size_t before = index_;
            DeclSpec memberSpec = parseDeclSpecifiers();
            if (memberSpec.storage != StorageClass::None) {
                diagnostics_.error(memberSpec.location, "storage class is not allowed on a member");
            }
            if (match(";")) {
                diagnostics_.error(memberSpec.location, "member declaration has no declarator");
                continue;
            }
            for (;;) {
                std::unique_ptr<DeclaratorNode> node = parseDeclaratorNode(false);
                if (!node) {
                    synchronize();
                    break;
                }
                DeclaratorInfo info;
                info.location = node->location;
                info.type = applyDeclaratorNode(*node, memberSpec.type, info);
                if (info.name.empty()) {
                    diagnostics_.error(node->location, "member declaration needs a name");
                }
                int bitWidth = -1;
                if (match(":")) {
                    auto widthExpression = parseConditionalExpression();
                    if (widthExpression) {
                        if (const auto value = evaluateConstant(*widthExpression); value.has_value()) {
                            bitWidth = static_cast<int>(*value);
                        } else {
                            diagnostics_.error(node->location, "bit-field width is not constant");
                        }
                    }
                    diagnostics_.error(node->location,
                                       "bit-fields are not supported by kevinc's KR32 layout");
                }
                record->members.push_back(
                    RecordMember{info.name, info.type, 0, bitWidth, node->location});
                if (match(",")) {
                    continue;
                }
                expect(";");
                break;
            }
            if (index_ == before) {
                advance();
            }
        }
        expect("}");
        layoutRecord(*record);
        record->complete = true;
        return type;
    }

    void layoutRecord(RecordInfo& record) {
        int offset = 0;
        int maximumAlignment = 1;
        for (RecordMember& member : record.members) {
            const std::size_t alignment = std::max<std::size_t>(1, types_.alignOf(member.type));
            const std::size_t size = types_.sizeOf(member.type);
            maximumAlignment = std::max(maximumAlignment, static_cast<int>(alignment));
            if (record.isUnion) {
                member.offset = 0;
                offset = std::max(offset, static_cast<int>(size));
            } else {
                offset = static_cast<int>(alignUp<std::size_t>(static_cast<std::size_t>(offset),
                                                               alignment));
                member.offset = offset;
                offset += static_cast<int>(size);
            }
        }
        record.alignment = maximumAlignment;
        record.size = static_cast<int>(
            alignUp<std::size_t>(static_cast<std::size_t>(offset),
                                 static_cast<std::size_t>(maximumAlignment)));
    }

    [[nodiscard]] TypePtr parseEnumSpecifier() {
        const SourceLocation location = tokens_[index_ - 1].location;
        std::string tag;
        if (peek().kind == TokenKind::Identifier && !isKeyword(peek().text)) {
            tag = advance().text;
        }

        if (!check("{")) {
            if (tag.empty()) {
                diagnostics_.error(location, "expected enum tag");
                return types_.intType();
            }
            if (TypePtr existing = lookupTag(tag); existing != nullptr) {
                if (existing->kind != TypeKind::Enum) {
                    diagnostics_.error(location, concat("tag '", tag, "' is not an enum"));
                }
                return existing;
            }
            auto enumeration = std::make_shared<EnumInfo>();
            enumeration->tag = tag;
            enumeration->location = location;
            TypePtr type = types_.enumType(enumeration);
            scopes_.back().tags[tag] = type;
            return type;
        }

        expect("{");
        std::shared_ptr<EnumInfo> enumeration;
        TypePtr type;
        if (!tag.empty()) {
            const auto existing = scopes_.back().tags.find(tag);
            if (existing != scopes_.back().tags.end() && existing->second->kind == TypeKind::Enum) {
                type = existing->second;
                enumeration = type->enumeration;
                if (enumeration && enumeration->complete) {
                    diagnostics_.error(location, concat("redefinition of enum '", tag, "'"));
                }
            }
        }
        if (!enumeration) {
            enumeration = std::make_shared<EnumInfo>();
            enumeration->tag = tag;
            enumeration->location = location;
            type = types_.enumType(enumeration);
            if (!tag.empty()) {
                scopes_.back().tags[tag] = type;
            }
        }
        enumeration->constants.clear();
        enumeration->complete = false;

        std::int64_t nextValue = 0;
        while (!check("}") && !atEnd()) {
            if (peek().kind != TokenKind::Identifier) {
                diagnostics_.error(peek().location, "expected enumerator name");
                synchronize();
                break;
            }
            const Token name = advance();
            std::int64_t value = nextValue;
            if (match("=")) {
                auto expression = parseConditionalExpression();
                if (expression) {
                    if (const auto evaluated = evaluateConstant(*expression); evaluated.has_value()) {
                        value = *evaluated;
                    } else {
                        diagnostics_.error(name.location, "enumerator value is not constant");
                    }
                }
            }
            enumeration->constants.push_back(EnumConstant{name.text, value, name.location});
            scopes_.back().enumConstants[name.text] = value;
            scopes_.back().ordinaryNames.insert(name.text);
            nextValue = value + 1;
            if (match(",")) {
                continue;
            }
            break;
        }
        expect("}");
        enumeration->complete = true;
        return type;
    }

    [[nodiscard]] std::unique_ptr<DeclaratorNode> parseDeclaratorNode(bool allowAbstract) {
        const SourceLocation location = peek().location;
        std::vector<std::unique_ptr<DeclaratorNode>> pointers;
        while (match("*")) {
            while (checkKeyword("const") || checkKeyword("volatile") || checkKeyword("restrict")) {
                advance();
            }
            auto pointer = std::make_unique<DeclaratorNode>();
            pointer->kind = DeclaratorNode::Kind::Pointer;
            pointer->location = location;
            pointers.push_back(std::move(pointer));
        }

        std::unique_ptr<DeclaratorNode> node = parseDirectDeclaratorNode(allowAbstract);
        if (!node) {
            node = std::make_unique<DeclaratorNode>();
            node->kind = DeclaratorNode::Kind::Name;
            node->location = location;
        }

        for (auto iterator = pointers.rbegin(); iterator != pointers.rend(); ++iterator) {
            (*iterator)->inner = std::move(node);
            node = std::move(*iterator);
        }
        return node;
    }

    [[nodiscard]] std::unique_ptr<DeclaratorNode> parseDirectDeclaratorNode(bool allowAbstract) {
        const SourceLocation location = peek().location;
        std::unique_ptr<DeclaratorNode> node;

        const bool grouped = check("(") && !looksLikeTypeName(1) && !peek(1).isPunct(")") &&
                             !peek(1).isPunct("...");
        if (grouped) {
            advance();
            node = parseDeclaratorNode(allowAbstract);
            expect(")");
        } else if (peek().kind == TokenKind::Identifier && !isKeyword(peek().text)) {
            node = std::make_unique<DeclaratorNode>();
            node->kind = DeclaratorNode::Kind::Name;
            node->name = advance().text;
            node->location = location;
        } else if (allowAbstract) {
            node = std::make_unique<DeclaratorNode>();
            node->kind = DeclaratorNode::Kind::Name;
            node->location = location;
        } else {
            diagnostics_.error(location, "expected declarator");
            return nullptr;
        }

        for (;;) {
            if (match("[")) {
                std::int64_t length = -1;
                if (!check("]")) {
                    while (checkKeyword("static") || checkKeyword("const") || checkKeyword("restrict")) {
                        advance();
                    }
                    auto expression = parseAssignmentExpression();
                    if (expression) {
                        if (const auto value = evaluateConstant(*expression); value.has_value()) {
                            length = *value;
                        } else {
                            diagnostics_.error(expression->location,
                                               "array bound is not an integer constant");
                            length = 1;
                        }
                    }
                }
                expect("]");
                auto array = std::make_unique<DeclaratorNode>();
                array->kind = DeclaratorNode::Kind::Array;
                array->location = location;
                array->arrayLength = length;
                array->inner = std::move(node);
                node = std::move(array);
                continue;
            }
            if (check("(")) {
                ParameterList parameters = parseParameterList();
                auto function = std::make_unique<DeclaratorNode>();
                function->kind = DeclaratorNode::Kind::Function;
                function->location = location;
                function->parameters = std::move(parameters.parameters);
                function->variadic = parameters.variadic;
                function->hasPrototype = parameters.hasPrototype;
                for (const Parameter& parameter : function->parameters) {
                    function->parameterTypes.push_back(parameter.type);
                    function->parameterNames.push_back(parameter.name);
                }
                function->inner = std::move(node);
                node = std::move(function);
                continue;
            }
            break;
        }
        return node;
    }

    [[nodiscard]] ParameterList parseParameterList() {
        ParameterList result;
        expect("(");
        if (match(")")) {
            result.hasPrototype = false;
            return result;
        }

        if (checkKeyword("void") && peek(1).isPunct(")")) {
            advance();
            advance();
            result.hasPrototype = true;
            return result;
        }

        const bool identifierList = peek().kind == TokenKind::Identifier &&
                                    !looksLikeTypeName(0) && !isKeyword(peek().text) &&
                                    (peek(1).isPunct(",") || peek(1).isPunct(")"));
        if (identifierList) {
            result.knr = true;
            result.hasPrototype = false;
            for (;;) {
                if (peek().kind != TokenKind::Identifier) {
                    diagnostics_.error(peek().location, "expected parameter name");
                    break;
                }
                const Token name = advance();
                Parameter parameter;
                parameter.name = name.text;
                parameter.type = types_.intType();
                parameter.location = name.location;
                result.parameters.push_back(std::move(parameter));
                if (match(",")) {
                    continue;
                }
                break;
            }
            expect(")");
            return result;
        }

        for (;;) {
            if (match("...")) {
                result.variadic = true;
                if (result.parameters.empty()) {
                    diagnostics_.error(peek().location,
                                       "variadic parameter list needs a named parameter");
                }
                expect(")");
                break;
            }
            DeclSpec spec = parseDeclSpecifiers();
            std::unique_ptr<DeclaratorNode> node = parseDeclaratorNode(true);
            if (!node) {
                synchronize();
                break;
            }
            DeclaratorInfo info;
            info.location = node->location;
            info.type = applyDeclaratorNode(*node, spec.type, info);
            TypePtr parameterType = info.type;
            if (parameterType && parameterType->isArray()) {
                parameterType = types_.pointerTo(parameterType->element);
            } else if (parameterType && parameterType->isFunction()) {
                parameterType = types_.pointerTo(parameterType);
            }
            Parameter parameter;
            parameter.name = info.name;
            parameter.type = parameterType;
            parameter.location = node->location;
            result.parameters.push_back(std::move(parameter));
            if (match(",")) {
                continue;
            }
            expect(")");
            break;
        }
        return result;
    }

    [[nodiscard]] TypePtr applyDeclaratorNode(const DeclaratorNode& node, TypePtr base,
                                              DeclaratorInfo& info) {
        switch (node.kind) {
        case DeclaratorNode::Kind::Name:
            info.name = node.name;
            return base;
        case DeclaratorNode::Kind::Pointer:
            if (!node.inner) {
                return base;
            }
            return applyDeclaratorNode(*node.inner, types_.pointerTo(base), info);
        case DeclaratorNode::Kind::Array:
            if (!node.inner) {
                return base;
            }
            return applyDeclaratorNode(*node.inner, types_.arrayOf(base, node.arrayLength), info);
        case DeclaratorNode::Kind::Function:
            if (!node.inner) {
                return base;
            }
            return applyDeclaratorNode(
                *node.inner,
                types_.functionOf(base, node.parameterTypes, node.parameterNames, node.variadic,
                                  node.hasPrototype),
                info);
        }
        return base;
    }

    void collectDefinitionParameters(const DeclaratorNode& node, DeclaratorInfo& info) {
        if (node.kind == DeclaratorNode::Kind::Function && node.inner &&
            node.inner->kind == DeclaratorNode::Kind::Name) {
            info.parameters = node.parameters;
            return;
        }
        if (node.inner) {
            collectDefinitionParameters(*node.inner, info);
        }
    }

    void parseKnRParameterDeclarations(DeclaratorInfo& info) {
        while (!check("{") && !atEnd() && isDeclarationStart()) {
            std::unique_ptr<Declaration> declaration = parseDeclaration(false, false);
            if (!declaration) {
                break;
            }
            for (const DeclaratorInfo& declarator : declaration->declarators) {
                for (Parameter& parameter : info.parameters) {
                    if (parameter.name == declarator.name) {
                        parameter.type = declarator.type;
                    }
                }
            }
        }
        std::vector<TypePtr> parameterTypes;
        std::vector<std::string> parameterNames;
        for (const Parameter& parameter : info.parameters) {
            parameterTypes.push_back(parameter.type);
            parameterNames.push_back(parameter.name);
        }
        if (info.type) {
            info.type = types_.functionOf(info.type->returnType, std::move(parameterTypes),
                                          std::move(parameterNames), false, false);
        }
    }

    [[nodiscard]] std::unique_ptr<Stmt> parseFunctionBody(DeclaratorInfo& info) {
        if (!check("{")) {
            diagnostics_.error(peek().location, "expected function body");
            return nullptr;
        }
        return parseCompoundStatement(true, &info.parameters);
    }

    [[nodiscard]] std::unique_ptr<Initializer> parseInitializer() {
        auto initializer = std::make_unique<Initializer>();
        initializer->location = peek().location;
        if (match("{")) {
            initializer->isList = true;
            while (!check("}") && !atEnd()) {
                initializer->elements.push_back(parseInitializer());
                if (match(",")) {
                    continue;
                }
                break;
            }
            expect("}");
            return initializer;
        }
        initializer->expression = parseAssignmentExpression();
        return initializer;
    }

    // -- statements ---------------------------------------------------------

    [[nodiscard]] std::unique_ptr<Stmt> parseCompoundStatement(
        bool createScope, const std::vector<Parameter>* parameters = nullptr) {
        const SourceLocation location = peek().location;
        if (!expect("{")) {
            return nullptr;
        }
        if (createScope) {
            pushScope();
            if (parameters != nullptr) {
                for (const Parameter& parameter : *parameters) {
                    if (!parameter.name.empty()) {
                        scopes_.back().ordinaryNames.insert(parameter.name);
                    }
                }
            }
        }

        auto statement = std::make_unique<Stmt>();
        statement->kind = Stmt::Kind::Compound;
        statement->location = location;

        bool sawStatement = false;
        while (!check("}") && !atEnd()) {
            const std::size_t before = index_;
            if (isDeclarationStart()) {
                if (sawStatement) {
                    diagnostics_.warning(peek().location,
                                         "declaration after statement is not C89");
                }
                std::unique_ptr<Declaration> declaration = parseDeclaration(false, true);
                if (declaration) {
                    auto child = std::make_unique<Stmt>();
                    child->kind = Stmt::Kind::Declaration;
                    child->location = declaration->location;
                    child->declaration = std::move(declaration);
                    statement->children.push_back(std::move(child));
                }
            } else {
                std::unique_ptr<Stmt> child = parseStatement();
                if (child) {
                    if (child->kind != Stmt::Kind::Empty) {
                        sawStatement = true;
                    }
                    statement->children.push_back(std::move(child));
                }
            }
            if (index_ == before) {
                diagnostics_.error(peek().location, "unable to parse statement");
                advance();
            }
        }
        expect("}");
        if (createScope) {
            popScope();
        }
        return statement;
    }

    [[nodiscard]] std::unique_ptr<Stmt> parseStatement() {
        const SourceLocation location = peek().location;

        if (check("{")) {
            return parseCompoundStatement(true);
        }
        if (match(";")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::Empty;
            statement->location = location;
            return statement;
        }
        if (matchKeyword("if")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::If;
            statement->location = location;
            expect("(");
            statement->condition = parseExpression();
            expect(")");
            statement->body = parseStatement();
            if (matchKeyword("else")) {
                statement->elseBody = parseStatement();
            }
            return statement;
        }
        if (matchKeyword("while")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::While;
            statement->location = location;
            expect("(");
            statement->condition = parseExpression();
            expect(")");
            statement->body = parseStatement();
            return statement;
        }
        if (matchKeyword("do")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::DoWhile;
            statement->location = location;
            statement->body = parseStatement();
            expectKeyword("while");
            expect("(");
            statement->condition = parseExpression();
            expect(")");
            expect(";");
            return statement;
        }
        if (matchKeyword("for")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::For;
            statement->location = location;
            expect("(");
            if (isDeclarationStart()) {
                std::unique_ptr<Declaration> declaration = parseDeclaration(false, true);
                auto init = std::make_unique<Stmt>();
                init->kind = Stmt::Kind::Declaration;
                init->location = declaration ? declaration->location : location;
                init->declaration = std::move(declaration);
                statement->initStatement = std::move(init);
            } else if (!check(";")) {
                auto init = std::make_unique<Stmt>();
                init->kind = Stmt::Kind::Expression;
                init->location = peek().location;
                init->expression = parseExpression();
                statement->initStatement = std::move(init);
                expect(";");
            } else {
                expect(";");
            }
            if (!check(";")) {
                statement->condition = parseExpression();
            }
            expect(";");
            if (!check(")")) {
                statement->increment = parseExpression();
            }
            expect(")");
            statement->body = parseStatement();
            return statement;
        }
        if (matchKeyword("switch")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::Switch;
            statement->location = location;
            expect("(");
            statement->expression = parseExpression();
            expect(")");
            statement->body = parseStatement();
            return statement;
        }
        if (matchKeyword("case")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::Case;
            statement->location = location;
            statement->expression = parseConditionalExpression();
            expect(":");
            if (statement->expression) {
                if (const auto value = evaluateConstant(*statement->expression); value.has_value()) {
                    statement->caseValue = *value;
                }
            }
            statement->body = parseStatement();
            return statement;
        }
        if (matchKeyword("default")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::Default;
            statement->location = location;
            expect(":");
            statement->body = parseStatement();
            return statement;
        }
        if (matchKeyword("break")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::Break;
            statement->location = location;
            expect(";");
            return statement;
        }
        if (matchKeyword("continue")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::Continue;
            statement->location = location;
            expect(";");
            return statement;
        }
        if (matchKeyword("return")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::Return;
            statement->location = location;
            if (!check(";")) {
                statement->expression = parseExpression();
            }
            expect(";");
            return statement;
        }
        if (matchKeyword("goto")) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::Goto;
            statement->location = location;
            if (peek().kind == TokenKind::Identifier) {
                statement->label = advance().text;
            } else {
                diagnostics_.error(peek().location, "expected label name after goto");
            }
            expect(";");
            return statement;
        }
        if (peek().kind == TokenKind::Identifier && peek(1).isPunct(":") &&
            !isKeyword(peek().text)) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::Label;
            statement->location = location;
            statement->label = advance().text;
            ++index_; // ':'
            statement->body = parseStatement();
            return statement;
        }
        if (isDeclarationStart()) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = Stmt::Kind::Declaration;
            statement->location = location;
            statement->declaration = parseDeclaration(false, true);
            return statement;
        }

        auto statement = std::make_unique<Stmt>();
        statement->kind = Stmt::Kind::Expression;
        statement->location = location;
        statement->expression = parseExpression();
        expect(";");
        return statement;
    }

    // -- expressions --------------------------------------------------------

    [[nodiscard]] std::unique_ptr<Expr> parseExpression() {
        auto expression = parseAssignmentExpression();
        while (match(",")) {
            auto comma = std::make_unique<Expr>();
            comma->kind = Expr::Kind::Comma;
            comma->location = expression ? expression->location : peek().location;
            comma->left = std::move(expression);
            comma->right = parseAssignmentExpression();
            expression = std::move(comma);
        }
        return expression;
    }

    [[nodiscard]] std::unique_ptr<Expr> parseAssignmentExpression() {
        auto left = parseConditionalExpression();
        static const std::array<std::pair<std::string_view, AssignOp>, 11> assignments = {
            std::pair{"=", AssignOp::Assign},          std::pair{"+=", AssignOp::AddAssign},
            std::pair{"-=", AssignOp::SubtractAssign}, std::pair{"*=", AssignOp::MultiplyAssign},
            std::pair{"/=", AssignOp::DivideAssign},   std::pair{"%=", AssignOp::ModuloAssign},
            std::pair{"<<=", AssignOp::ShiftLeftAssign},
            std::pair{">>=", AssignOp::ShiftRightAssign},
            std::pair{"&=", AssignOp::AndAssign},      std::pair{"|=", AssignOp::OrAssign},
            std::pair{"^=", AssignOp::XorAssign}};
        for (const auto& [spelling, operation] : assignments) {
            if (check(spelling)) {
                const SourceLocation location = peek().location;
                advance();
                auto assignment = std::make_unique<Expr>();
                assignment->kind = Expr::Kind::Assignment;
                assignment->location = location;
                assignment->assignOp = operation;
                assignment->left = std::move(left);
                assignment->right = parseAssignmentExpression();
                return assignment;
            }
        }
        return left;
    }

    [[nodiscard]] std::unique_ptr<Expr> parseConditionalExpression() {
        auto condition = parseBinaryExpression(1);
        if (match("?")) {
            auto conditional = std::make_unique<Expr>();
            conditional->kind = Expr::Kind::Conditional;
            conditional->location = condition ? condition->location : peek().location;
            conditional->left = std::move(condition);
            conditional->right = parseExpression();
            expect(":");
            conditional->third = parseConditionalExpression();
            return conditional;
        }
        return condition;
    }

    struct BinaryInfo {
        BinaryOp operation;
        int precedence;
    };

    [[nodiscard]] static std::optional<BinaryInfo> binaryInfo(std::string_view text) noexcept {
        if (text == "||") {
            return BinaryInfo{BinaryOp::LogicalOr, 1};
        }
        if (text == "&&") {
            return BinaryInfo{BinaryOp::LogicalAnd, 2};
        }
        if (text == "|") {
            return BinaryInfo{BinaryOp::BitwiseOr, 3};
        }
        if (text == "^") {
            return BinaryInfo{BinaryOp::BitwiseXor, 4};
        }
        if (text == "&") {
            return BinaryInfo{BinaryOp::BitwiseAnd, 5};
        }
        if (text == "==") {
            return BinaryInfo{BinaryOp::Equal, 6};
        }
        if (text == "!=") {
            return BinaryInfo{BinaryOp::NotEqual, 6};
        }
        if (text == "<") {
            return BinaryInfo{BinaryOp::Less, 7};
        }
        if (text == ">") {
            return BinaryInfo{BinaryOp::Greater, 7};
        }
        if (text == "<=") {
            return BinaryInfo{BinaryOp::LessEqual, 7};
        }
        if (text == ">=") {
            return BinaryInfo{BinaryOp::GreaterEqual, 7};
        }
        if (text == "<<") {
            return BinaryInfo{BinaryOp::ShiftLeft, 8};
        }
        if (text == ">>") {
            return BinaryInfo{BinaryOp::ShiftRight, 8};
        }
        if (text == "+") {
            return BinaryInfo{BinaryOp::Add, 9};
        }
        if (text == "-") {
            return BinaryInfo{BinaryOp::Subtract, 9};
        }
        if (text == "*") {
            return BinaryInfo{BinaryOp::Multiply, 10};
        }
        if (text == "/") {
            return BinaryInfo{BinaryOp::Divide, 10};
        }
        if (text == "%") {
            return BinaryInfo{BinaryOp::Modulo, 10};
        }
        return std::nullopt;
    }

    [[nodiscard]] std::unique_ptr<Expr> parseBinaryExpression(int minimumPrecedence) {
        auto left = parseUnaryExpression();
        while (!atEnd() && peek().kind == TokenKind::Punctuator) {
            const auto info = binaryInfo(peek().text);
            if (!info.has_value() || info->precedence < minimumPrecedence) {
                break;
            }
            const SourceLocation location = peek().location;
            advance();
            auto right = parseBinaryExpression(info->precedence + 1);
            auto binary = std::make_unique<Expr>();
            binary->kind = Expr::Kind::Binary;
            binary->location = location;
            binary->binaryOp = info->operation;
            binary->left = std::move(left);
            binary->right = std::move(right);
            left = std::move(binary);
        }
        return left;
    }

    [[nodiscard]] std::unique_ptr<Expr> parseUnaryExpression() {
        const SourceLocation location = peek().location;
        if (match("++")) {
            auto expression = std::make_unique<Expr>();
            expression->kind = Expr::Kind::Unary;
            expression->location = location;
            expression->unaryOp = UnaryOp::PreIncrement;
            expression->left = parseUnaryExpression();
            return expression;
        }
        if (match("--")) {
            auto expression = std::make_unique<Expr>();
            expression->kind = Expr::Kind::Unary;
            expression->location = location;
            expression->unaryOp = UnaryOp::PreDecrement;
            expression->left = parseUnaryExpression();
            return expression;
        }
        const std::array<std::pair<std::string_view, UnaryOp>, 6> unaryOperators = {
            std::pair{"&", UnaryOp::AddressOf}, std::pair{"*", UnaryOp::Dereference},
            std::pair{"+", UnaryOp::Plus},      std::pair{"-", UnaryOp::Minus},
            std::pair{"~", UnaryOp::BitwiseNot}, std::pair{"!", UnaryOp::LogicalNot}};
        for (const auto& [spelling, operation] : unaryOperators) {
            if (check(spelling)) {
                advance();
                auto expression = std::make_unique<Expr>();
                expression->kind = Expr::Kind::Unary;
                expression->location = location;
                expression->unaryOp = operation;
                expression->left = parseUnaryExpression();
                return expression;
            }
        }
        if (matchKeyword("sizeof")) {
            auto expression = std::make_unique<Expr>();
            expression->kind = Expr::Kind::SizeOf;
            expression->location = location;
            if (check("(") && looksLikeTypeName(1)) {
                advance();
                expression->sizeofType = parseTypeName();
                expect(")");
            } else {
                expression->left = parseUnaryExpression();
            }
            return expression;
        }
        if (check("(") && looksLikeTypeName(1)) {
            const std::size_t saved = index_;
            advance();
            TypePtr type = parseTypeName();
            if (match(")")) {
                auto expression = std::make_unique<Expr>();
                expression->kind = Expr::Kind::Cast;
                expression->location = location;
                expression->castType = std::move(type);
                expression->left = parseUnaryExpression();
                return expression;
            }
            index_ = saved;
        }
        return parsePostfixExpression();
    }

    [[nodiscard]] std::unique_ptr<Expr> parsePostfixExpression() {
        auto expression = parsePrimaryExpression();
        for (;;) {
            if (match("[")) {
                auto indexExpression = std::make_unique<Expr>();
                indexExpression->kind = Expr::Kind::Index;
                indexExpression->location = expression ? expression->location : peek().location;
                indexExpression->left = std::move(expression);
                indexExpression->right = parseExpression();
                expect("]");
                expression = std::move(indexExpression);
                continue;
            }
            if (match("(")) {
                auto call = std::make_unique<Expr>();
                call->kind = Expr::Kind::Call;
                call->location = expression ? expression->location : peek().location;
                call->left = std::move(expression);
                if (!check(")")) {
                    for (;;) {
                        call->arguments.push_back(parseAssignmentExpression());
                        if (match(",")) {
                            continue;
                        }
                        break;
                    }
                }
                expect(")");
                expression = std::move(call);
                continue;
            }
            if (match(".") || match("->")) {
                const bool arrow = tokens_[index_ - 1].isPunct("->");
                auto member = std::make_unique<Expr>();
                member->kind = Expr::Kind::Member;
                member->location = expression ? expression->location : peek().location;
                member->isArrow = arrow;
                member->left = std::move(expression);
                if (peek().kind == TokenKind::Identifier) {
                    member->memberName = advance().text;
                } else {
                    diagnostics_.error(peek().location, "expected member name");
                }
                expression = std::move(member);
                continue;
            }
            if (match("++") || match("--")) {
                const bool increment = tokens_[index_ - 1].isPunct("++");
                auto unary = std::make_unique<Expr>();
                unary->kind = Expr::Kind::Unary;
                unary->location = expression ? expression->location : peek().location;
                unary->unaryOp = increment ? UnaryOp::PostIncrement : UnaryOp::PostDecrement;
                unary->left = std::move(expression);
                expression = std::move(unary);
                continue;
            }
            break;
        }
        return expression;
    }

    [[nodiscard]] std::unique_ptr<Expr> parsePrimaryExpression() {
        const SourceLocation location = peek().location;
        if (atEnd()) {
            diagnostics_.error(location, "unexpected end of input in expression");
            auto expression = std::make_unique<Expr>();
            expression->kind = Expr::Kind::IntegerLiteral;
            expression->location = location;
            return expression;
        }
        if (peek().kind == TokenKind::Number) {
            const Token token = advance();
            return parseNumberLiteral(token);
        }
        if (peek().kind == TokenKind::CharLiteral) {
            const Token token = advance();
            auto expression = std::make_unique<Expr>();
            expression->kind = Expr::Kind::CharacterLiteral;
            expression->location = token.location;
            if (const auto value = literals::charValue(token, diagnostics_); value.has_value()) {
                expression->integerValue = *value;
            }
            return expression;
        }
        if (peek().kind == TokenKind::StringLiteral) {
            auto expression = std::make_unique<Expr>();
            expression->kind = Expr::Kind::StringLiteral;
            expression->location = location;
            while (peek().kind == TokenKind::StringLiteral) {
                const Token token = advance();
                if (const auto value = literals::stringValue(token, diagnostics_); value.has_value()) {
                    expression->stringValue += *value;
                }
            }
            return expression;
        }
        if (peek().kind == TokenKind::Identifier && !isKeyword(peek().text)) {
            auto expression = std::make_unique<Expr>();
            expression->kind = Expr::Kind::Identifier;
            expression->location = location;
            expression->name = advance().text;
            return expression;
        }
        if (match("(")) {
            auto expression = parseExpression();
            expect(")");
            return expression;
        }
        diagnostics_.error(location, concat("unexpected token '", peek().text, "' in expression"));
        auto expression = std::make_unique<Expr>();
        expression->kind = Expr::Kind::IntegerLiteral;
        expression->location = location;
        advance();
        return expression;
    }

    [[nodiscard]] std::unique_ptr<Expr> parseNumberLiteral(const Token& token) {
        auto expression = std::make_unique<Expr>();
        expression->location = token.location;
        const std::string& text = token.text;
        const bool hexadecimal = text.size() > 1 && text[0] == '0' &&
                                 (text[1] == 'x' || text[1] == 'X');
        bool floating = text.find('.') != std::string::npos;
        if (!floating) {
            for (std::size_t index = hexadecimal ? 2 : 0; index < text.size(); ++index) {
                const char ch = text[index];
                if ((ch == 'e' || ch == 'E') && !hexadecimal) {
                    floating = true;
                    break;
                }
                if ((ch == 'p' || ch == 'P') && hexadecimal) {
                    floating = true;
                    break;
                }
            }
        }
        if (floating) {
            expression->kind = Expr::Kind::FloatingLiteral;
            std::string body = text;
            bool isFloat = false;
            bool isLongDouble = false;
            if (!body.empty() && (body.back() == 'f' || body.back() == 'F')) {
                isFloat = true;
                body.pop_back();
            } else if (!body.empty() && (body.back() == 'l' || body.back() == 'L')) {
                isLongDouble = true;
                body.pop_back();
            }
            char* end = nullptr;
            expression->floatingValue = std::strtold(body.c_str(), &end);
            if (end == nullptr || *end != '\0') {
                diagnostics_.error(token.location, concat("invalid floating literal '", text, "'"));
            }
            expression->type = isFloat ? types_.floatType()
                                       : (isLongDouble ? types_.longDoubleType() : types_.doubleType());
            return expression;
        }

        expression->kind = Expr::Kind::IntegerLiteral;
        std::string body = text;
        bool unsignedSuffix = false;
        while (!body.empty()) {
            const char last = body.back();
            if (last == 'u' || last == 'U') {
                unsignedSuffix = true;
                body.pop_back();
                continue;
            }
            if (last == 'l' || last == 'L') {
                body.pop_back();
                continue;
            }
            break;
        }
        int base = 10;
        if (body.size() > 1 && body[0] == '0' && (body[1] == 'x' || body[1] == 'X')) {
            base = 16;
            body = body.substr(2);
        } else if (body.size() > 1 && body[0] == '0') {
            base = 8;
            body = body.substr(1);
        }
        std::uint64_t value = 0;
        for (const char ch : body) {
            int digit = -1;
            if (base == 16) {
                digit = hexValue(ch);
            } else if (ch >= '0' && ch <= '9') {
                digit = ch - '0';
            }
            if (digit < 0 || digit >= base) {
                diagnostics_.error(token.location, concat("invalid integer literal '", text, "'"));
                break;
            }
            value = value * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(digit);
        }
        expression->integerValue = static_cast<std::int64_t>(value);
        expression->integerUnsigned = unsignedSuffix || value > 0x7FFFFFFFU;
        return expression;
    }

    [[nodiscard]] TypePtr parseTypeName() {
        DeclSpec spec = parseDeclSpecifiers();
        if (spec.storage != StorageClass::None) {
            diagnostics_.error(spec.location, "storage class is not allowed in a type name");
        }
        std::unique_ptr<DeclaratorNode> node = parseDeclaratorNode(true);
        DeclaratorInfo info;
        if (!node) {
            return spec.type;
        }
        return applyDeclaratorNode(*node, spec.type, info);
    }

    // -- constant evaluation ------------------------------------------------

    [[nodiscard]] std::optional<std::int64_t> evaluateConstant(const Expr& expression) const {
        switch (expression.kind) {
        case Expr::Kind::IntegerLiteral:
        case Expr::Kind::CharacterLiteral:
            return expression.integerValue;
        case Expr::Kind::Identifier:
            return lookupEnumConstant(expression.name);
        case Expr::Kind::Unary: {
            if (!expression.left) {
                return std::nullopt;
            }
            const auto value = evaluateConstant(*expression.left);
            if (!value.has_value()) {
                return std::nullopt;
            }
            switch (expression.unaryOp) {
            case UnaryOp::Plus:
                return *value;
            case UnaryOp::Minus:
                return -*value;
            case UnaryOp::LogicalNot:
                return *value == 0 ? 1 : 0;
            case UnaryOp::BitwiseNot:
                return ~*value;
            default:
                return std::nullopt;
            }
        }
        case Expr::Kind::Binary: {
            if (!expression.left || !expression.right) {
                return std::nullopt;
            }
            const auto left = evaluateConstant(*expression.left);
            const auto right = evaluateConstant(*expression.right);
            if (!left.has_value() || !right.has_value()) {
                return std::nullopt;
            }
            switch (expression.binaryOp) {
            case BinaryOp::Add:
                return *left + *right;
            case BinaryOp::Subtract:
                return *left - *right;
            case BinaryOp::Multiply:
                return *left * *right;
            case BinaryOp::Divide:
                return *right == 0 ? std::nullopt
                                   : std::optional<std::int64_t>(*left / *right);
            case BinaryOp::Modulo:
                return *right == 0 ? std::nullopt
                                   : std::optional<std::int64_t>(*left % *right);
            case BinaryOp::ShiftLeft:
                return *left << (*right & 31);
            case BinaryOp::ShiftRight:
                return *left >> (*right & 31);
            case BinaryOp::Less:
                return *left < *right ? 1 : 0;
            case BinaryOp::Greater:
                return *left > *right ? 1 : 0;
            case BinaryOp::LessEqual:
                return *left <= *right ? 1 : 0;
            case BinaryOp::GreaterEqual:
                return *left >= *right ? 1 : 0;
            case BinaryOp::Equal:
                return *left == *right ? 1 : 0;
            case BinaryOp::NotEqual:
                return *left != *right ? 1 : 0;
            case BinaryOp::BitwiseAnd:
                return *left & *right;
            case BinaryOp::BitwiseOr:
                return *left | *right;
            case BinaryOp::BitwiseXor:
                return *left ^ *right;
            case BinaryOp::LogicalAnd:
                return (*left != 0 && *right != 0) ? 1 : 0;
            case BinaryOp::LogicalOr:
                return (*left != 0 || *right != 0) ? 1 : 0;
            }
            return std::nullopt;
        }
        case Expr::Kind::Conditional: {
            if (!expression.left) {
                return std::nullopt;
            }
            const auto condition = evaluateConstant(*expression.left);
            if (!condition.has_value()) {
                return std::nullopt;
            }
            if (*condition != 0) {
                return expression.right ? evaluateConstant(*expression.right) : std::nullopt;
            }
            return expression.third ? evaluateConstant(*expression.third) : std::nullopt;
        }
        case Expr::Kind::Cast: {
            if (!expression.left) {
                return std::nullopt;
            }
            return evaluateConstant(*expression.left);
        }
        case Expr::Kind::SizeOf: {
            if (expression.sizeofType) {
                return static_cast<std::int64_t>(types_.sizeOf(expression.sizeofType));
            }
            if (expression.left && expression.left->kind == Expr::Kind::StringLiteral) {
                return static_cast<std::int64_t>(expression.left->stringValue.size() + 1);
            }
            return std::nullopt;
        }
        case Expr::Kind::Comma:
            return expression.right ? evaluateConstant(*expression.right) : std::nullopt;
        default:
            return std::nullopt;
        }
    }

    std::vector<Token> tokens_;
    TypeContext& types_;
    Diagnostics& diagnostics_;
    std::size_t index_ = 0;
    std::vector<ParserScope> scopes_;
};

// ---------------------------------------------------------------------------
// Semantic analysis
// ---------------------------------------------------------------------------

enum class SymbolKind {
    Variable,
    Function,
    EnumConstant
};

struct Symbol {
    std::string name;
    std::string asmName;
    SymbolKind kind = SymbolKind::Variable;
    TypePtr type;
    StorageClass storage = StorageClass::None;
    SourceLocation location;
    bool isGlobal = true;
    bool isStatic = false;
    bool isDefined = false;
    bool isFunction = false;
    int frameOffset = 0;
    int frameSize = 0;
    std::int64_t enumValue = 0;
    const Initializer* initializer = nullptr;
    const DeclaratorInfo* declarator = nullptr;
};

struct FunctionDefinition {
    Symbol* symbol = nullptr;
    DeclaratorInfo* declarator = nullptr;
};

struct SemanticProgram {
    std::vector<Symbol*> globals;
    std::vector<Symbol*> externals;
    std::vector<FunctionDefinition> functions;
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer(TranslationUnit& unit, TypeContext& types, Diagnostics& diagnostics)
        : unit_(unit), types_(types), diagnostics_(diagnostics) {
        scopes_.emplace_back();
    }

    [[nodiscard]] SemanticProgram analyze() {
        collectGlobals();
        for (std::unique_ptr<Declaration>& declaration : unit_.declarations) {
            if (!declaration || declaration->isTypedef) {
                continue;
            }
            for (DeclaratorInfo& declarator : declaration->declarators) {
                if (!declarator.type) {
                    continue;
                }
                if (declarator.type->isFunction()) {
                    if (declarator.body) {
                        analyzeFunction(declarator, declarator.symbol);
                    }
                } else if (declarator.initializer) {
                    analyzeInitializer(*declarator.initializer, declarator.type, true);
                }
            }
        }
        return std::move(program_);
    }

private:
    struct FunctionContext {
        Symbol* symbol = nullptr;
        TypePtr returnType;
        std::set<std::string> labels;
        std::set<std::string> gotos;
    };

    // -- symbols and scopes -------------------------------------------------

    [[nodiscard]] std::unique_ptr<Symbol> makeSymbol(std::string name, TypePtr type,
                                                     SourceLocation location) {
        auto symbol = std::make_unique<Symbol>();
        symbol->name = std::move(name);
        symbol->asmName = symbol->name;
        symbol->type = std::move(type);
        symbol->location = location;
        symbol->isFunction = symbol->type && symbol->type->isFunction();
        symbol->kind = symbol->isFunction ? SymbolKind::Function : SymbolKind::Variable;
        return symbol;
    }

    [[nodiscard]] Symbol* keep(std::unique_ptr<Symbol> symbol, bool global) {
        Symbol* raw = symbol.get();
        raw->isGlobal = global;
        symbols_.push_back(std::move(symbol));
        return raw;
    }

    void pushScope() { scopes_.emplace_back(); }
    void popScope() {
        if (scopes_.size() > 1) {
            scopes_.pop_back();
        }
    }

    [[nodiscard]] Symbol* lookup(std::string_view name) const {
        for (auto iterator = scopes_.rbegin(); iterator != scopes_.rend(); ++iterator) {
            const auto found = iterator->find(std::string(name));
            if (found != iterator->end()) {
                return found->second;
            }
        }
        return nullptr;
    }

    void declareLocal(std::string_view name, Symbol* symbol) {
        if (name.empty()) {
            return;
        }
        auto& scope = scopes_.back();
        const auto found = scope.find(std::string(name));
        if (found != scope.end() && found->second != symbol) {
            diagnostics_.error(symbol->location, concat("redefinition of '", name, "'"));
            return;
        }
        scope[std::string(name)] = symbol;
    }

    [[nodiscard]] Symbol* createGlobal(std::string name, TypePtr type, StorageClass storage,
                                       SourceLocation location) {
        auto& globalScope = scopes_.front();
        const auto found = globalScope.find(name);
        if (found != globalScope.end()) {
            Symbol* existing = found->second;
            if (!TypeContext::compatibleType(existing->type, type) &&
                !TypeContext::sameType(existing->type, type)) {
                diagnostics_.error(location, concat("conflicting types for '", name, "'"));
            }
            if (storage == StorageClass::Static) {
                existing->isStatic = true;
            }
            return existing;
        }
        std::unique_ptr<Symbol> symbol = makeSymbol(name, type, location);
        symbol->storage = storage;
        symbol->isStatic = storage == StorageClass::Static;
        Symbol* raw = keep(std::move(symbol), true);
        globalScope[name] = raw;
        return raw;
    }

    [[nodiscard]] Symbol* createStaticLocal(std::string name, TypePtr type, SourceLocation location) {
        std::unique_ptr<Symbol> symbol = makeSymbol(name, type, location);
        symbol->isGlobal = true;
        symbol->isStatic = true;
        symbol->isDefined = true;
        symbol->asmName = concat("__static_", name, "_", staticLocalCounter_++);
        Symbol* raw = keep(std::move(symbol), true);
        program_.globals.push_back(raw);
        return raw;
    }

    [[nodiscard]] Symbol* createLocal(std::string name, TypePtr type, SourceLocation location) {
        std::unique_ptr<Symbol> symbol = makeSymbol(name, type, location);
        symbol->isGlobal = false;
        Symbol* raw = keep(std::move(symbol), false);
        raw->frameOffset = allocateLocal(type);
        return raw;
    }

    [[nodiscard]] int allocateLocal(TypePtr type) {
        const std::size_t alignment = std::min<std::size_t>(
            8, std::max<std::size_t>(1, types_.alignOf(type)));
        const std::size_t size = types_.sizeOf(type);
        if (!types_.isComplete(type)) {
            diagnostics_.error(currentFunction_ && currentFunction_->symbol
                                   ? currentFunction_->symbol->location
                                   : SourceLocation{},
                               concat("incomplete type '", types_.describe(type),
                                      "' cannot be a local variable"));
        }
        frameSize_ = static_cast<int>(
            alignUp<std::size_t>(static_cast<std::size_t>(frameSize_), alignment));
        const int offset = frameSize_;
        frameSize_ += static_cast<int>(size);
        return offset;
    }

    // -- collection pass ----------------------------------------------------

    void collectGlobals() {
        for (std::unique_ptr<Declaration>& declaration : unit_.declarations) {
            if (!declaration || declaration->isTypedef) {
                continue;
            }
            registerEnumConstants(declaration->baseType, scopes_.front());
            for (DeclaratorInfo& declarator : declaration->declarators) {
                if (declarator.name.empty() || !declarator.type) {
                    continue;
                }
                if (declarator.type->isFunction()) {
                    collectFunction(*declaration, declarator);
                } else {
                    collectVariable(*declaration, declarator);
                }
            }
        }
    }

    void registerEnumConstants(TypePtr type,
                               std::map<std::string, Symbol*>& scope) {
        if (!type || type->kind != TypeKind::Enum || !type->enumeration) {
            return;
        }
        for (const EnumConstant& constant : type->enumeration->constants) {
            if (scope.count(constant.name) != 0) {
                continue;
            }
            std::unique_ptr<Symbol> symbol = makeSymbol(constant.name, types_.intType(),
                                                        constant.location);
            symbol->kind = SymbolKind::EnumConstant;
            symbol->isGlobal = true;
            symbol->enumValue = constant.value;
            Symbol* raw = keep(std::move(symbol), true);
            scope[constant.name] = raw;
        }
    }

    void collectFunction(const Declaration& declaration, DeclaratorInfo& declarator) {
        Symbol* symbol = createGlobal(declarator.name, declarator.type, declaration.storage,
                                      declarator.location);
        symbol->isFunction = true;
        symbol->kind = SymbolKind::Function;
        symbol->declarator = &declarator;
        declarator.symbol = symbol;
        if (declarator.body) {
            if (symbol->isDefined) {
                diagnostics_.error(declarator.location,
                                   concat("redefinition of function '", declarator.name, "'"));
            }
            symbol->isDefined = true;
            symbol->type = declarator.type;
            program_.functions.push_back(FunctionDefinition{symbol, &declarator});
        } else if (!symbol->isDefined) {
            if (std::find(program_.externals.begin(), program_.externals.end(), symbol) ==
                program_.externals.end()) {
                program_.externals.push_back(symbol);
            }
        }
    }

    void collectVariable(const Declaration& declaration, DeclaratorInfo& declarator) {
        Symbol* symbol = createGlobal(declarator.name, declarator.type, declaration.storage,
                                      declarator.location);
        symbol->declarator = &declarator;
        declarator.symbol = symbol;
        const bool hasInitializer = declarator.initializer != nullptr;
        const bool isDefinition = hasInitializer || declaration.storage != StorageClass::Extern;
        if (!isDefinition) {
            if (std::find(program_.externals.begin(), program_.externals.end(), symbol) ==
                program_.externals.end()) {
                program_.externals.push_back(symbol);
            }
            return;
        }
        if (symbol->isDefined && (symbol->initializer != nullptr || hasInitializer)) {
            diagnostics_.error(declarator.location,
                               concat("redefinition of '", declarator.name, "'"));
        }
        symbol->isDefined = true;
        if (hasInitializer) {
            symbol->initializer = declarator.initializer.get();
        }
        if (std::find(program_.globals.begin(), program_.globals.end(), symbol) ==
            program_.globals.end()) {
            program_.globals.push_back(symbol);
        }
    }

    // -- functions ----------------------------------------------------------

    void analyzeFunction(DeclaratorInfo& declarator, Symbol* symbol) {
        if (!symbol || !declarator.type || !declarator.type->isFunction() || !declarator.body) {
            return;
        }
        FunctionContext context;
        context.symbol = symbol;
        context.returnType = declarator.type->returnType;
        currentFunction_ = &context;
        frameSize_ = 0;
        loopDepth_ = 0;
        breakDepth_ = 0;
        switchCases_.clear();
        switchDefaults_.clear();

        pushScope();
        for (Parameter& parameter : declarator.parameters) {
            Symbol* local = createLocal(parameter.name, parameter.type, parameter.location);
            parameter.symbol = local;
            if (!parameter.name.empty()) {
                declareLocal(parameter.name, local);
            }
        }
        analyzeStatement(*declarator.body, true);
        for (const std::string& label : context.gotos) {
            if (context.labels.count(label) == 0) {
                diagnostics_.error(symbol->location, concat("goto to undefined label '", label, "'"));
            }
        }
        popScope();

        symbol->frameSize =
            static_cast<int>(alignUp<std::size_t>(static_cast<std::size_t>(frameSize_), 8));
        currentFunction_ = nullptr;
    }

    // -- statements ---------------------------------------------------------

    void analyzeStatement(Stmt& statement, bool reuseScope = false) {
        switch (statement.kind) {
        case Stmt::Kind::Empty:
            return;
        case Stmt::Kind::Compound: {
            if (!reuseScope) {
                pushScope();
            }
            for (std::unique_ptr<Stmt>& child : statement.children) {
                if (child) {
                    analyzeStatement(*child);
                }
            }
            if (!reuseScope) {
                popScope();
            }
            return;
        }
        case Stmt::Kind::Declaration:
            if (statement.declaration) {
                analyzeLocalDeclaration(*statement.declaration);
            }
            return;
        case Stmt::Kind::Expression:
            if (statement.expression) {
                analyzeExpression(statement.expression);
            }
            return;
        case Stmt::Kind::If:
            if (statement.condition) {
                analyzeExpression(statement.condition);
                requireScalar(*statement.condition, "if condition");
            }
            if (statement.body) {
                analyzeStatement(*statement.body);
            }
            if (statement.elseBody) {
                analyzeStatement(*statement.elseBody);
            }
            return;
        case Stmt::Kind::While:
            if (statement.condition) {
                analyzeExpression(statement.condition);
                requireScalar(*statement.condition, "while condition");
            }
            ++loopDepth_;
            ++breakDepth_;
            if (statement.body) {
                analyzeStatement(*statement.body);
            }
            --breakDepth_;
            --loopDepth_;
            return;
        case Stmt::Kind::DoWhile:
            ++loopDepth_;
            ++breakDepth_;
            if (statement.body) {
                analyzeStatement(*statement.body);
            }
            --breakDepth_;
            --loopDepth_;
            if (statement.condition) {
                analyzeExpression(statement.condition);
                requireScalar(*statement.condition, "do-while condition");
            }
            return;
        case Stmt::Kind::For: {
            pushScope();
            if (statement.initStatement) {
                analyzeStatement(*statement.initStatement);
            }
            if (statement.condition) {
                analyzeExpression(statement.condition);
                requireScalar(*statement.condition, "for condition");
            }
            if (statement.increment) {
                analyzeExpression(statement.increment);
            }
            ++loopDepth_;
            ++breakDepth_;
            if (statement.body) {
                analyzeStatement(*statement.body);
            }
            --breakDepth_;
            --loopDepth_;
            popScope();
            return;
        }
        case Stmt::Kind::Switch: {
            if (!statement.expression) {
                return;
            }
            analyzeExpression(statement.expression);
            if (!statement.expression->type || !statement.expression->type->isInteger()) {
                diagnostics_.error(statement.location, "switch expression must have integer type");
            }
            switchCases_.emplace_back();
            switchDefaults_.push_back(false);
            ++breakDepth_;
            if (statement.body) {
                analyzeStatement(*statement.body);
            }
            --breakDepth_;
            switchDefaults_.pop_back();
            switchCases_.pop_back();
            return;
        }
        case Stmt::Kind::Case: {
            if (switchCases_.empty()) {
                diagnostics_.error(statement.location, "'case' outside of switch");
                if (statement.body) {
                    analyzeStatement(*statement.body);
                }
                return;
            }
            if (statement.expression) {
                analyzeExpression(statement.expression);
                if (!statement.expression->constantValue.has_value()) {
                    diagnostics_.error(statement.location, "case label is not an integer constant");
                } else {
                    const std::int64_t value = *statement.expression->constantValue;
                    if (!switchCases_.back().insert(value).second) {
                        diagnostics_.error(statement.location, concat("duplicate case value ", value));
                    }
                    statement.caseValue = value;
                }
            }
            if (statement.body) {
                analyzeStatement(*statement.body);
            }
            return;
        }
        case Stmt::Kind::Default:
            if (switchDefaults_.empty()) {
                diagnostics_.error(statement.location, "'default' outside of switch");
            } else if (switchDefaults_.back()) {
                diagnostics_.error(statement.location, "duplicate default label");
            } else {
                switchDefaults_.back() = true;
            }
            if (statement.body) {
                analyzeStatement(*statement.body);
            }
            return;
        case Stmt::Kind::Break:
            if (breakDepth_ == 0) {
                diagnostics_.error(statement.location, "'break' outside of loop or switch");
            }
            return;
        case Stmt::Kind::Continue:
            if (loopDepth_ == 0) {
                diagnostics_.error(statement.location, "'continue' outside of loop");
            }
            return;
        case Stmt::Kind::Return:
            if (!currentFunction_) {
                diagnostics_.error(statement.location, "'return' outside of function");
                return;
            }
            if (statement.expression) {
                TypePtr type = analyzeExpression(statement.expression);
                if (currentFunction_->returnType->isVoid()) {
                    diagnostics_.error(statement.location, "void function returns a value");
                } else if (type) {
                    convertExpression(statement.expression, currentFunction_->returnType,
                                      "return value");
                }
            } else if (!currentFunction_->returnType->isVoid()) {
                diagnostics_.warning(statement.location,
                                     "non-void function returns without a value");
            }
            return;
        case Stmt::Kind::Goto:
            if (currentFunction_) {
                currentFunction_->gotos.insert(statement.label);
            }
            return;
        case Stmt::Kind::Label:
            if (currentFunction_) {
                if (!currentFunction_->labels.insert(statement.label).second) {
                    diagnostics_.error(statement.location,
                                       concat("duplicate label '", statement.label, "'"));
                }
            }
            if (statement.body) {
                analyzeStatement(*statement.body);
            }
            return;
        }
    }

    void analyzeLocalDeclaration(Declaration& declaration) {
        if (declaration.isTypedef) {
            return;
        }
        registerEnumConstants(declaration.baseType, scopes_.back());
        for (DeclaratorInfo& declarator : declaration.declarators) {
            if (!declarator.type) {
                continue;
            }
            if (declarator.type->isFunction()) {
                if (declarator.body) {
                    diagnostics_.error(declarator.location, "nested function definitions are not C89");
                    continue;
                }
                Symbol* symbol = createGlobal(declarator.name, declarator.type,
                                              StorageClass::Extern, declarator.location);
                symbol->isFunction = true;
                symbol->kind = SymbolKind::Function;
                declarator.symbol = symbol;
                declareLocal(declarator.name, symbol);
                continue;
            }
            if (declaration.storage == StorageClass::Extern) {
                Symbol* symbol = createGlobal(declarator.name, declarator.type,
                                              StorageClass::Extern, declarator.location);
                declarator.symbol = symbol;
                declareLocal(declarator.name, symbol);
                continue;
            }
            if (declaration.storage == StorageClass::Static) {
                Symbol* symbol = createStaticLocal(declarator.name, declarator.type,
                                                   declarator.location);
                symbol->storage = StorageClass::Static;
                declarator.symbol = symbol;
                declareLocal(declarator.name, symbol);
                if (declarator.initializer) {
                    symbol->initializer = declarator.initializer.get();
                    analyzeInitializer(*declarator.initializer, declarator.type, true);
                }
                continue;
            }
            Symbol* symbol = createLocal(declarator.name, declarator.type, declarator.location);
            symbol->storage = declaration.storage;
            declarator.symbol = symbol;
            declareLocal(declarator.name, symbol);
            if (declarator.initializer) {
                analyzeInitializer(*declarator.initializer, declarator.type, false);
            }
        }
    }

    // -- initializers -------------------------------------------------------

    void analyzeInitializer(Initializer& initializer, TypePtr target, bool global) {
        if (!target) {
            return;
        }
        if (!initializer.isList) {
            if (!initializer.expression) {
                return;
            }
            if (target->isArray() && target->element && target->element->kind == TypeKind::Char &&
                initializer.expression->kind == Expr::Kind::StringLiteral) {
                analyzeExpression(initializer.expression);
                const std::size_t length = initializer.expression->stringValue.size() + 1;
                if (target->arrayLength < 0) {
                    target->arrayLength = static_cast<std::int64_t>(length);
                } else if (length > static_cast<std::size_t>(target->arrayLength)) {
                    diagnostics_.error(initializer.location,
                                       "string initializer is too long for the array");
                }
                return;
            }
            analyzeExpression(initializer.expression);
            convertExpression(initializer.expression, target, "initializer");
            if (global && !isConstantInitializer(*initializer.expression)) {
                diagnostics_.error(initializer.location,
                                   "file-scope initializer is not a constant expression");
            }
            return;
        }

        if (target->isArray()) {
            if (!target->element) {
                return;
            }
            if (target->arrayLength < 0) {
                target->arrayLength = static_cast<std::int64_t>(initializer.elements.size());
            }
            if (initializer.elements.size() > static_cast<std::size_t>(target->arrayLength)) {
                diagnostics_.error(initializer.location, "too many initializers for array");
            }
            for (std::size_t index = 0; index < initializer.elements.size(); ++index) {
                if (initializer.elements[index]) {
                    analyzeInitializer(*initializer.elements[index], target->element, global);
                }
            }
            return;
        }

        if (target->isRecord() && target->record) {
            const RecordInfo& record = *target->record;
            const std::size_t limit = record.isUnion ? 1U : record.members.size();
            if (initializer.elements.size() > limit && !record.members.empty()) {
                diagnostics_.error(initializer.location, "too many initializers for aggregate");
            }
            for (std::size_t index = 0; index < initializer.elements.size(); ++index) {
                if (index >= record.members.size()) {
                    break;
                }
                if (initializer.elements[index]) {
                    analyzeInitializer(*initializer.elements[index], record.members[index].type,
                                       global);
                }
            }
            return;
        }

        if (initializer.elements.size() != 1 || !initializer.elements.front()) {
            diagnostics_.error(initializer.location, "scalar initializer has too many values");
            return;
        }
        analyzeInitializer(*initializer.elements.front(), target, global);
    }

    [[nodiscard]] bool isConstantInitializer(const Expr& expression) const {
        if (expression.constantValue.has_value()) {
            return true;
        }
        if (expression.kind == Expr::Kind::StringLiteral ||
            expression.kind == Expr::Kind::FloatingLiteral) {
            return true;
        }
        if (expression.kind == Expr::Kind::Cast && expression.left) {
            return isConstantInitializer(*expression.left);
        }
        if (expression.kind == Expr::Kind::Unary &&
            (expression.unaryOp == UnaryOp::Plus || expression.unaryOp == UnaryOp::Minus) &&
            expression.left) {
            return isConstantInitializer(*expression.left);
        }
        if (expression.kind == Expr::Kind::Unary && expression.unaryOp == UnaryOp::AddressOf &&
            expression.left) {
            return expression.left->lvalue ||
                   (expression.left->type && expression.left->type->isFunction());
        }
        if (expression.kind == Expr::Kind::Identifier && expression.symbol != nullptr) {
            return expression.symbol->isGlobal;
        }
        return false;
    }

    // -- expressions --------------------------------------------------------

    TypePtr analyzeExpression(std::unique_ptr<Expr>& expression) {
        if (!expression) {
            return nullptr;
        }
        TypePtr type = analyzeExpressionImpl(expression);
        expression->type = type;
        if (const auto value = evaluateConstant(*expression); value.has_value()) {
            expression->constantValue = value;
        }
        return type;
    }

    [[nodiscard]] TypePtr analyzeExpressionImpl(std::unique_ptr<Expr>& expression) {
        switch (expression->kind) {
        case Expr::Kind::IntegerLiteral: {
            TypePtr type = types_.intType();
            if (expression->integerUnsigned) {
                type = types_.unsignedIntType();
            }
            if (expression->integerValue > static_cast<std::int64_t>(0x7FFFFFFF)) {
                type = types_.unsignedIntType();
            }
            if (expression->integerValue > static_cast<std::int64_t>(0xFFFFFFFFLL)) {
                type = types_.longLongType();
            }
            expression->constantValue = expression->integerValue;
            return type;
        }
        case Expr::Kind::FloatingLiteral:
            return expression->type ? expression->type : types_.doubleType();
        case Expr::Kind::CharacterLiteral:
            expression->constantValue = expression->integerValue;
            return types_.intType();
        case Expr::Kind::StringLiteral: {
            TypePtr character = types_.charType();
            return types_.arrayOf(character,
                                  static_cast<std::int64_t>(expression->stringValue.size() + 1));
        }
        case Expr::Kind::Identifier: {
            Symbol* symbol = lookup(expression->name);
            if (symbol == nullptr) {
                diagnostics_.error(expression->location,
                                   concat("use of undeclared identifier '", expression->name, "'"));
                return types_.intType();
            }
            expression->symbol = symbol;
            if (symbol->kind == SymbolKind::EnumConstant) {
                expression->kind = Expr::Kind::IntegerLiteral;
                expression->integerValue = symbol->enumValue;
                expression->constantValue = symbol->enumValue;
                expression->type = types_.intType();
                expression->lvalue = false;
                return expression->type;
            }
            if (symbol->isFunction) {
                return symbol->type;
            }
            expression->lvalue = true;
            expression->modifiableLvalue =
                !symbol->type->isConst && !symbol->type->isArray() && !symbol->type->isFunction();
            return symbol->type;
        }
        case Expr::Kind::Unary:
            return analyzeUnary(expression);
        case Expr::Kind::Binary:
            return analyzeBinary(expression);
        case Expr::Kind::Assignment:
            return analyzeAssignment(expression);
        case Expr::Kind::Conditional:
            return analyzeConditional(expression);
        case Expr::Kind::Call:
            return analyzeCall(expression);
        case Expr::Kind::Member:
            return analyzeMember(expression);
        case Expr::Kind::Index:
            return analyzeIndex(expression);
        case Expr::Kind::Cast:
            return analyzeCast(expression);
        case Expr::Kind::SizeOf:
            return analyzeSizeof(expression);
        case Expr::Kind::Comma:
            if (expression->left) {
                analyzeExpression(expression->left);
            }
            if (!expression->right) {
                return types_.voidType();
            }
            return analyzeExpression(expression->right);
        }
        return types_.intType();
    }

    [[nodiscard]] TypePtr analyzeUnary(std::unique_ptr<Expr>& expression) {
        if (!expression->left) {
            return types_.intType();
        }
        TypePtr operand = analyzeExpression(expression->left);
        switch (expression->unaryOp) {
        case UnaryOp::AddressOf: {
            if (!expression->left->lvalue && !operand->isFunction()) {
                diagnostics_.error(expression->location, "cannot take the address of an rvalue");
            }
            return types_.pointerTo(operand);
        }
        case UnaryOp::Dereference: {
            TypePtr decayed = types_.decay(operand);
            if (!decayed->isPointer()) {
                diagnostics_.error(expression->location, "cannot dereference a non-pointer");
                return types_.intType();
            }
            TypePtr element = decayed->element;
            expression->lvalue = true;
            expression->modifiableLvalue = element && !element->isConst && !element->isArray() &&
                                           !element->isFunction();
            return element;
        }
        case UnaryOp::Plus:
        case UnaryOp::Minus:
        case UnaryOp::BitwiseNot:
            if (!operand->isInteger()) {
                diagnostics_.error(expression->location, "operand must have integer type");
                return types_.intType();
            }
            return promoteType(operand);
        case UnaryOp::LogicalNot:
            requireScalar(*expression->left, "logical operand");
            return types_.intType();
        case UnaryOp::PreIncrement:
        case UnaryOp::PreDecrement:
        case UnaryOp::PostIncrement:
        case UnaryOp::PostDecrement:
            if (!expression->left->modifiableLvalue) {
                diagnostics_.error(expression->location,
                                   "increment/decrement requires a modifiable lvalue");
            }
            if (!operand->isArithmetic() && !operand->isPointer()) {
                diagnostics_.error(expression->location,
                                   "increment/decrement requires arithmetic or pointer type");
            }
            expression->lvalue = false;
            return operand;
        }
        return types_.intType();
    }

    [[nodiscard]] TypePtr analyzeBinary(std::unique_ptr<Expr>& expression) {
        if (!expression->left || !expression->right) {
            return types_.intType();
        }
        TypePtr left = analyzeExpression(expression->left);
        TypePtr right = analyzeExpression(expression->right);

        switch (expression->binaryOp) {
        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr:
            requireScalar(*expression->left, "logical operand");
            requireScalar(*expression->right, "logical operand");
            return types_.intType();
        case BinaryOp::Less:
        case BinaryOp::Greater:
        case BinaryOp::LessEqual:
        case BinaryOp::GreaterEqual:
        case BinaryOp::Equal:
        case BinaryOp::NotEqual: {
            if (left->isArithmetic() && right->isArithmetic()) {
                TypePtr common = commonArithmeticType(left, right, expression->location);
                convertExpression(expression->left, common, "comparison");
                convertExpression(expression->right, common, "comparison");
                return types_.intType();
            }
            const bool equality = expression->binaryOp == BinaryOp::Equal ||
                                  expression->binaryOp == BinaryOp::NotEqual;
            if (left->isPointer() && right->isPointer()) {
                if (!TypeContext::compatibleType(left, right)) {
                    diagnostics_.error(expression->location,
                                       "comparison of incompatible pointer types");
                }
                return types_.intType();
            }
            if (equality && (left->isPointer() || right->isPointer())) {
                const bool leftNull = left->isPointer() ? isNullConstant(*expression->right)
                                                        : isNullConstant(*expression->left);
                if (!leftNull && !isNullConstant(*expression->left) &&
                    !isNullConstant(*expression->right)) {
                    diagnostics_.error(expression->location,
                                       "comparison between pointer and integer");
                }
                return types_.intType();
            }
            diagnostics_.error(expression->location, "invalid operands to comparison");
            return types_.intType();
        }
        case BinaryOp::Add:
        case BinaryOp::Subtract:
            return analyzeAdditive(expression, left, right);
        case BinaryOp::ShiftLeft:
        case BinaryOp::ShiftRight:
            if (!left->isInteger() || !right->isInteger()) {
                diagnostics_.error(expression->location, "shift operands must have integer type");
                return types_.intType();
            }
            return promoteType(left);
        case BinaryOp::BitwiseAnd:
        case BinaryOp::BitwiseOr:
        case BinaryOp::BitwiseXor:
            if (!left->isInteger() || !right->isInteger()) {
                diagnostics_.error(expression->location, "bitwise operands must have integer type");
                return types_.intType();
            }
            {
                TypePtr common = commonArithmeticType(left, right, expression->location);
                convertExpression(expression->left, common, "bitwise operand");
                convertExpression(expression->right, common, "bitwise operand");
                return common;
            }
        case BinaryOp::Multiply:
        case BinaryOp::Divide:
        case BinaryOp::Modulo: {
            if (!left->isArithmetic() || !right->isArithmetic()) {
                diagnostics_.error(expression->location, "arithmetic operands required");
                return types_.intType();
            }
            if (expression->binaryOp == BinaryOp::Modulo && (!left->isInteger() || !right->isInteger())) {
                diagnostics_.error(expression->location, "modulo requires integer operands");
            }
            TypePtr common = commonArithmeticType(left, right, expression->location);
            convertExpression(expression->left, common, "arithmetic operand");
            convertExpression(expression->right, common, "arithmetic operand");
            return common;
        }
        }
        return types_.intType();
    }

    [[nodiscard]] TypePtr analyzeAdditive(std::unique_ptr<Expr>& expression, TypePtr left,
                                          TypePtr right) {
        if (left->isArithmetic() && right->isArithmetic()) {
            TypePtr common = commonArithmeticType(left, right, expression->location);
            convertExpression(expression->left, common, "arithmetic operand");
            convertExpression(expression->right, common, "arithmetic operand");
            return common;
        }

        const bool leftPointer = left->isPointer() || left->isArray();
        const bool rightPointer = right->isPointer() || right->isArray();
        TypePtr leftDecayed = types_.decay(left);
        TypePtr rightDecayed = types_.decay(right);

        if (leftPointer && right->isInteger()) {
            decayExpression(expression->left);
            return leftDecayed;
        }
        if (rightPointer && left->isInteger() && expression->binaryOp == BinaryOp::Add) {
            decayExpression(expression->right);
            return rightDecayed;
        }
        if (leftPointer && rightPointer && expression->binaryOp == BinaryOp::Subtract) {
            if (!TypeContext::compatibleType(leftDecayed, rightDecayed)) {
                diagnostics_.error(expression->location,
                                   "subtraction of incompatible pointer types");
            }
            decayExpression(expression->left);
            decayExpression(expression->right);
            return types_.intType();
        }
        diagnostics_.error(expression->location, "invalid operands to additive operator");
        return types_.intType();
    }

    [[nodiscard]] TypePtr analyzeAssignment(std::unique_ptr<Expr>& expression) {
        if (!expression->left || !expression->right) {
            return types_.intType();
        }
        TypePtr left = analyzeExpression(expression->left);
        TypePtr right = analyzeExpression(expression->right);
        if (!expression->left->modifiableLvalue) {
            diagnostics_.error(expression->location, "assignment requires a modifiable lvalue");
        }
        if (expression->assignOp == AssignOp::Assign) {
            convertExpression(expression->right, left, "assignment");
            return left;
        }
        if (!left->isArithmetic() && !left->isPointer()) {
            diagnostics_.error(expression->location, "compound assignment requires arithmetic type");
            return left;
        }
        if (left->isPointer()) {
            if (!right->isInteger()) {
                diagnostics_.error(expression->location,
                                   "pointer compound assignment requires an integer operand");
            }
        } else if (!right->isArithmetic()) {
            diagnostics_.error(expression->location,
                               "compound assignment requires arithmetic operands");
        }
        return left;
    }

    [[nodiscard]] TypePtr analyzeConditional(std::unique_ptr<Expr>& expression) {
        if (!expression->left || !expression->right || !expression->third) {
            return types_.intType();
        }
        analyzeExpression(expression->left);
        requireScalar(*expression->left, "conditional condition");
        TypePtr whenTrue = analyzeExpression(expression->right);
        TypePtr whenFalse = analyzeExpression(expression->third);

        if (whenTrue->isArithmetic() && whenFalse->isArithmetic()) {
            TypePtr common = commonArithmeticType(whenTrue, whenFalse, expression->location);
            convertExpression(expression->right, common, "conditional branch");
            convertExpression(expression->third, common, "conditional branch");
            return common;
        }
        if (whenTrue->isVoid() && whenFalse->isVoid()) {
            return types_.voidType();
        }
        TypePtr trueDecayed = types_.decay(whenTrue);
        TypePtr falseDecayed = types_.decay(whenFalse);
        if (trueDecayed->isPointer() && falseDecayed->isPointer()) {
            if (!TypeContext::compatibleType(trueDecayed, falseDecayed)) {
                diagnostics_.error(expression->location,
                                   "conditional branches have incompatible pointer types");
            }
            decayExpression(expression->right);
            decayExpression(expression->third);
            return trueDecayed;
        }
        if (trueDecayed->isPointer() && isNullConstant(*expression->third)) {
            decayExpression(expression->right);
            return trueDecayed;
        }
        if (falseDecayed->isPointer() && isNullConstant(*expression->right)) {
            decayExpression(expression->third);
            return falseDecayed;
        }
        diagnostics_.error(expression->location,
                           "conditional branches have incompatible types");
        return whenTrue;
    }

    [[nodiscard]] TypePtr analyzeCall(std::unique_ptr<Expr>& expression) {
        if (!expression->left) {
            return types_.intType();
        }
        TypePtr callee = analyzeExpression(expression->left);
        TypePtr function = callee;
        if (function->isPointer() && function->element && function->element->isFunction()) {
            function = function->element;
        }
        if (!function->isFunction()) {
            diagnostics_.error(expression->location, "called expression is not a function");
            for (std::unique_ptr<Expr>& argument : expression->arguments) {
                analyzeExpression(argument);
            }
            return types_.intType();
        }

        for (std::unique_ptr<Expr>& argument : expression->arguments) {
            analyzeExpression(argument);
            decayExpression(argument);
        }

        const std::size_t parameterCount = function->parameterTypes.size();
        if (function->hasPrototype) {
            if (expression->arguments.size() < parameterCount) {
                diagnostics_.error(expression->location, "too few arguments to function call");
            }
            for (std::size_t index = 0; index < expression->arguments.size(); ++index) {
                if (index < parameterCount) {
                    convertExpression(expression->arguments[index], function->parameterTypes[index],
                                      "argument");
                } else if (function->variadic) {
                    defaultPromote(expression->arguments[index]);
                } else {
                    diagnostics_.error(expression->arguments[index]->location,
                                       "too many arguments to function call");
                }
            }
        } else {
            for (std::unique_ptr<Expr>& argument : expression->arguments) {
                defaultPromote(argument);
            }
        }
        return function->returnType ? function->returnType : types_.intType();
    }

    [[nodiscard]] TypePtr analyzeMember(std::unique_ptr<Expr>& expression) {
        if (!expression->left) {
            return types_.intType();
        }
        TypePtr base = analyzeExpression(expression->left);
        if (expression->isArrow) {
            TypePtr decayed = types_.decay(base);
            if (!decayed->isPointer() || !decayed->element) {
                diagnostics_.error(expression->location, "'->' requires a pointer to struct/union");
                return types_.intType();
            }
            base = decayed->element;
        }
        if (!base->isRecord() || !base->record) {
            diagnostics_.error(expression->location, "member access requires struct or union");
            return types_.intType();
        }
        const RecordInfo& record = *base->record;
        const auto found = std::find_if(record.members.begin(), record.members.end(),
                                        [&](const RecordMember& member) {
                                            return member.name == expression->memberName;
                                        });
        if (found == record.members.end()) {
            diagnostics_.error(expression->location,
                               concat("no member named '", expression->memberName, "'"));
            return types_.intType();
        }
        expression->member = const_cast<RecordMember*>(&*found);
        expression->lvalue = expression->isArrow || expression->left->lvalue;
        expression->modifiableLvalue =
            expression->lvalue && !found->type->isConst && !found->type->isArray();
        return found->type;
    }

    [[nodiscard]] TypePtr analyzeIndex(std::unique_ptr<Expr>& expression) {
        if (!expression->left || !expression->right) {
            return types_.intType();
        }
        TypePtr left = analyzeExpression(expression->left);
        TypePtr right = analyzeExpression(expression->right);
        TypePtr leftDecayed = types_.decay(left);
        TypePtr rightDecayed = types_.decay(right);
        if (leftDecayed->isPointer() && right->isInteger()) {
            expression->lvalue = true;
            expression->modifiableLvalue = leftDecayed->element && !leftDecayed->element->isConst;
            return leftDecayed->element;
        }
        if (rightDecayed->isPointer() && left->isInteger()) {
            expression->lvalue = true;
            expression->modifiableLvalue = rightDecayed->element && !rightDecayed->element->isConst;
            return rightDecayed->element;
        }
        diagnostics_.error(expression->location, "subscripted value is not a pointer or array");
        return types_.intType();
    }

    [[nodiscard]] TypePtr analyzeCast(std::unique_ptr<Expr>& expression) {
        if (!expression->castType) {
            return types_.intType();
        }
        if (expression->left) {
            TypePtr source = analyzeExpression(expression->left);
            const bool valid = source->isArithmetic() || source->isPointer() ||
                               expression->castType->isVoid() ||
                               (expression->castType->isPointer() && source->isInteger());
            if (!valid) {
                diagnostics_.error(expression->location, "invalid cast");
            }
        }
        expression->lvalue = false;
        expression->modifiableLvalue = false;
        return expression->castType;
    }

    [[nodiscard]] TypePtr analyzeSizeof(std::unique_ptr<Expr>& expression) {
        TypePtr target = expression->sizeofType;
        if (!target && expression->left) {
            target = analyzeExpression(expression->left);
        }
        if (!target) {
            diagnostics_.error(expression->location, "sizeof requires a type or expression");
            return types_.unsignedIntType();
        }
        if (!types_.isComplete(target)) {
            diagnostics_.error(expression->location,
                               concat("sizeof applied to incomplete type '", types_.describe(target),
                                      "'"));
        }
        expression->constantValue = static_cast<std::int64_t>(types_.sizeOf(target));
        return types_.unsignedIntType();
    }

    // -- expression utilities ----------------------------------------------

    void decayExpression(std::unique_ptr<Expr>& expression) {
        if (!expression || !expression->type) {
            return;
        }
        if (expression->type->isArray() || expression->type->isFunction()) {
            castExpression(expression, types_.decay(expression->type));
        }
    }

    void castExpression(std::unique_ptr<Expr>& expression, TypePtr target) {
        if (!expression || !target) {
            return;
        }
        if (expression->type && TypeContext::sameType(expression->type, target)) {
            return;
        }
        auto cast = std::make_unique<Expr>();
        cast->kind = Expr::Kind::Cast;
        cast->location = expression->location;
        cast->castType = target;
        cast->type = target;
        cast->left = std::move(expression);
        if (const auto value = evaluateConstant(*cast); value.has_value()) {
            cast->constantValue = value;
        }
        expression = std::move(cast);
    }

    void convertExpression(std::unique_ptr<Expr>& expression, TypePtr target,
                           std::string_view context) {
        if (!expression || !target) {
            return;
        }
        TypePtr source = expression->type;
        if (!source) {
            return;
        }
        if (source->isArray() || source->isFunction()) {
            decayExpression(expression);
            source = expression->type;
        }
        if (source->isVoid() || target->isVoid()) {
            return;
        }
        if (source->isArithmetic() && target->isArithmetic()) {
            castExpression(expression, target);
            return;
        }
        if (source->isPointer() && target->isPointer()) {
            if (!TypeContext::compatibleType(source, target)) {
                diagnostics_.warning(expression->location,
                                     concat("incompatible pointer conversion in ", context));
            }
            castExpression(expression, target);
            return;
        }
        if (target->isPointer() && source->isInteger()) {
            if (!isNullConstant(*expression)) {
                diagnostics_.warning(expression->location,
                                     concat("integer to pointer conversion in ", context));
            }
            castExpression(expression, target);
            return;
        }
        if (source->isPointer() && target->isInteger()) {
            diagnostics_.warning(expression->location,
                                 concat("pointer to integer conversion in ", context));
            castExpression(expression, target);
            return;
        }
        diagnostics_.error(expression->location,
                           concat("cannot convert '", types_.describe(source), "' to '",
                                  types_.describe(target), "' in ", context));
    }

    void requireScalar(const Expr& expression, std::string_view context) {
        if (!expression.type || !expression.type->isScalar()) {
            diagnostics_.error(expression.location,
                               concat(context, " requires a scalar expression"));
        }
    }

    void defaultPromote(std::unique_ptr<Expr>& expression) {
        if (!expression || !expression->type) {
            return;
        }
        if (expression->type->kind == TypeKind::Float) {
            castExpression(expression, types_.doubleType());
        } else if (expression->type->isInteger()) {
            castExpression(expression, promoteType(expression->type));
        }
    }

    [[nodiscard]] bool isNullConstant(const Expr& expression) const {
        return expression.constantValue.has_value() && *expression.constantValue == 0 &&
               expression.type && expression.type->isInteger();
    }

    [[nodiscard]] TypePtr promoteType(TypePtr type) const {
        if (!type) {
            return types_.intType();
        }
        if (type->isEnum()) {
            return types_.intType();
        }
        if (type->isInteger() && (typeRank(type->kind) < 3 || type->isBool())) {
            return types_.intType();
        }
        return type;
    }

    [[nodiscard]] TypePtr commonArithmeticType(TypePtr left, TypePtr right,
                                               SourceLocation location) const {
        if (left->kind == TypeKind::LongDouble || right->kind == TypeKind::LongDouble) {
            return types_.longDoubleType();
        }
        if (left->kind == TypeKind::Double || right->kind == TypeKind::Double) {
            return types_.doubleType();
        }
        if (left->kind == TypeKind::Float || right->kind == TypeKind::Float) {
            return types_.floatType();
        }
        TypePtr promotedLeft = promoteType(left);
        TypePtr promotedRight = promoteType(right);
        if (promotedLeft->kind == promotedRight->kind) {
            return promotedLeft;
        }
        const int leftRank = typeRank(promotedLeft->kind);
        const int rightRank = typeRank(promotedRight->kind);
        const bool leftUnsigned = promotedLeft->isUnsignedInteger();
        const bool rightUnsigned = promotedRight->isUnsignedInteger();
        if (leftUnsigned != rightUnsigned) {
            if (leftUnsigned) {
                return leftRank >= rightRank ? promotedLeft : unsignedVersion(promotedRight);
            }
            return rightRank >= leftRank ? promotedRight : unsignedVersion(promotedLeft);
        }
        if (leftRank == rightRank) {
            diagnostics_.error(location, "unable to find a common arithmetic type");
            return promotedLeft;
        }
        return leftRank > rightRank ? promotedLeft : promotedRight;
    }

    [[nodiscard]] TypePtr unsignedVersion(TypePtr type) const {
        switch (type->kind) {
        case TypeKind::Char:
        case TypeKind::SChar:
            return types_.unsignedCharType();
        case TypeKind::Short:
            return types_.unsignedShortType();
        case TypeKind::Int:
            return types_.unsignedIntType();
        case TypeKind::Long:
            return types_.unsignedLongType();
        case TypeKind::LongLong:
            return types_.unsignedLongLongType();
        default:
            return type;
        }
    }

    // -- constant folding ---------------------------------------------------

    [[nodiscard]] std::optional<std::int64_t> evaluateConstant(const Expr& expression) const {
        switch (expression.kind) {
        case Expr::Kind::IntegerLiteral:
        case Expr::Kind::CharacterLiteral:
            return expression.integerValue;
        case Expr::Kind::Unary: {
            if (!expression.left) {
                return std::nullopt;
            }
            const auto value = evaluateConstant(*expression.left);
            if (!value.has_value()) {
                return std::nullopt;
            }
            switch (expression.unaryOp) {
            case UnaryOp::Plus:
                return *value;
            case UnaryOp::Minus:
                return -*value;
            case UnaryOp::LogicalNot:
                return *value == 0 ? 1 : 0;
            case UnaryOp::BitwiseNot:
                return ~*value;
            default:
                return std::nullopt;
            }
        }
        case Expr::Kind::Binary: {
            if (!expression.left || !expression.right) {
                return std::nullopt;
            }
            const auto left = evaluateConstant(*expression.left);
            const auto right = evaluateConstant(*expression.right);
            if (!left.has_value() || !right.has_value()) {
                return std::nullopt;
            }
            switch (expression.binaryOp) {
            case BinaryOp::Add:
                return *left + *right;
            case BinaryOp::Subtract:
                return *left - *right;
            case BinaryOp::Multiply:
                return *left * *right;
            case BinaryOp::Divide:
                return *right == 0 ? std::nullopt
                                   : std::optional<std::int64_t>(*left / *right);
            case BinaryOp::Modulo:
                return *right == 0 ? std::nullopt
                                   : std::optional<std::int64_t>(*left % *right);
            case BinaryOp::ShiftLeft:
                return *left << (*right & 31);
            case BinaryOp::ShiftRight:
                return *left >> (*right & 31);
            case BinaryOp::Less:
                return *left < *right ? 1 : 0;
            case BinaryOp::Greater:
                return *left > *right ? 1 : 0;
            case BinaryOp::LessEqual:
                return *left <= *right ? 1 : 0;
            case BinaryOp::GreaterEqual:
                return *left >= *right ? 1 : 0;
            case BinaryOp::Equal:
                return *left == *right ? 1 : 0;
            case BinaryOp::NotEqual:
                return *left != *right ? 1 : 0;
            case BinaryOp::BitwiseAnd:
                return *left & *right;
            case BinaryOp::BitwiseOr:
                return *left | *right;
            case BinaryOp::BitwiseXor:
                return *left ^ *right;
            case BinaryOp::LogicalAnd:
                return (*left != 0 && *right != 0) ? 1 : 0;
            case BinaryOp::LogicalOr:
                return (*left != 0 || *right != 0) ? 1 : 0;
            }
            return std::nullopt;
        }
        case Expr::Kind::Conditional: {
            if (!expression.left) {
                return std::nullopt;
            }
            const auto condition = evaluateConstant(*expression.left);
            if (!condition.has_value()) {
                return std::nullopt;
            }
            if (*condition != 0) {
                return expression.right ? evaluateConstant(*expression.right) : std::nullopt;
            }
            return expression.third ? evaluateConstant(*expression.third) : std::nullopt;
        }
        case Expr::Kind::Cast: {
            if (!expression.left || !expression.castType) {
                return std::nullopt;
            }
            const auto value = evaluateConstant(*expression.left);
            if (!value.has_value() || !expression.castType->isInteger()) {
                return std::nullopt;
            }
            const std::size_t size = types_.sizeOf(expression.castType);
            if (size == 1) {
                return expression.castType->isUnsignedInteger()
                           ? static_cast<std::int64_t>(static_cast<std::uint8_t>(*value))
                           : static_cast<std::int64_t>(static_cast<std::int8_t>(*value));
            }
            if (size == 2) {
                return expression.castType->isUnsignedInteger()
                           ? static_cast<std::int64_t>(static_cast<std::uint16_t>(*value))
                           : static_cast<std::int64_t>(static_cast<std::int16_t>(*value));
            }
            if (size == 4) {
                return expression.castType->isUnsignedInteger()
                           ? static_cast<std::int64_t>(static_cast<std::uint32_t>(*value))
                           : static_cast<std::int64_t>(static_cast<std::int32_t>(*value));
            }
            return value;
        }
        case Expr::Kind::SizeOf:
            return expression.constantValue;
        case Expr::Kind::Comma:
            return expression.right ? evaluateConstant(*expression.right) : std::nullopt;
        default:
            return std::nullopt;
        }
    }

    TranslationUnit& unit_;
    TypeContext& types_;
    Diagnostics& diagnostics_;
    std::vector<std::map<std::string, Symbol*>> scopes_;
    std::vector<std::unique_ptr<Symbol>> symbols_;
    SemanticProgram program_;
    FunctionContext* currentFunction_ = nullptr;
    int frameSize_ = 0;
    int loopDepth_ = 0;
    int breakDepth_ = 0;
    std::vector<std::set<std::int64_t>> switchCases_;
    std::vector<bool> switchDefaults_;
    int staticLocalCounter_ = 0;
};

// ---------------------------------------------------------------------------
// KR32 code generation
// ---------------------------------------------------------------------------

class CodeGenerator {
public:
    CodeGenerator(SemanticProgram& program, TypeContext& types, Diagnostics& diagnostics)
        : program_(program), types_(types), diagnostics_(diagnostics) {}

    [[nodiscard]] std::string generate() {
        text_ << "; kevinc: C89 to KR32 assembly\n";
        text_ << "; stack-machine code generation, KR32 ILP32 ABI\n";
        text_ << "    .text\n";
        for (Symbol* symbol : program_.externals) {
            if (symbol != nullptr && !symbol->isDefined) {
                text_ << "    .extern " << symbol->asmName << "\n";
            }
        }
        text_ << "\n";
        for (FunctionDefinition& function : program_.functions) {
            generateFunction(function);
        }

        data_ << "    .data\n";
        for (Symbol* global : program_.globals) {
            if (global != nullptr) {
                generateGlobal(*global);
            }
        }
        generateLiteralPool();
        return text_.str() + data_.str();
    }

private:
    struct FloatLiteral {
        std::string label;
        bool isFloat = false;
        std::uint64_t bits = 0;
    };

    static std::string hex32(std::uint32_t value) {
        std::ostringstream stream;
        stream << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << value;
        return stream.str();
    }

    static std::string hex64(std::uint64_t value) {
        std::ostringstream stream;
        stream << "0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << value;
        return stream.str();
    }

    void emit(const std::string& text) { text_ << "    " << text << '\n'; }
    void emitData(const std::string& text) { data_ << "    " << text << '\n'; }

    [[nodiscard]] std::string newLabel(std::string_view prefix) {
        return concat(".L", prefix, "_", labelCounter_++);
    }

    [[nodiscard]] std::string userLabel(std::string_view name) const {
        return concat(".Luser_", functionCounter_, "_", name);
    }

    // -- literals -----------------------------------------------------------

    [[nodiscard]] std::string labelForString(const std::string& value) {
        const auto found = stringLabels_.find(value);
        if (found != stringLabels_.end()) {
            return found->second;
        }
        const std::string label = concat(".Lstr_", stringCounter_++);
        stringLabels_[value] = label;
        strings_.push_back({label, value});
        return label;
    }

    [[nodiscard]] std::string labelForFloat(bool isFloat, std::uint64_t bits) {
        const std::pair<bool, std::uint64_t> key{isFloat, bits};
        const auto found = floatLabels_.find(key);
        if (found != floatLabels_.end()) {
            return found->second;
        }
        const std::string label = concat(".Lfp_", floatCounter_++);
        floatLabels_[key] = label;
        floats_.push_back({label, isFloat, bits});
        return label;
    }

    void generateLiteralPool() {
        for (const auto& [label, value] : strings_) {
            data_ << "    .align 4\n" << label << ":\n";
            std::size_t index = 0;
            while (index <= value.size()) {
                std::ostringstream line;
                line << "    .byte ";
                const std::size_t end = std::min(value.size() + 1, index + 16);
                bool first = true;
                for (std::size_t at = index; at < end; ++at) {
                    if (!first) {
                        line << ", ";
                    }
                    first = false;
                    const unsigned byte = at < value.size()
                                              ? static_cast<unsigned char>(value[at])
                                              : 0U;
                    line << byte;
                }
                data_ << line.str() << '\n';
                index = end;
            }
        }
        for (const auto& [label, isFloat, bits] : floats_) {
            data_ << "    .align " << (isFloat ? 4 : 8) << "\n" << label << ":\n";
            if (isFloat) {
                emitData(".word " + hex32(static_cast<std::uint32_t>(bits)));
            } else {
                emitData(".word " + hex32(static_cast<std::uint32_t>(bits & 0xFFFFFFFFU)) + ", " +
                         hex32(static_cast<std::uint32_t>(bits >> 32)));
            }
        }
    }

    // -- type helpers -------------------------------------------------------

    [[nodiscard]] static bool isFloatType(const TypePtr& type) {
        return type && type->kind == TypeKind::Float;
    }

    [[nodiscard]] static bool isDoubleType(const TypePtr& type) {
        return type && (type->kind == TypeKind::Double || type->kind == TypeKind::LongDouble);
    }

    [[nodiscard]] static std::string floatSuffix(const TypePtr& type) {
        return isFloatType(type) ? "S" : "D";
    }

    [[nodiscard]] static std::string floatWidth(const TypePtr& type) {
        return isFloatType(type) ? "W" : "D";
    }

    [[nodiscard]] static std::string loadMnemonic(const TypePtr& type) {
        if (!type) {
            return "LOAD";
        }
        switch (type->kind) {
        case TypeKind::Bool:
        case TypeKind::UChar:
            return "LOAD8U";
        case TypeKind::Char:
        case TypeKind::SChar:
            return "LOAD8S";
        case TypeKind::UShort:
            return "LOAD16U";
        case TypeKind::Short:
            return "LOAD16S";
        default:
            return "LOAD";
        }
    }

    [[nodiscard]] static std::string storeMnemonic(const TypePtr& type) {
        if (!type) {
            return "STORE";
        }
        switch (type->kind) {
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::SChar:
        case TypeKind::UChar:
            return "STORE8";
        case TypeKind::Short:
        case TypeKind::UShort:
            return "STORE16";
        default:
            return "STORE";
        }
    }

    void emitLoadFromR0(const TypePtr& type) {
        if (!type || type->isArray() || type->isRecord() || type->isFunction()) {
            return;
        }
        if (type->isFloating()) {
            emit(concat("FLOAD.", floatWidth(type), " F0, [R0]"));
            return;
        }
        emit(concat(loadMnemonic(type), " R0, [R0]"));
    }

    void emitStoreToR1(const TypePtr& type) {
        if (!type) {
            return;
        }
        if (type->isFloating()) {
            emit(concat("FSTORE.", floatWidth(type), " F0, [R1]"));
            return;
        }
        emit(concat(storeMnemonic(type), " R0, [R1]"));
    }

    void emitLoadDirect(const TypePtr& type, int offset) {
        const std::string address = concat("[R7 + ", offset, "]");
        if (type && type->isFloating()) {
            emit(concat("FLOAD.", floatWidth(type), " F0, ", address));
            return;
        }
        emit(concat(loadMnemonic(type), " R0, ", address));
    }

    void emitStoreDirect(const TypePtr& type, int offset) {
        const std::string address = concat("[R7 + ", offset, "]");
        if (type && type->isFloating()) {
            emit(concat("FSTORE.", floatWidth(type), " F0, ", address));
            return;
        }
        emit(concat(storeMnemonic(type), " R0, ", address));
    }

    void emitExtendR0(const TypePtr& type) {
        if (!type || !type->isInteger()) {
            return;
        }
        if (type->isBool()) {
            emitBoolFromR0();
            return;
        }
        const std::size_t size = types_.sizeOf(type);
        if (size >= 4) {
            return;
        }
        if (size == 1) {
            if (type->isUnsignedInteger()) {
                emit("AND R0, R0, #255");
            } else {
                emit("SHL R0, R0, #24");
                emit("SAR R0, R0, #24");
            }
        } else if (size == 2) {
            if (type->isUnsignedInteger()) {
                emit("AND R0, R0, #65535");
            } else {
                emit("SHL R0, R0, #16");
                emit("SAR R0, R0, #16");
            }
        }
    }

    void emitBoolFromR0() {
        const std::string done = newLabel("bool");
        emit("CMP R0, #0");
        emit("MOV R0, #0");
        emit("JZ " + done);
        emit("MOV R0, #1");
        text_ << done << ":\n";
    }

    void emitLogicalNot() {
        const std::string done = newLabel("not");
        emit("CMP R0, #0");
        emit("MOV R0, #0");
        emit("JNZ " + done);
        emit("MOV R0, #1");
        text_ << done << ":\n";
    }

    // -- addresses ----------------------------------------------------------

    void generateAddress(const Expr& expression) {
        switch (expression.kind) {
        case Expr::Kind::Identifier: {
            Symbol* symbol = expression.symbol;
            if (symbol == nullptr) {
                diagnostics_.error(expression.location, "internal: unresolved identifier address");
                emit("MOV R0, #0");
                return;
            }
            if (symbol->isGlobal) {
                emit("LEA R0, [" + symbol->asmName + "]");
            } else {
                emit(concat("LEA R0, [R7 + ", symbol->frameOffset, "]"));
            }
            return;
        }
        case Expr::Kind::Unary:
            if (expression.unaryOp == UnaryOp::Dereference && expression.left) {
                generateExpression(*expression.left);
                return;
            }
            break;
        case Expr::Kind::Member: {
            if (!expression.left || !expression.member) {
                break;
            }
            if (expression.isArrow) {
                generateExpression(*expression.left);
            } else {
                generateAddress(*expression.left);
            }
            if (expression.member->offset != 0) {
                emit(concat("ADD R0, R0, #", expression.member->offset));
            }
            return;
        }
        case Expr::Kind::Index: {
            if (!expression.left || !expression.right) {
                break;
            }
            const TypePtr leftType = expression.left->type;
            const bool leftPointer = leftType && (leftType->isPointer() || leftType->isArray());
            TypePtr pointerType = leftPointer ? types_.decay(leftType) : types_.decay(expression.right->type);
            const std::size_t elementSize =
                pointerType && pointerType->element ? types_.sizeOf(pointerType->element) : 1;
            if (leftPointer) {
                generateExpression(*expression.left);
                emit("PUSH R0");
                generateExpression(*expression.right);
            } else {
                generateExpression(*expression.right);
                emit("PUSH R0");
                generateExpression(*expression.left);
            }
            if (elementSize > 1) {
                emit(concat("MUL R0, R0, #", elementSize));
            }
            emit("POP R1");
            emit("ADD R0, R1, R0");
            return;
        }
        case Expr::Kind::StringLiteral:
            emit("LEA R0, [" + labelForString(expression.stringValue) + "]");
            return;
        default:
            break;
        }
        diagnostics_.error(expression.location, "expression is not addressable");
        generateExpression(expression);
    }

    // -- expressions --------------------------------------------------------

    void generateExpression(const Expr& expression) {
        switch (expression.kind) {
        case Expr::Kind::IntegerLiteral:
            emit(concat("MOV R0, #", static_cast<std::int32_t>(expression.integerValue)));
            return;
        case Expr::Kind::CharacterLiteral:
            emit(concat("MOV R0, #", static_cast<std::int32_t>(expression.integerValue)));
            return;
        case Expr::Kind::FloatingLiteral: {
            const bool isFloat = isFloatType(expression.type);
            std::uint64_t bits = 0;
            if (isFloat) {
                const float value = static_cast<float>(expression.floatingValue);
                std::memcpy(&bits, &value, sizeof(value));
            } else {
                const double value = static_cast<double>(expression.floatingValue);
                std::memcpy(&bits, &value, sizeof(value));
            }
            const std::string label = labelForFloat(isFloat, bits);
            emit("LEA R0, [" + label + "]");
            emit(concat("FLOAD.", isFloat ? "W" : "D", " F0, [R0]"));
            return;
        }
        case Expr::Kind::StringLiteral:
            emit("LEA R0, [" + labelForString(expression.stringValue) + "]");
            return;
        case Expr::Kind::Identifier:
            generateIdentifier(expression);
            return;
        case Expr::Kind::Unary:
            generateUnary(expression);
            return;
        case Expr::Kind::Binary:
            generateBinary(expression);
            return;
        case Expr::Kind::Assignment:
            generateAssignment(expression);
            return;
        case Expr::Kind::Conditional:
            generateConditional(expression);
            return;
        case Expr::Kind::Call:
            generateCall(expression);
            return;
        case Expr::Kind::Member:
        case Expr::Kind::Index:
            generateAddress(expression);
            emitLoadFromR0(expression.type);
            return;
        case Expr::Kind::Cast:
            generateCast(expression);
            return;
        case Expr::Kind::SizeOf: {
            const std::int64_t value = expression.constantValue.value_or(
                expression.sizeofType ? static_cast<std::int64_t>(types_.sizeOf(expression.sizeofType))
                                      : 0);
            emit(concat("MOV R0, #", static_cast<std::int32_t>(value)));
            return;
        }
        case Expr::Kind::Comma:
            if (expression.left) {
                generateExpression(*expression.left);
            }
            if (expression.right) {
                generateExpression(*expression.right);
            }
            return;
        }
    }

    void generateIdentifier(const Expr& expression) {
        Symbol* symbol = expression.symbol;
        if (symbol == nullptr) {
            diagnostics_.error(expression.location, "internal: unresolved identifier");
            emit("MOV R0, #0");
            return;
        }
        if (symbol->kind == SymbolKind::EnumConstant) {
            emit(concat("MOV R0, #", static_cast<std::int32_t>(symbol->enumValue)));
            return;
        }
        if (symbol->isFunction) {
            emit("LEA R0, [" + symbol->asmName + "]");
            return;
        }
        if (expression.type && (expression.type->isArray() || expression.type->isRecord())) {
            generateAddress(expression);
            return;
        }
        if (symbol->isGlobal) {
            emit("LEA R0, [" + symbol->asmName + "]");
            emitLoadFromR0(expression.type);
        } else {
            emitLoadDirect(expression.type, symbol->frameOffset);
        }
    }

    void generateUnary(const Expr& expression) {
        if (!expression.left) {
            return;
        }
        switch (expression.unaryOp) {
        case UnaryOp::AddressOf:
            generateAddress(*expression.left);
            return;
        case UnaryOp::Dereference:
            generateExpression(*expression.left);
            emitLoadFromR0(expression.type);
            return;
        case UnaryOp::Plus:
            generateExpression(*expression.left);
            return;
        case UnaryOp::Minus:
            generateExpression(*expression.left);
            if (expression.type && expression.type->isFloating()) {
                emit(concat("FNEG.", floatSuffix(expression.type), " F0, F0"));
            } else {
                emit("NEG R0, R0");
            }
            return;
        case UnaryOp::LogicalNot:
            generateCondition(*expression.left);
            emitLogicalNot();
            return;
        case UnaryOp::BitwiseNot:
            generateExpression(*expression.left);
            emit("NOT R0, R0");
            return;
        case UnaryOp::PreIncrement:
        case UnaryOp::PreDecrement:
        case UnaryOp::PostIncrement:
        case UnaryOp::PostDecrement:
            generateIncrement(expression);
            return;
        }
    }

    void generateIncrement(const Expr& expression) {
        const bool decrement = expression.unaryOp == UnaryOp::PreDecrement ||
                               expression.unaryOp == UnaryOp::PostDecrement;
        const bool postfix = expression.unaryOp == UnaryOp::PostIncrement ||
                             expression.unaryOp == UnaryOp::PostDecrement;
        const TypePtr type = expression.left ? expression.left->type : nullptr;
        if (!type) {
            return;
        }
        if (type->isFloating()) {
            diagnostics_.error(expression.location, "floating-point increment is not supported");
            return;
        }
        int step = 1;
        if (type->isPointer() && type->element) {
            step = static_cast<int>(std::max<std::size_t>(1, types_.sizeOf(type->element)));
        }
        generateAddress(*expression.left);
        emit("PUSH R0");
        emitLoadFromR0(type);
        emit("PUSH R0");
        emit(concat(decrement ? "SUB R0, R0, #" : "ADD R0, R0, #", step));
        emit("POP R2");
        emit("POP R1");
        emitStoreToR1(type);
        emitExtendR0(type);
        if (postfix) {
            emit("MOV R0, R2");
        }
    }

    void generateBinary(const Expr& expression) {
        if (!expression.left || !expression.right) {
            return;
        }
        if (expression.binaryOp == BinaryOp::LogicalAnd) {
            generateLogicalAnd(expression);
            return;
        }
        if (expression.binaryOp == BinaryOp::LogicalOr) {
            generateLogicalOr(expression);
            return;
        }
        const bool floatingOperands =
            (expression.left->type && expression.left->type->isFloating()) ||
            (expression.right->type && expression.right->type->isFloating());
        if ((expression.type && expression.type->isFloating()) || floatingOperands) {
            generateFloatingBinary(expression);
            return;
        }

        const TypePtr leftType = expression.left->type;
        const TypePtr rightType = expression.right->type;
        const bool leftPointer = leftType && (leftType->isPointer() || leftType->isArray());
        const bool rightPointer = rightType && (rightType->isPointer() || rightType->isArray());

        if ((expression.binaryOp == BinaryOp::Add || expression.binaryOp == BinaryOp::Subtract) &&
            (leftPointer || rightPointer) && expression.type && expression.type->isPointer()) {
            TypePtr pointerType = leftPointer ? types_.decay(leftType) : types_.decay(rightType);
            const std::size_t elementSize =
                pointerType && pointerType->element ? types_.sizeOf(pointerType->element) : 1;
            if (leftPointer && !rightPointer) {
                generateExpression(*expression.left);
                emit("PUSH R0");
                generateExpression(*expression.right);
                if (elementSize > 1) {
                    emit(concat("MUL R0, R0, #", elementSize));
                }
                emit("POP R1");
                emit("ADD R0, R1, R0");
                return;
            }
            if (rightPointer && !leftPointer && expression.binaryOp == BinaryOp::Add) {
                generateExpression(*expression.right);
                emit("PUSH R0");
                generateExpression(*expression.left);
                if (elementSize > 1) {
                    emit(concat("MUL R0, R0, #", elementSize));
                }
                emit("POP R1");
                emit("ADD R0, R1, R0");
                return;
            }
            if (leftPointer && rightPointer && expression.binaryOp == BinaryOp::Subtract) {
                generateExpression(*expression.left);
                emit("PUSH R0");
                generateExpression(*expression.right);
                emit("POP R1");
                emit("SUB R0, R1, R0");
                if (elementSize > 1) {
                    emit(concat("DIVS R0, R0, #", elementSize));
                }
                return;
            }
        }

        generateExpression(*expression.left);
        emit("PUSH R0");
        generateExpression(*expression.right);
        emit("POP R1");
        const bool comparison = expression.binaryOp == BinaryOp::Less ||
                                expression.binaryOp == BinaryOp::Greater ||
                                expression.binaryOp == BinaryOp::LessEqual ||
                                expression.binaryOp == BinaryOp::GreaterEqual;
        emitIntegerBinary(expression.binaryOp, comparison ? leftType : expression.type);
    }

    void emitIntegerBinary(BinaryOp operation, TypePtr type) {
        const bool isUnsigned = type && type->isUnsignedInteger();
        switch (operation) {
        case BinaryOp::Add:
            emit("ADD R0, R1, R0");
            return;
        case BinaryOp::Subtract:
            emit("SUB R0, R1, R0");
            return;
        case BinaryOp::Multiply:
            emit("MUL R0, R1, R0");
            return;
        case BinaryOp::Divide:
            emit(isUnsigned ? "DIV R0, R1, R0" : "DIVS R0, R1, R0");
            return;
        case BinaryOp::Modulo:
            emit(isUnsigned ? "MOD R0, R1, R0" : "MODS R0, R1, R0");
            return;
        case BinaryOp::ShiftLeft:
            emit("SHL R0, R1, R0");
            return;
        case BinaryOp::ShiftRight:
            emit(isUnsigned ? "SHR R0, R1, R0" : "SAR R0, R1, R0");
            return;
        case BinaryOp::BitwiseAnd:
            emit("AND R0, R1, R0");
            return;
        case BinaryOp::BitwiseOr:
            emit("OR R0, R1, R0");
            return;
        case BinaryOp::BitwiseXor:
            emit("XOR R0, R1, R0");
            return;
        case BinaryOp::Less:
            emit(isUnsigned ? "SLTU R0, R1, R0" : "SLT R0, R1, R0");
            return;
        case BinaryOp::Greater:
            emit(isUnsigned ? "SLTU R0, R0, R1" : "SLT R0, R0, R1");
            return;
        case BinaryOp::LessEqual:
            emit(isUnsigned ? "SLTU R0, R0, R1" : "SLT R0, R0, R1");
            emit("XOR R0, R0, #1");
            return;
        case BinaryOp::GreaterEqual:
            emit(isUnsigned ? "SLTU R0, R1, R0" : "SLT R0, R1, R0");
            emit("XOR R0, R0, #1");
            return;
        case BinaryOp::Equal:
            emit("XOR R0, R1, R0");
            emit("SLTU R0, R0, #1");
            return;
        case BinaryOp::NotEqual:
            emit("XOR R0, R1, R0");
            emit("SLTU R0, R0, #1");
            emit("XOR R0, R0, #1");
            return;
        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr:
            return;
        }
    }

    void generateLogicalAnd(const Expr& expression) {
        const std::string falseLabel = newLabel("and_false");
        const std::string endLabel = newLabel("and_end");
        generateCondition(*expression.left);
        emit("JZ " + falseLabel);
        generateCondition(*expression.right);
        emit("JZ " + falseLabel);
        emit("MOV R0, #1");
        emit("JMP " + endLabel);
        text_ << falseLabel << ":\n";
        emit("MOV R0, #0");
        text_ << endLabel << ":\n";
    }

    void generateLogicalOr(const Expr& expression) {
        const std::string trueLabel = newLabel("or_true");
        const std::string endLabel = newLabel("or_end");
        generateCondition(*expression.left);
        emit("JNZ " + trueLabel);
        generateCondition(*expression.right);
        emit("JNZ " + trueLabel);
        emit("MOV R0, #0");
        emit("JMP " + endLabel);
        text_ << trueLabel << ":\n";
        emit("MOV R0, #1");
        text_ << endLabel << ":\n";
    }

    void generateFloatingBinary(const Expr& expression) {
        TypePtr type = expression.type;
        if (!type || !type->isFloating()) {
            type = (expression.left->type && expression.left->type->isFloating())
                       ? expression.left->type
                       : expression.right->type;
        }
        const std::string suffix = floatSuffix(type);
        const std::string width = floatWidth(type);
        generateExpression(*expression.left);
        emit("ADJSP #-8");
        emit(concat("FSTORE.", width, " F0, [SP]"));
        generateExpression(*expression.right);
        emit(concat("FLOAD.", width, " F1, [SP]"));
        emit("ADJSP #8");

        switch (expression.binaryOp) {
        case BinaryOp::Add:
            emit(concat("FADD.", suffix, " F0, F1, F0"));
            return;
        case BinaryOp::Subtract:
            emit(concat("FSUB.", suffix, " F0, F1, F0"));
            return;
        case BinaryOp::Multiply:
            emit(concat("FMUL.", suffix, " F0, F1, F0"));
            return;
        case BinaryOp::Divide:
            emit(concat("FDIV.", suffix, " F0, F1, F0"));
            return;
        case BinaryOp::Less:
            emit(concat("FLT.", suffix, " R0, F1, F0"));
            return;
        case BinaryOp::Greater:
            emit(concat("FLT.", suffix, " R0, F0, F1"));
            return;
        case BinaryOp::LessEqual:
            emit(concat("FLE.", suffix, " R0, F1, F0"));
            return;
        case BinaryOp::GreaterEqual:
            emit(concat("FLE.", suffix, " R0, F0, F1"));
            return;
        case BinaryOp::Equal:
            emit(concat("FEQ.", suffix, " R0, F1, F0"));
            return;
        case BinaryOp::NotEqual:
            emit(concat("FEQ.", suffix, " R0, F1, F0"));
            emit("XOR R0, R0, #1");
            return;
        default:
            diagnostics_.error(expression.location, "unsupported floating-point operator");
            return;
        }
    }

    void generateAssignment(const Expr& expression) {
        if (!expression.left || !expression.right) {
            return;
        }
        const TypePtr target = expression.left->type;
        if (!target) {
            return;
        }
        if (target->isRecord()) {
            generateStructAssignment(expression);
            return;
        }
        if (expression.assignOp == AssignOp::Assign) {
            generateAddress(*expression.left);
            emit("PUSH R0");
            generateExpression(*expression.right);
            emit("POP R1");
            emitStoreToR1(target);
            emitExtendR0(target);
            return;
        }

        if (target->isFloating()) {
            diagnostics_.error(expression.location,
                               "floating-point compound assignment is not supported");
            return;
        }
        generateAddress(*expression.left);
        emit("PUSH R0");
        emitLoadFromR0(target);
        emit("PUSH R0");
        generateExpression(*expression.right);
        emit("POP R1");
        if (target->isPointer() && (expression.assignOp == AssignOp::AddAssign ||
                                    expression.assignOp == AssignOp::SubtractAssign)) {
            int size = 1;
            if (target->element) {
                size = static_cast<int>(std::max<std::size_t>(1, types_.sizeOf(target->element)));
            }
            if (size > 1) {
                emit(concat("MUL R0, R0, #", size));
            }
            emit(expression.assignOp == AssignOp::AddAssign ? "ADD R0, R1, R0" : "SUB R0, R1, R0");
        } else {
            emitIntegerBinary(assignToBinary(expression.assignOp), target);
        }
        emit("POP R1");
        emitStoreToR1(target);
        emitExtendR0(target);
    }

    [[nodiscard]] static BinaryOp assignToBinary(AssignOp operation) noexcept {
        switch (operation) {
        case AssignOp::AddAssign:
            return BinaryOp::Add;
        case AssignOp::SubtractAssign:
            return BinaryOp::Subtract;
        case AssignOp::MultiplyAssign:
            return BinaryOp::Multiply;
        case AssignOp::DivideAssign:
            return BinaryOp::Divide;
        case AssignOp::ModuloAssign:
            return BinaryOp::Modulo;
        case AssignOp::ShiftLeftAssign:
            return BinaryOp::ShiftLeft;
        case AssignOp::ShiftRightAssign:
            return BinaryOp::ShiftRight;
        case AssignOp::AndAssign:
            return BinaryOp::BitwiseAnd;
        case AssignOp::OrAssign:
            return BinaryOp::BitwiseOr;
        case AssignOp::XorAssign:
            return BinaryOp::BitwiseXor;
        case AssignOp::Assign:
            return BinaryOp::Add;
        }
        return BinaryOp::Add;
    }

    void generateStructAssignment(const Expr& expression) {
        const std::size_t size = types_.sizeOf(expression.left->type);
        const std::string loop = newLabel("copy");
        generateAddress(*expression.left);
        emit("PUSH R0");
        generateExpression(*expression.right);
        emit("MOV R2, R0");
        emit("POP R1");
        emit(concat("MOV R3, #", size));
        text_ << loop << ":\n";
        emit("LOAD8U R0, [R2]");
        emit("STORE8 R0, [R1]");
        emit("ADD R2, R2, #1");
        emit("ADD R1, R1, #1");
        emit("SUB R3, R3, #1");
        emit("JNZ " + loop);
    }

    void generateConditional(const Expr& expression) {
        if (!expression.left || !expression.right || !expression.third) {
            return;
        }
        const std::string elseLabel = newLabel("cond_else");
        const std::string endLabel = newLabel("cond_end");
        generateCondition(*expression.left);
        emit("JZ " + elseLabel);
        generateExpression(*expression.right);
        emit("JMP " + endLabel);
        text_ << elseLabel << ":\n";
        generateExpression(*expression.third);
        text_ << endLabel << ":\n";
    }

    void generateCondition(const Expr& expression) {
        generateExpression(expression);
        if (expression.type && expression.type->isFloating()) {
            emit(concat("FCVT.W.", floatSuffix(expression.type), " R0, F0"));
        }
        emit("CMP R0, #0");
    }

    void generateCall(const Expr& expression) {
        if (!expression.left) {
            return;
        }
        const bool direct = expression.left->kind == Expr::Kind::Identifier &&
                            expression.left->symbol != nullptr &&
                            expression.left->symbol->isFunction;
        if (expression.arguments.size() > 4) {
            diagnostics_.error(expression.location,
                               "calls with more than four arguments are not supported");
            return;
        }
        if (!direct) {
            generateExpression(*expression.left);
            emit("ADJSP #-4");
            emit("STORE R0, [SP]");
        }
        for (std::size_t index = expression.arguments.size(); index > 0; --index) {
            const std::unique_ptr<Expr>& argument = expression.arguments[index - 1];
            emit("ADJSP #-8");
            generateExpression(*argument);
            if (argument->type && argument->type->isFloating()) {
                emit(concat("FSTORE.", floatWidth(argument->type), " F0, [SP]"));
            } else {
                emit("STORE R0, [SP]");
            }
        }
        int integerIndex = 0;
        int floatIndex = 0;
        for (std::size_t index = 0; index < expression.arguments.size(); ++index) {
            const std::unique_ptr<Expr>& argument = expression.arguments[index];
            const int offset = static_cast<int>(index * 8);
            const std::string address = concat("[SP + ", offset, "]");
            if (argument->type && argument->type->isFloating()) {
                emit(concat("FLOAD.", floatWidth(argument->type), " F", floatIndex, ", ", address));
                ++floatIndex;
            } else {
                emit(concat("LOAD R", integerIndex, ", ", address));
                ++integerIndex;
            }
        }
        const std::size_t argumentBytes = expression.arguments.size() * 8;
        if (direct) {
            emit("CALL " + expression.left->symbol->asmName);
            if (argumentBytes > 0) {
                emit(concat("ADJSP #", argumentBytes));
            }
        } else {
            const int calleeOffset = static_cast<int>(argumentBytes);
            emit(concat("LOAD R4, [SP + ", calleeOffset, "]"));
            emit("CALL R4");
            emit(concat("ADJSP #", argumentBytes + 4));
        }
        if (expression.type && expression.type->isInteger() && types_.sizeOf(expression.type) < 4) {
            emitExtendR0(expression.type);
        }
    }

    void generateCast(const Expr& expression) {
        if (!expression.left) {
            return;
        }
        generateExpression(*expression.left);
        const TypePtr from = expression.left->type;
        const TypePtr to = expression.castType;
        if (!from || !to || to->isVoid()) {
            return;
        }
        if (from->isFloating() && to->isFloating()) {
            if (from->kind != to->kind) {
                emit(concat("FCVT.", floatSuffix(to), ".", floatSuffix(from), " F0, F0"));
            }
            return;
        }
        if (from->isFloating() && to->isInteger()) {
            emit(concat("FCVT.", to->isUnsignedInteger() ? "WU" : "W", ".", floatSuffix(from),
                        " R0, F0"));
            emitExtendR0(to);
            return;
        }
        if (from->isInteger() && to->isFloating()) {
            emit(concat("FCVT.", floatSuffix(to), ".", from->isUnsignedInteger() ? "WU" : "W",
                        " F0, R0"));
            return;
        }
        if (to->isInteger()) {
            emitExtendR0(to);
            return;
        }
        // Pointer and integer-address conversions are bit-preserving.
    }

    // -- statements ---------------------------------------------------------

    void generateStatement(const Stmt& statement) {
        switch (statement.kind) {
        case Stmt::Kind::Empty:
            return;
        case Stmt::Kind::Compound:
            for (const std::unique_ptr<Stmt>& child : statement.children) {
                if (child) {
                    generateStatement(*child);
                }
            }
            return;
        case Stmt::Kind::Declaration:
            if (statement.declaration) {
                generateLocalDeclaration(*statement.declaration);
            }
            return;
        case Stmt::Kind::Expression:
            if (statement.expression) {
                generateExpression(*statement.expression);
            }
            return;
        case Stmt::Kind::If: {
            if (!statement.condition) {
                return;
            }
            const std::string elseLabel = newLabel("if_else");
            const std::string endLabel = newLabel("if_end");
            generateCondition(*statement.condition);
            emit("JZ " + elseLabel);
            if (statement.body) {
                generateStatement(*statement.body);
            }
            emit("JMP " + endLabel);
            text_ << elseLabel << ":\n";
            if (statement.elseBody) {
                generateStatement(*statement.elseBody);
            }
            text_ << endLabel << ":\n";
            return;
        }
        case Stmt::Kind::While: {
            const std::string startLabel = newLabel("while");
            const std::string endLabel = newLabel("while_end");
            text_ << startLabel << ":\n";
            if (statement.condition) {
                generateCondition(*statement.condition);
                emit("JZ " + endLabel);
            }
            breakTargets_.push_back(endLabel);
            continueTargets_.push_back(startLabel);
            if (statement.body) {
                generateStatement(*statement.body);
            }
            continueTargets_.pop_back();
            breakTargets_.pop_back();
            emit("JMP " + startLabel);
            text_ << endLabel << ":\n";
            return;
        }
        case Stmt::Kind::DoWhile: {
            const std::string startLabel = newLabel("do");
            const std::string continueLabel = newLabel("do_continue");
            const std::string endLabel = newLabel("do_end");
            text_ << startLabel << ":\n";
            breakTargets_.push_back(endLabel);
            continueTargets_.push_back(continueLabel);
            if (statement.body) {
                generateStatement(*statement.body);
            }
            continueTargets_.pop_back();
            breakTargets_.pop_back();
            text_ << continueLabel << ":\n";
            if (statement.condition) {
                generateCondition(*statement.condition);
                emit("JNZ " + startLabel);
            }
            text_ << endLabel << ":\n";
            return;
        }
        case Stmt::Kind::For: {
            const std::string startLabel = newLabel("for");
            const std::string continueLabel = newLabel("for_continue");
            const std::string endLabel = newLabel("for_end");
            if (statement.initStatement) {
                generateStatement(*statement.initStatement);
            }
            text_ << startLabel << ":\n";
            if (statement.condition) {
                generateCondition(*statement.condition);
                emit("JZ " + endLabel);
            }
            breakTargets_.push_back(endLabel);
            continueTargets_.push_back(continueLabel);
            if (statement.body) {
                generateStatement(*statement.body);
            }
            continueTargets_.pop_back();
            breakTargets_.pop_back();
            text_ << continueLabel << ":\n";
            if (statement.increment) {
                generateExpression(*statement.increment);
            }
            emit("JMP " + startLabel);
            text_ << endLabel << ":\n";
            return;
        }
        case Stmt::Kind::Switch:
            generateSwitch(statement);
            return;
        case Stmt::Kind::Case:
        case Stmt::Kind::Default: {
            const auto found = caseLabels_.find(&statement);
            if (found != caseLabels_.end()) {
                text_ << found->second << ":\n";
            }
            if (statement.body) {
                generateStatement(*statement.body);
            }
            return;
        }
        case Stmt::Kind::Break:
            if (breakTargets_.empty()) {
                diagnostics_.error(statement.location, "internal: break without a target");
            } else {
                emit("JMP " + breakTargets_.back());
            }
            return;
        case Stmt::Kind::Continue:
            if (continueTargets_.empty()) {
                diagnostics_.error(statement.location, "internal: continue without a target");
            } else {
                emit("JMP " + continueTargets_.back());
            }
            return;
        case Stmt::Kind::Return:
            if (statement.expression) {
                generateExpression(*statement.expression);
            }
            emit("JMP " + returnLabel_);
            return;
        case Stmt::Kind::Goto:
            emit("JMP " + userLabel(statement.label));
            return;
        case Stmt::Kind::Label:
            text_ << userLabel(statement.label) << ":\n";
            if (statement.body) {
                generateStatement(*statement.body);
            }
            return;
        }
    }

    void generateSwitch(const Stmt& statement) {
        if (!statement.body) {
            return;
        }
        std::vector<const Stmt*> cases;
        const Stmt* defaultStatement = nullptr;
        collectSwitchCases(*statement.body, cases, defaultStatement);
        const std::string endLabel = newLabel("switch_end");
        for (const Stmt* label : cases) {
            caseLabels_[label] = newLabel("case");
        }
        if (defaultStatement != nullptr) {
            caseLabels_[defaultStatement] = newLabel("default");
        }

        if (statement.expression) {
            generateExpression(*statement.expression);
        }
        for (const Stmt* label : cases) {
            const auto value = label->caseValue.value_or(0);
            emit(concat("CMP R0, #", static_cast<std::int32_t>(value)));
            emit("JZ " + caseLabels_[label]);
        }
        if (defaultStatement != nullptr) {
            emit("JMP " + caseLabels_[defaultStatement]);
        } else {
            emit("JMP " + endLabel);
        }
        breakTargets_.push_back(endLabel);
        generateStatement(*statement.body);
        breakTargets_.pop_back();
        text_ << endLabel << ":\n";
    }

    void collectSwitchCases(const Stmt& statement, std::vector<const Stmt*>& cases,
                            const Stmt*& defaultStatement) {
        switch (statement.kind) {
        case Stmt::Kind::Case:
            cases.push_back(&statement);
            if (statement.body) {
                collectSwitchCases(*statement.body, cases, defaultStatement);
            }
            return;
        case Stmt::Kind::Default:
            defaultStatement = &statement;
            if (statement.body) {
                collectSwitchCases(*statement.body, cases, defaultStatement);
            }
            return;
        case Stmt::Kind::Compound:
            for (const std::unique_ptr<Stmt>& child : statement.children) {
                if (child) {
                    collectSwitchCases(*child, cases, defaultStatement);
                }
            }
            return;
        case Stmt::Kind::If:
            if (statement.body) {
                collectSwitchCases(*statement.body, cases, defaultStatement);
            }
            if (statement.elseBody) {
                collectSwitchCases(*statement.elseBody, cases, defaultStatement);
            }
            return;
        case Stmt::Kind::While:
        case Stmt::Kind::DoWhile:
        case Stmt::Kind::For:
        case Stmt::Kind::Label:
            if (statement.body) {
                collectSwitchCases(*statement.body, cases, defaultStatement);
            }
            return;
        case Stmt::Kind::Switch:
            // A nested switch owns its own case labels.
            return;
        default:
            return;
        }
    }

    void generateLocalDeclaration(const Declaration& declaration) {
        for (const DeclaratorInfo& declarator : declaration.declarators) {
            if (!declarator.symbol || !declarator.initializer) {
                continue;
            }
            if (declarator.symbol->isGlobal) {
                continue;
            }
            generateLocalInitializer(*declarator.symbol, declarator.type, *declarator.initializer, 0);
        }
    }

    void zeroLocal(int offset, std::size_t size) {
        std::size_t position = 0;
        while (position + 4 <= size) {
            emit("MOV R0, #0");
            emit(concat("STORE R0, [R7 + ", offset + static_cast<int>(position), "]"));
            position += 4;
        }
        while (position < size) {
            emit("MOV R0, #0");
            emit(concat("STORE8 R0, [R7 + ", offset + static_cast<int>(position), "]"));
            ++position;
        }
    }

    void generateLocalInitializer(const Symbol& symbol, const TypePtr& type,
                                  const Initializer& initializer, int baseOffset) {
        const int offset = symbol.frameOffset + baseOffset;
        if (!type) {
            return;
        }
        if (!initializer.isList) {
            if (!initializer.expression) {
                return;
            }
            if (type->isArray() && type->element && type->element->kind == TypeKind::Char &&
                initializer.expression->kind == Expr::Kind::StringLiteral) {
                const std::size_t size = types_.sizeOf(type);
                zeroLocal(offset, size);
                for (std::size_t index = 0; index < initializer.expression->stringValue.size() &&
                                            index < size;
                     ++index) {
                    emit(concat("MOV R0, #",
                                static_cast<unsigned>(
                                    static_cast<unsigned char>(initializer.expression->stringValue[index]))));
                    emit(concat("STORE8 R0, [R7 + ",
                                offset + static_cast<int>(index), "]"));
                }
                return;
            }
            if (type->isArray() || type->isRecord()) {
                diagnostics_.error(initializer.location,
                                   "aggregate local initializer requires a brace list");
                return;
            }
            generateExpression(*initializer.expression);
            emitStoreDirect(type, offset);
            emitExtendR0(type);
            return;
        }

        if (type->isArray()) {
            const std::size_t elementSize =
                type->element ? types_.sizeOf(type->element) : 0;
            zeroLocal(offset, types_.sizeOf(type));
            const std::size_t limit = type->arrayLength < 0
                                          ? initializer.elements.size()
                                          : static_cast<std::size_t>(type->arrayLength);
            for (std::size_t index = 0; index < initializer.elements.size() && index < limit;
                 ++index) {
                if (initializer.elements[index]) {
                    generateLocalInitializer(symbol, type->element, *initializer.elements[index],
                                             baseOffset + static_cast<int>(index * elementSize));
                }
            }
            return;
        }

        if (type->isRecord() && type->record) {
            zeroLocal(offset, types_.sizeOf(type));
            const RecordInfo& record = *type->record;
            const std::size_t limit = record.isUnion ? 1U : record.members.size();
            for (std::size_t index = 0; index < initializer.elements.size() && index < limit;
                 ++index) {
                if (!initializer.elements[index]) {
                    continue;
                }
                const RecordMember& member = record.members[index];
                generateLocalInitializer(symbol, member.type, *initializer.elements[index],
                                         baseOffset + member.offset);
            }
            return;
        }

        if (initializer.elements.size() == 1 && initializer.elements.front()) {
            generateLocalInitializer(symbol, type, *initializer.elements.front(), baseOffset);
        } else {
            diagnostics_.error(initializer.location, "invalid scalar initializer");
        }
    }

    // -- functions ----------------------------------------------------------

    void generateFunction(const FunctionDefinition& function) {
        Symbol* symbol = function.symbol;
        DeclaratorInfo* declarator = function.declarator;
        if (symbol == nullptr || declarator == nullptr || !declarator->body) {
            return;
        }
        ++functionCounter_;
        returnLabel_ = newLabel("return");
        const int frame = symbol->frameSize;

        text_ << "\n";
        if (!symbol->isStatic) {
            text_ << "    .global " << symbol->asmName << "\n";
        }
        text_ << symbol->asmName << ":\n";
        emit("PUSH R7");
        if (frame > 0) {
            emit(concat("ADJSP #-", frame));
        }
        emit("LEA R7, [SP]");

        int integerIndex = 0;
        int floatIndex = 0;
        for (const Parameter& parameter : declarator->parameters) {
            if (!parameter.symbol) {
                continue;
            }
            const TypePtr& type = parameter.type;
            if (type && type->isFloating()) {
                if (floatIndex < 8) {
                    emit(concat("FSTORE.", floatWidth(type), " F", floatIndex, ", [R7 + ",
                                parameter.symbol->frameOffset, "]"));
                } else {
                    diagnostics_.error(parameter.location,
                                       "floating-point stack parameters are not supported");
                }
                ++floatIndex;
            } else {
                if (integerIndex < 4) {
                    emit(concat("STORE R", integerIndex, ", [R7 + ",
                                parameter.symbol->frameOffset, "]"));
                } else {
                    diagnostics_.error(parameter.location,
                                       "integer stack parameters are not supported");
                }
                ++integerIndex;
            }
        }

        generateStatement(*declarator->body);
        text_ << returnLabel_ << ":\n";
        if (frame > 0) {
            emit(concat("ADJSP #", frame));
        }
        emit("POP R7");
        emit("RET");
    }

    // -- global data --------------------------------------------------------

    void generateGlobal(const Symbol& symbol) {
        if (!symbol.type) {
            return;
        }
        const std::size_t alignment = std::max<std::size_t>(1, types_.alignOf(symbol.type));
        data_ << "    .align " << alignment << "\n";
        if (!symbol.isStatic) {
            data_ << "    .global " << symbol.asmName << "\n";
        }
        data_ << symbol.asmName << ":\n";

        const std::size_t size = types_.sizeOf(symbol.type);
        if (symbol.initializer == nullptr) {
            data_ << "    .space " << size << "\n";
            return;
        }
        std::size_t offset = 0;
        generateGlobalInitializer(symbol.type, symbol.initializer, offset);
        if (offset < size) {
            data_ << "    .space " << (size - offset) << "\n";
        }
    }

    void generateGlobalInitializer(const TypePtr& type, const Initializer* initializer,
                                   std::size_t& offset) {
        if (!type) {
            return;
        }
        if (type->isArray()) {
            const std::size_t elementSize = type->element ? types_.sizeOf(type->element) : 0;
            if (initializer != nullptr && !initializer->isList && initializer->expression &&
                type->element && type->element->kind == TypeKind::Char &&
                initializer->expression->kind == Expr::Kind::StringLiteral) {
                for (const char character : initializer->expression->stringValue) {
                    emitData(concat(".byte ", static_cast<unsigned>(static_cast<unsigned char>(character))));
                    ++offset;
                }
                emitData(".byte 0");
                ++offset;
            } else if (initializer != nullptr && initializer->isList) {
                const std::size_t limit = type->arrayLength < 0
                                              ? initializer->elements.size()
                                              : static_cast<std::size_t>(type->arrayLength);
                for (std::size_t index = 0; index < initializer->elements.size() && index < limit;
                     ++index) {
                    if (initializer->elements[index]) {
                        generateGlobalInitializer(type->element, initializer->elements[index].get(),
                                                  offset);
                    }
                    (void)elementSize;
                }
            }
            const std::size_t total = types_.sizeOf(type);
            if (offset < total) {
                data_ << "    .space " << (total - offset) << "\n";
                offset = total;
            }
            return;
        }

        if (type->isRecord() && type->record) {
            const RecordInfo& record = *type->record;
            if (initializer == nullptr || !initializer->isList) {
                data_ << "    .space " << record.size << "\n";
                offset += static_cast<std::size_t>(record.size);
                return;
            }
            const std::size_t limit = record.isUnion ? 1U : record.members.size();
            for (std::size_t index = 0; index < limit; ++index) {
                const RecordMember& member = record.members[index];
                if (member.offset > static_cast<int>(offset)) {
                    data_ << "    .space " << (static_cast<std::size_t>(member.offset) - offset)
                          << "\n";
                    offset = static_cast<std::size_t>(member.offset);
                }
                if (index < initializer->elements.size() && initializer->elements[index]) {
                    generateGlobalInitializer(member.type, initializer->elements[index].get(),
                                              offset);
                } else {
                    const std::size_t memberSize = types_.sizeOf(member.type);
                    data_ << "    .space " << memberSize << "\n";
                    offset += memberSize;
                }
            }
            if (offset < static_cast<std::size_t>(record.size)) {
                data_ << "    .space " << (static_cast<std::size_t>(record.size) - offset) << "\n";
                offset = static_cast<std::size_t>(record.size);
            }
            return;
        }

        const std::size_t size = types_.sizeOf(type);
        if (initializer == nullptr) {
            data_ << "    .space " << size << "\n";
            offset += size;
            return;
        }
        if (initializer->isList) {
            if (initializer->elements.size() == 1 && initializer->elements.front()) {
                generateGlobalInitializer(type, initializer->elements.front().get(), offset);
            } else {
                data_ << "    .space " << size << "\n";
                offset += size;
            }
            return;
        }
        if (!initializer->expression) {
            data_ << "    .space " << size << "\n";
            offset += size;
            return;
        }
        generateGlobalScalar(type, *initializer->expression);
        offset += size;
    }

    void generateGlobalScalar(const TypePtr& type, const Expr& expression) {
        if (type->isFloating()) {
            long double value = 0.0L;
            if (const auto constant = constantFloatingValue(expression); constant.has_value()) {
                value = *constant;
            }
            if (isFloatType(type)) {
                const float converted = static_cast<float>(value);
                std::uint32_t bits = 0;
                std::memcpy(&bits, &converted, sizeof(bits));
                emitData(".word " + hex32(bits));
            } else {
                const double converted = static_cast<double>(value);
                std::uint64_t bits = 0;
                std::memcpy(&bits, &converted, sizeof(bits));
                emitData(".word " + hex32(static_cast<std::uint32_t>(bits & 0xFFFFFFFFU)) + ", " +
                         hex32(static_cast<std::uint32_t>(bits >> 32)));
            }
            return;
        }
        if (type->isPointer()) {
            if (const auto label = addressLabel(expression); label.has_value()) {
                emitData(".word " + *label);
            } else if (expression.constantValue.has_value()) {
                emitData(".word " + std::to_string(
                                        static_cast<std::int32_t>(*expression.constantValue)));
            } else {
                emitData(".word 0");
            }
            return;
        }
        const std::int64_t value = expression.constantValue.value_or(0);
        const std::size_t size = types_.sizeOf(type);
        emitIntegerBytes(value, size);
    }

    [[nodiscard]] std::optional<std::string> addressLabel(const Expr& expression) {
        if (expression.kind == Expr::Kind::StringLiteral) {
            return labelForString(expression.stringValue);
        }
        if (expression.kind == Expr::Kind::Cast && expression.left) {
            return addressLabel(*expression.left);
        }
        if (expression.kind == Expr::Kind::Unary && expression.unaryOp == UnaryOp::AddressOf &&
            expression.left) {
            if (expression.left->kind == Expr::Kind::Identifier && expression.left->symbol) {
                return expression.left->symbol->asmName;
            }
        }
        if (expression.kind == Expr::Kind::Identifier && expression.symbol &&
            expression.symbol->isGlobal) {
            return expression.symbol->asmName;
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::optional<long double> constantFloatingValue(const Expr& expression) {
        if (expression.kind == Expr::Kind::FloatingLiteral) {
            return expression.floatingValue;
        }
        if (expression.kind == Expr::Kind::IntegerLiteral ||
            expression.kind == Expr::Kind::CharacterLiteral) {
            return static_cast<long double>(expression.integerValue);
        }
        if (expression.kind == Expr::Kind::Cast && expression.left) {
            return constantFloatingValue(*expression.left);
        }
        if (expression.kind == Expr::Kind::Unary && expression.unaryOp == UnaryOp::Minus &&
            expression.left) {
            const auto value = constantFloatingValue(*expression.left);
            return value.has_value() ? std::optional<long double>(-*value) : std::nullopt;
        }
        return std::nullopt;
    }

    void emitIntegerBytes(std::int64_t value, std::size_t size) {
        const std::uint64_t bits = static_cast<std::uint64_t>(value);
        if (size == 1) {
            emitData(concat(".byte ", bits & 0xFFU));
            return;
        }
        if (size == 2) {
            emitData(concat(".byte ", bits & 0xFFU, ", ", (bits >> 8) & 0xFFU));
            return;
        }
        if (size == 4) {
            emitData(".word " + hex32(static_cast<std::uint32_t>(bits)));
            return;
        }
        emitData(".word " + hex32(static_cast<std::uint32_t>(bits & 0xFFFFFFFFU)) + ", " +
                 hex32(static_cast<std::uint32_t>(bits >> 32)));
    }

    SemanticProgram& program_;
    TypeContext& types_;
    Diagnostics& diagnostics_;
    std::ostringstream text_;
    std::ostringstream data_;
    int labelCounter_ = 0;
    int functionCounter_ = 0;
    int stringCounter_ = 0;
    int floatCounter_ = 0;
    std::string returnLabel_;
    std::vector<std::string> breakTargets_;
    std::vector<std::string> continueTargets_;
    std::map<const Stmt*, std::string> caseLabels_;
    std::map<std::string, std::string> stringLabels_;
    std::vector<std::pair<std::string, std::string>> strings_;
    std::map<std::pair<bool, std::uint64_t>, std::string> floatLabels_;
    std::vector<FloatLiteral> floats_;
};

// ---------------------------------------------------------------------------
// Driver
// ---------------------------------------------------------------------------

struct DriverOptions {
    fs::path input;
    fs::path output;
    std::vector<fs::path> includeDirectories;
    std::vector<std::string> defines;
    std::vector<std::string> undefines;
    bool preprocessOnly = false;
    bool dumpTokens = false;
    bool dumpAst = false;
    bool showHelp = false;
    bool showVersion = false;
};

[[nodiscard]] std::string helpText() {
    return R"(kevinc — C89 to KR32 assembly compiler

Usage: kevinc [options] <input.c>

Options:
  -o <file>          write assembly to <file> (default: <input>.S)
  -I <dir>           add an include search directory
  -D <name[=value]>  define a preprocessor macro
  -U <name>          undefine a preprocessor macro
  -E                 preprocess only; write tokens to stdout
  --dump-tokens      print the preprocessed token stream and stop
  --dump-ast         print the parsed translation unit after semantic analysis
  --help, -h         show this help
  --version          show the version

The compiler accepts C89 with a few documented conveniences. Output is KR32
assembly text; assembling or linking it is a separate step.
)";
}

[[nodiscard]] bool startsWith(std::string_view text, std::string_view prefix) noexcept {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] std::optional<DriverOptions> parseDriverOptions(int argc, char** argv,
                                                              std::string& error) {
    DriverOptions options;
    bool endOfOptions = false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (!endOfOptions && argument == "--") {
            endOfOptions = true;
            continue;
        }
        if (!endOfOptions && (argument == "--help" || argument == "-h")) {
            options.showHelp = true;
            continue;
        }
        if (!endOfOptions && argument == "--version") {
            options.showVersion = true;
            continue;
        }
        if (!endOfOptions && argument == "-E") {
            options.preprocessOnly = true;
            continue;
        }
        if (!endOfOptions && argument == "--dump-tokens") {
            options.dumpTokens = true;
            continue;
        }
        if (!endOfOptions && argument == "--dump-ast") {
            options.dumpAst = true;
            continue;
        }
        if (!endOfOptions && (argument == "-S" || argument == "-std=c89" ||
                              argument == "-std=iso9899:1990")) {
            continue;
        }
        if (!endOfOptions && argument == "-o") {
            if (++index >= argc) {
                error = "-o requires a file name";
                return std::nullopt;
            }
            options.output = argv[index];
            continue;
        }
        if (!endOfOptions && startsWith(argument, "-o") && argument.size() > 2) {
            options.output = argument.substr(2);
            continue;
        }
        if (!endOfOptions && argument == "-I") {
            if (++index >= argc) {
                error = "-I requires a directory";
                return std::nullopt;
            }
            options.includeDirectories.emplace_back(argv[index]);
            continue;
        }
        if (!endOfOptions && startsWith(argument, "-I") && argument.size() > 2) {
            options.includeDirectories.emplace_back(argument.substr(2));
            continue;
        }
        if (!endOfOptions && argument == "-D") {
            if (++index >= argc) {
                error = "-D requires a macro definition";
                return std::nullopt;
            }
            options.defines.emplace_back(argv[index]);
            continue;
        }
        if (!endOfOptions && startsWith(argument, "-D") && argument.size() > 2) {
            options.defines.emplace_back(argument.substr(2));
            continue;
        }
        if (!endOfOptions && argument == "-U") {
            if (++index >= argc) {
                error = "-U requires a macro name";
                return std::nullopt;
            }
            options.undefines.emplace_back(argv[index]);
            continue;
        }
        if (!endOfOptions && startsWith(argument, "-U") && argument.size() > 2) {
            options.undefines.emplace_back(argument.substr(2));
            continue;
        }
        if (!endOfOptions && !argument.empty() && argument.front() == '-') {
            error = concat("unknown option '", argument, "'");
            return std::nullopt;
        }
        if (!options.input.empty()) {
            error = "only one input file is supported";
            return std::nullopt;
        }
        options.input = argument;
    }
    if (options.showHelp || options.showVersion) {
        return options;
    }
    if (options.input.empty()) {
        error = "no input file";
        return std::nullopt;
    }
    if (options.output.empty()) {
        options.output = options.input;
        options.output.replace_extension(".S");
    }
    return options;
}

void printPreprocessedTokens(const std::vector<Token>& tokens, std::ostream& output) {
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::End) {
            break;
        }
        output << token.text << ' ';
    }
    output << '\n';
}

class AstDumper {
public:
    AstDumper(TypeContext& types, std::ostream& output) : types_(types), output_(output) {}

    void dump(const TranslationUnit& unit) {
        for (const std::unique_ptr<Declaration>& declaration : unit.declarations) {
            if (declaration) {
                dumpDeclaration(*declaration);
            }
        }
    }

private:
    std::ostream& indent() {
        output_ << std::string(depth_ * 2, ' ');
        return output_;
    }

    void dumpDeclaration(const Declaration& declaration) {
        indent() << "decl";
        if (declaration.isTypedef) {
            output_ << " typedef";
        }
        if (declaration.baseType) {
            output_ << " type=" << types_.describe(declaration.baseType);
        }
        output_ << '\n';
        ++depth_;
        for (const DeclaratorInfo& declarator : declaration.declarators) {
            indent() << "declarator " << declarator.name;
            if (declarator.type) {
                output_ << " : " << types_.describe(declarator.type);
            }
            output_ << '\n';
            if (declarator.initializer) {
                ++depth_;
                dumpInitializer(*declarator.initializer);
                --depth_;
            }
            if (declarator.body) {
                dumpStatement(*declarator.body);
            }
        }
        --depth_;
    }

    void dumpInitializer(const Initializer& initializer) {
        indent() << (initializer.isList ? "initializer-list" : "initializer") << '\n';
        ++depth_;
        if (initializer.expression) {
            dumpExpression(*initializer.expression);
        }
        for (const std::unique_ptr<Initializer>& element : initializer.elements) {
            if (element) {
                dumpInitializer(*element);
            }
        }
        --depth_;
    }

    void dumpStatement(const Stmt& statement) {
        indent();
        switch (statement.kind) {
        case Stmt::Kind::Empty:
            output_ << "empty\n";
            return;
        case Stmt::Kind::Expression:
            output_ << "expression-statement\n";
            break;
        case Stmt::Kind::Compound:
            output_ << "compound\n";
            break;
        case Stmt::Kind::Declaration:
            output_ << "declaration-statement\n";
            break;
        case Stmt::Kind::If:
            output_ << "if\n";
            break;
        case Stmt::Kind::While:
            output_ << "while\n";
            break;
        case Stmt::Kind::DoWhile:
            output_ << "do-while\n";
            break;
        case Stmt::Kind::For:
            output_ << "for\n";
            break;
        case Stmt::Kind::Switch:
            output_ << "switch\n";
            break;
        case Stmt::Kind::Case:
            output_ << "case";
            if (statement.caseValue.has_value()) {
                output_ << ' ' << *statement.caseValue;
            }
            output_ << '\n';
            break;
        case Stmt::Kind::Default:
            output_ << "default\n";
            break;
        case Stmt::Kind::Break:
            output_ << "break\n";
            return;
        case Stmt::Kind::Continue:
            output_ << "continue\n";
            return;
        case Stmt::Kind::Return:
            output_ << "return\n";
            break;
        case Stmt::Kind::Goto:
            output_ << "goto " << statement.label << '\n';
            return;
        case Stmt::Kind::Label:
            output_ << "label " << statement.label << '\n';
            break;
        }
        ++depth_;
        if (statement.declaration) {
            dumpDeclaration(*statement.declaration);
        }
        if (statement.expression) {
            dumpExpression(*statement.expression);
        }
        if (statement.condition) {
            dumpExpression(*statement.condition);
        }
        if (statement.increment) {
            dumpExpression(*statement.increment);
        }
        if (statement.initStatement) {
            dumpStatement(*statement.initStatement);
        }
        if (statement.body) {
            dumpStatement(*statement.body);
        }
        if (statement.elseBody) {
            indent() << "else\n";
            ++depth_;
            dumpStatement(*statement.elseBody);
            --depth_;
        }
        for (const std::unique_ptr<Stmt>& child : statement.children) {
            if (child) {
                dumpStatement(*child);
            }
        }
        --depth_;
    }

    using LiteralValue = std::variant<std::int64_t, long double, std::string>;

    [[nodiscard]] static LiteralValue literalValue(const Expr& expression) {
        if (expression.kind == Expr::Kind::FloatingLiteral) {
            return expression.floatingValue;
        }
        if (expression.kind == Expr::Kind::StringLiteral) {
            return expression.stringValue;
        }
        return expression.integerValue;
    }

    void dumpLiteral(const Expr& expression) {
        std::visit(
            [this](const auto& value) {
                using ValueType = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<ValueType, std::string>) {
                    output_ << '"' << value << '"';
                } else {
                    output_ << value;
                }
            },
            literalValue(expression));
    }

    void dumpExpression(const Expr& expression) {
        indent();
        switch (expression.kind) {
        case Expr::Kind::IntegerLiteral:
            output_ << "int ";
            dumpLiteral(expression);
            break;
        case Expr::Kind::FloatingLiteral:
            output_ << "float ";
            dumpLiteral(expression);
            break;
        case Expr::Kind::CharacterLiteral:
            output_ << "char ";
            dumpLiteral(expression);
            break;
        case Expr::Kind::StringLiteral:
            output_ << "string ";
            dumpLiteral(expression);
            break;
        case Expr::Kind::Identifier:
            output_ << "identifier " << expression.name;
            break;
        case Expr::Kind::Unary:
            output_ << "unary";
            break;
        case Expr::Kind::Binary:
            output_ << "binary";
            break;
        case Expr::Kind::Assignment:
            output_ << "assignment";
            break;
        case Expr::Kind::Conditional:
            output_ << "conditional";
            break;
        case Expr::Kind::Call:
            output_ << "call";
            break;
        case Expr::Kind::Member:
            output_ << "member " << expression.memberName;
            break;
        case Expr::Kind::Index:
            output_ << "index";
            break;
        case Expr::Kind::Cast:
            output_ << "cast";
            break;
        case Expr::Kind::SizeOf:
            output_ << "sizeof";
            break;
        case Expr::Kind::Comma:
            output_ << "comma";
            break;
        }
        if (expression.type) {
            output_ << " : " << types_.describe(expression.type);
        }
        output_ << '\n';
        ++depth_;
        if (expression.left) {
            dumpExpression(*expression.left);
        }
        if (expression.right) {
            dumpExpression(*expression.right);
        }
        if (expression.third) {
            dumpExpression(*expression.third);
        }
        for (const std::unique_ptr<Expr>& argument : expression.arguments) {
            if (argument) {
                dumpExpression(*argument);
            }
        }
        --depth_;
    }

    TypeContext& types_;
    std::ostream& output_;
    int depth_ = 0;
};

[[nodiscard]] bool writeOutputFile(const fs::path& path, std::string_view contents,
                                   std::string& error) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = concat("cannot open output file '", path.string(), "'");
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output) {
        error = concat("failed while writing '", path.string(), "'");
        return false;
    }
    return true;
}

int runCompiler(int argc, char** argv) {
    std::string optionError;
    const std::optional<DriverOptions> parsed = parseDriverOptions(argc, argv, optionError);
    if (!parsed.has_value()) {
        std::cerr << "kevinc: " << optionError << '\n';
        std::cerr << "Try 'kevinc --help'.\n";
        return 2;
    }
    const DriverOptions& options = *parsed;
    if (options.showHelp) {
        std::cout << helpText();
        return 0;
    }
    if (options.showVersion) {
        std::cout << "kevinc 0.1 (C89 to KR32)\n";
        return 0;
    }

    SourceManager sources;
    Diagnostics diagnostics(sources, std::cerr);
    Preprocessor preprocessor(sources, diagnostics);
    for (const fs::path& directory : options.includeDirectories) {
        preprocessor.addIncludeDirectory(directory);
    }
    for (const std::string& define : options.defines) {
        preprocessor.defineFromCommandLine(define);
    }
    for (const std::string& undefine : options.undefines) {
        preprocessor.undefineFromCommandLine(undefine);
    }

    std::vector<Token> tokens = preprocessor.run(options.input);
    if (diagnostics.hadError()) {
        return 1;
    }
    if (options.dumpTokens || options.preprocessOnly) {
        printPreprocessedTokens(tokens, std::cout);
        return 0;
    }

    TypeContext types;
    Parser parser(std::move(tokens), types, diagnostics);
    std::unique_ptr<TranslationUnit> unit = parser.parse();
    if (diagnostics.hadError()) {
        return 1;
    }

    SemanticAnalyzer analyzer(*unit, types, diagnostics);
    SemanticProgram program = analyzer.analyze();
    if (diagnostics.hadError()) {
        return 1;
    }
    if (options.dumpAst) {
        AstDumper dumper(types, std::cout);
        dumper.dump(*unit);
        return diagnostics.hadError() ? 1 : 0;
    }

    CodeGenerator generator(program, types, diagnostics);
    const std::string assembly = generator.generate();
    if (diagnostics.hadError()) {
        return 1;
    }
    if (options.output == "-") {
        std::cout << assembly;
        return 0;
    }
    std::string writeError;
    if (!writeOutputFile(options.output, assembly, writeError)) {
        std::cerr << "kevinc: " << writeError << '\n';
        return 1;
    }
    return 0;
}

} // namespace kevinc

int main(int argc, char** argv) {
    try {
        return kevinc::runCompiler(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "kevinc: fatal: " << error.what() << '\n';
        return 1;
    }
}
