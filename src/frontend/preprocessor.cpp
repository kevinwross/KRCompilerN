#include "preprocessor.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace krc {

Preprocessor::Preprocessor(SourceManager& source_manager,
                           DiagnosticEngine& diagnostics)
    : source_manager_(source_manager), diagnostics_(diagnostics) {}

bool Preprocessor::push_file(std::string_view path) {
    int file_id = source_manager_.load_file(std::string(path));
    if (file_id >= 0) {
        return start_file(file_id);
    }

    // Not found by path — search include directories.
    std::filesystem::path include_path(path);
    for (const auto& dir : search_dirs()) {
        std::filesystem::path full_path = std::filesystem::path(dir) / include_path;
        file_id = source_manager_.load_file(full_path.string());
        if (file_id >= 0) {
            return start_file(file_id);
        }
    }

    return false;
}

bool Preprocessor::start_file(int file_id) {
    include_stack_.push_back(file_id);
    source_manager_.push_include(file_id);
    lexer_stack_.push_back(std::make_unique<Lexer>(source_manager_, file_id));
    last_line_ = 0;
    // Do not prime here; the caller (parse_translation_unit) will call next().
    return true;
}

void Preprocessor::end_file() {
    if (!include_stack_.empty()) {
        source_manager_.pop_include();
        include_stack_.pop_back();
    }
    if (!lexer_stack_.empty()) {
        lexer_stack_.pop_back();
    }
}

bool Preprocessor::pop_file() {
    if (include_stack_.size() <= 1) {
        return false;
    }
    end_file();
    // Parent file's lexer is already on the stack — no need to recreate.
    last_line_ = 0;
    return true;
}

int Preprocessor::current_file_id() const noexcept {
    if (include_stack_.empty()) {
        return -1;
    }
    return include_stack_.back();
}

// Read the next token, honoring any pushed-back token first.
LexResult Preprocessor::read_token() {
    if (!pushed_back_.empty()) {
        Token tok = pushed_back_.back();
        pushed_back_.pop_back();
        return {tok, true};
    }
    if (lexer_stack_.empty()) {
        return {Token{TokenKind::Eof, {}, "", 0}, true};
    }
    return lexer_stack_.back()->next();
}

void Preprocessor::push_back(const Token& tok) {
    pushed_back_.push_back(tok);
}

LexResult Preprocessor::next() {
    // If we have queued tokens (from macro expansion), return them first.
    if (!queued_tokens_.empty()) {
        Token front = queued_tokens_.front();
        // If the next queued token is a predefined macro (not a user macro),
        // expand it now so nested references like #define X __STDC_VERSION__
        // work correctly.
        if (front.kind == TokenKind::Identifier &&
            !find_macro(front.text) &&
            handle_predefined_macro(front)) {
            // Expansion replaced queued_tokens_; take the first expansion token.
        }
        current_ = queued_tokens_.front();
        queued_tokens_.erase(queued_tokens_.begin());
        if (current_.kind == TokenKind::Eof) {
            last_line_ = 0;
        }
        return {current_, true};
    }

    LexResult result = read_token();
    if (!result.ok) {
        current_ = result.token;
        return {current_, false};
    }

    Token tok = result.token;

    // Track line number for directive detection.
    bool is_line_start = (tok.location.line != last_line_);
    last_line_ = tok.location.line;

    // Conditional compilation: if we're in an inactive branch, skip
    // non-directive tokens.  Directives (#elif, #else, #endif) must still
    // be processed so the conditional stack stays balanced.
    if (!conditional_active() && !(is_line_start && tok.kind == TokenKind::Hash)) {
        // Skip this token and continue.
        return next();
    }

    // Directive handling: if '#' is at the start of a line, handle it.
    if (is_line_start && tok.kind == TokenKind::Hash) {
        current_ = tok;
        if (handle_directive()) {
            return next();
        }
        return {current_, true};
    }

    if (tok.kind == TokenKind::Eof) {
        // End of current file.  Try to pop to the previous file.
        if (include_stack_.size() > 1) {
            pop_file();
            return next();
        }
    }

    // Macro expansion (only when conditional compilation is active).
    if (conditional_active() && tok.kind == TokenKind::Identifier) {
        if (const MacroDefinition* macro = find_macro(tok.text)) {
            if (macro->is_defined && expanding_[std::string(tok.text)] == 0) {
                if (macro->is_function_like) {
                    // Peek at the next token to see if it's '('.
                    LexResult peek_result = read_token();
                    if (!peek_result.ok) {
                        current_ = tok;
                        return {tok, true};
                    }
                    if (peek_result.token.kind == TokenKind::LParen) {
                        expanding_[std::string(tok.text)]++;
                        std::vector<Token> expanded =
                            expand_macro(*macro, tok);
                        expanding_[std::string(tok.text)]--;
                        queued_tokens_ = std::move(expanded);
                        if (!queued_tokens_.empty()) {
                            // If the first expansion token is a predefined
                            // macro, expand it now.
                            if (queued_tokens_.front().kind ==
                                    TokenKind::Identifier &&
                                !find_macro(queued_tokens_.front().text) &&
                                handle_predefined_macro(
                                    queued_tokens_.front())) {
                                // Expansion replaced queued_tokens_; take the
                                // first expansion token.
                            }
                            current_ = queued_tokens_.front();
                            queued_tokens_.erase(queued_tokens_.begin());
                            return {current_, true};
                        }
                    } else {
                        // Not a function call: push back the peeked token.
                        push_back(peek_result.token);
                        current_ = tok;
                        return {tok, true};
                    }
                } else {
                    // Object-like macro.
                    expanding_[std::string(tok.text)]++;
                    std::vector<Token> expanded =
                        expand_macro(*macro, tok);
                    expanding_[std::string(tok.text)]--;
                    queued_tokens_ = std::move(expanded);
                    if (!queued_tokens_.empty()) {
                        // If the first expansion token is a predefined macro,
                        // expand it now.
                        if (queued_tokens_.front().kind ==
                                TokenKind::Identifier &&
                            !find_macro(queued_tokens_.front().text) &&
                            handle_predefined_macro(
                                queued_tokens_.front())) {
                            // Expansion replaced queued_tokens_; take the
                            // first expansion token.
                        }
                        current_ = queued_tokens_.front();
                        queued_tokens_.erase(queued_tokens_.begin());
                        return {current_, true};
                    }
                }
            }
        } else if (tok.text == "_Pragma") {
            // _Pragma operator: _Pragma("string") is equivalent to #pragma string
            if (handle_pragma_operator(tok)) {
                return next();
            }
        } else if (handle_predefined_macro(tok)) {
            current_ = queued_tokens_.front();
            queued_tokens_.erase(queued_tokens_.begin());
            return {current_, true};
        }
    }

    current_ = tok;
    return {current_, true};
}

const MacroDefinition* Preprocessor::find_macro(std::string_view name) const {
    auto it = macros_.find(std::string(name));
    if (it == macros_.end()) {
        return nullptr;
    }
    return &it->second;
}

void Preprocessor::define_macro(MacroDefinition macro) {
    macros_[macro.name] = std::move(macro);
}

void Preprocessor::undefine_macro(std::string_view name) {
    auto it = macros_.find(std::string(name));
    if (it != macros_.end()) {
        it->second.is_defined = false;
    }
}

bool Preprocessor::is_macro_defined(std::string_view name) const {
    auto it = macros_.find(std::string(name));
    if (it == macros_.end()) {
        // Predefined macros are always defined.
        if (name == "__FILE__" || name == "__LINE__" || name == "__COUNTER__" ||
            name == "__STDC_VERSION__" || name == "__cplusplus") {
            return true;
        }
        // Target macros are defined only for the active target.
        if (name == "__KR32__" && target_ == "kr32") return true;
        if (name == "__KR16__" && target_ == "kr16") return true;
        if (name == "__KR64__" && target_ == "kr64") return true;
        return false;
    }
    return it->second.is_defined;
}

void Preprocessor::define_from_command_line(std::string_view definition) {
    std::string def(definition);
    auto eq_pos = def.find('=');
    MacroDefinition macro;
    if (eq_pos == std::string::npos) {
        macro.name = def;
        macro.is_function_like = false;
        Token one{TokenKind::IntegerLiteral, {}, "1", 1};
        macro.replacement.push_back(one);
    } else {
        macro.name = def.substr(0, eq_pos);
        std::string value = def.substr(eq_pos + 1);
        bool is_number = !value.empty() &&
            std::isdigit(static_cast<unsigned char>(value[0]));
        Token tok{TokenKind::IntegerLiteral, {}, value, value.size()};
        tok.kind = is_number ? TokenKind::IntegerLiteral : TokenKind::Identifier;
        macro.replacement.push_back(tok);
    }
    define_macro(std::move(macro));
}

void Preprocessor::undefine(std::string_view name) {
    undefine_macro(name);
}

void Preprocessor::add_include_dir(std::string_view dir) {
    include_dirs_.push_back(std::string(dir));
}

void Preprocessor::set_sysroot(std::string_view sysroot) {
    sysroot_ = std::string(sysroot);
}

void Preprocessor::set_target(std::string_view target) {
    target_ = std::string(target);
}

void Preprocessor::set_nostdinc(bool value) {
    nostdinc_ = value;
}

// Build the ordered list of include directories: user dirs first, then
// sysroot dirs (unless -nostdinc).
std::vector<std::string> Preprocessor::search_dirs() const {
    std::vector<std::string> dirs = include_dirs_;
    if (!nostdinc_ && !sysroot_.empty()) {
        dirs.push_back(sysroot_ + "/include");
        dirs.push_back(sysroot_ + "/" + target_ + "/include");
    }
    return dirs;
}

// Check whether a header file can be found via the include search path.
// Does not load the file into the source manager permanently (the path is
// only checked for existence).
bool Preprocessor::header_found(std::string_view header) const {
    // Try the path as-is first (for quoted includes relative to current file).
    int file_id = source_manager_.load_file(std::string(header));
    if (file_id >= 0) {
        return true;
    }
    // Then search include directories.
    std::filesystem::path include_path(header);
    for (const auto& dir : search_dirs()) {
        std::filesystem::path full_path = std::filesystem::path(dir) / include_path;
        file_id = source_manager_.load_file(full_path.string());
        if (file_id >= 0) {
            return true;
        }
    }
    return false;
}

// --- Directive handling ----------------------------------------------

bool Preprocessor::handle_directive() {
    LexResult result = read_token();
    if (!result.ok) {
        return false;
    }
    Token directive_name = result.token;

    // Directive names may be identifiers or C keywords (e.g. "if", "else").
    if (directive_name.kind != TokenKind::Identifier &&
        !is_c11_keyword(directive_name.kind) &&
        !is_cpp_keyword(directive_name.kind)) {
        diagnostics_.error(
            DiagnosticID{2000, "pp-error"},
            "expected directive name after '#'",
            directive_name.location);
        return false;
    }

    std::string name = std::string(directive_name.text);

    if (name == "define") {
        handle_define();
    } else if (name == "undef") {
        handle_undef();
    } else if (name == "include") {
        handle_include();
    } else if (name == "if") {
        handle_if();
    } else if (name == "ifdef") {
        handle_ifdef();
    } else if (name == "ifndef") {
        handle_ifndef();
    } else if (name == "elif") {
        handle_elif();
    } else if (name == "else") {
        handle_else();
    } else if (name == "endif") {
        handle_endif();
    } else if (name == "pragma") {
        handle_pragma();
    } else if (name == "line") {
        handle_line();
    } else if (name == "error") {
        handle_error(true);
    } else if (name == "warning") {
        handle_error(false);
    } else {
        diagnostics_.error(
            DiagnosticID{2001, "pp-error-unknown-directive"},
            "unknown preprocessing directive: " + name,
            directive_name.location);
    }

    return true;
}

void Preprocessor::handle_define() {
    // Read the macro name.
    LexResult result = read_token();
    if (!result.ok || result.token.kind != TokenKind::Identifier) {
        diagnostics_.error(
            DiagnosticID{2002, "pp-error-define"},
            "expected macro name after #define",
            result.token.location);
        return;
    }
    std::string name = std::string(result.token.text);

    // Check if followed by '(' (function-like).
    // Per C11 6.10.3: if there's whitespace between the macro name and '(',
    // it's an object-like macro, not function-like.
    LexResult peek_result = read_token();
    if (!peek_result.ok) {
        return;
    }

    MacroDefinition macro;
    macro.name = name;

    if (peek_result.token.kind == TokenKind::LParen) {
        // Check for whitespace between name and '(' — if present, object-like.
        std::uint64_t name_end = result.token.location.offset + result.token.text.length();
        bool no_whitespace = (name_end == peek_result.token.location.offset);

        if (no_whitespace) {
            macro.is_function_like = true;
            // Parse parameter list until ')'.
            while (true) {
                LexResult p_result = read_token();
                if (!p_result.ok) break;
                if (p_result.token.kind == TokenKind::RParen) break;
                if (p_result.token.kind == TokenKind::Ellipsis) {
                    macro.is_variadic = true;
                    // __VA_ARGS__ is the variadic parameter name.
                    macro.params.push_back("__VA_ARGS__");
                    // Expect ')' next.
                    LexResult rp = read_token();
                    (void)rp;
                    break;
                }
                if (p_result.token.kind == TokenKind::Identifier) {
                    macro.params.push_back(std::string(p_result.token.text));
                }
                LexResult comma = read_token();
                if (!comma.ok || comma.token.kind != TokenKind::Comma) {
                    // If it's ')', we're done; otherwise break.
                    if (comma.token.kind == TokenKind::RParen) break;
                    break;
                }
            }
        } else {
            // Object-like: the '(' starts the replacement list.
            push_back(peek_result.token);
        }
    } else {
        // Object-like: the peeked token starts the replacement list.
        push_back(peek_result.token);
    }

    // Read replacement tokens until end of line or EOF.
    // The lexer skips newlines, so we detect line changes via token location.
    uint32_t define_line = result.token.location.line;
    bool done = false;
    while (!done) {
        LexResult t_result = read_token();
        if (!t_result.ok) break;
        if (t_result.token.kind == TokenKind::Eof) {
            done = true;
            break;
        }
        // Stop at end of line: push the token back and stop.
        if (t_result.token.location.line != define_line) {
            push_back(t_result.token);
            break;
        }
        macro.replacement.push_back(t_result.token);
    }

    define_macro(std::move(macro));
}

void Preprocessor::handle_undef() {
    LexResult result = read_token();
    if (!result.ok || result.token.kind != TokenKind::Identifier) {
        diagnostics_.error(
            DiagnosticID{2003, "pp-error-undef"},
            "expected macro name after #undef",
            result.token.location);
        return;
    }
    undefine_macro(std::string(result.token.text));
}

void Preprocessor::handle_include() {
    LexResult result = read_token();
    if (!result.ok) return;

    std::string path;
    if (result.token.kind == TokenKind::StringLiteral) {
        path = std::string(result.token.text);
        if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
            path = path.substr(1, path.size() - 2);
        }
    } else if (result.token.kind == TokenKind::Less) {
        std::ostringstream oss;
        while (true) {
            LexResult r = read_token();
            if (!r.ok || r.token.kind == TokenKind::Greater) break;
            oss << r.token.text;
        }
        path = oss.str();
    }

    if (path.empty()) {
        diagnostics_.error(
            DiagnosticID{2004, "pp-error-include"},
            "invalid #include path",
            result.token.location);
        return;
    }

    // Resolve the file_id: try direct path first, then include directories.
    int file_id = source_manager_.load_file(path);
    if (file_id < 0) {
        std::filesystem::path include_path(path);
        for (const auto& dir : search_dirs()) {
            std::filesystem::path full_path = std::filesystem::path(dir) / include_path;
            file_id = source_manager_.load_file(full_path.string());
            if (file_id >= 0) {
                break;
            }
        }
    }

    if (file_id < 0) {
        diagnostics_.error(
            DiagnosticID{2005, "pp-error-include-not-found"},
            "cannot open include file: " + path,
            result.token.location);
        return;
    }

    // Honor #pragma once: skip if already included.
    if (pragma_once_files_.count(file_id)) {
        return;
    }

    if (!start_file(file_id)) {
        diagnostics_.error(
            DiagnosticID{2005, "pp-error-include-not-found"},
            "cannot open include file: " + path,
            result.token.location);
    }
}

void Preprocessor::handle_if() {
    ConditionalFrame frame;
    frame.parent_active = conditional_active();
    frame.active = frame.parent_active;

    if (frame.active) {
        // Evaluate the #if condition. Currently supports defined(NAME) and
        // defined NAME. Other expressions default to true (placeholder).
        frame.active = evaluate_defined_expression();
        frame.saw_true = frame.active;
    }

    conditional_stack_.push_back(std::move(frame));
}

void Preprocessor::handle_ifdef() {
    ConditionalFrame frame;
    frame.parent_active = conditional_active();
    frame.active = frame.parent_active;

    LexResult result = read_token();
    if (result.ok && result.token.kind == TokenKind::Identifier) {
        if (frame.active) {
            frame.active = is_macro_defined(std::string(result.token.text));
            frame.saw_true = frame.active;
        }
    }

    conditional_stack_.push_back(std::move(frame));
}

void Preprocessor::handle_ifndef() {
    ConditionalFrame frame;
    frame.parent_active = conditional_active();
    frame.active = frame.parent_active;

    LexResult result = read_token();
    if (result.ok && result.token.kind == TokenKind::Identifier) {
        if (frame.active) {
            frame.active = !is_macro_defined(std::string(result.token.text));
            frame.saw_true = frame.active;
        }
    }

    conditional_stack_.push_back(std::move(frame));
}

void Preprocessor::handle_elif() {
    if (conditional_stack_.empty()) {
        diagnostics_.error(
            DiagnosticID{2006, "pp-error-elif"},
            "#elif without #if",
            current().location);
        return;
    }
    ConditionalFrame& frame = conditional_stack_.back();
    if (frame.saw_true || frame.saw_else) {
        frame.active = false;
    } else if (frame.parent_active) {
        // Evaluate the #elif condition. Currently supports defined(NAME) and
        // defined NAME. Other expressions default to true (placeholder).
        frame.active = evaluate_defined_expression();
        frame.saw_true = frame.active;
    } else {
        frame.active = false;
    }
}

void Preprocessor::handle_else() {
    if (conditional_stack_.empty()) {
        diagnostics_.error(
            DiagnosticID{2007, "pp-error-else"},
            "#else without #if",
            current().location);
        return;
    }
    ConditionalFrame& frame = conditional_stack_.back();
    if (frame.saw_true || frame.saw_else) {
        frame.active = false;
    } else {
        frame.active = frame.parent_active;
        frame.saw_else = true;
    }
}

void Preprocessor::handle_endif() {
    if (conditional_stack_.empty()) {
        diagnostics_.error(
            DiagnosticID{2008, "pp-error-endif"},
            "#endif without #if",
            current().location);
        return;
    }
    conditional_stack_.pop_back();
}

// Evaluate the condition expression after #if or #elif.  Reads tokens
// until end of line, looking for defined(NAME) or defined NAME.  If a
// defined() expression is found, returns the macro's defined status.
// Otherwise returns true (placeholder: accept unknown expressions).
bool Preprocessor::evaluate_defined_expression() {
    // Remember the starting line so we know when the directive line ends.
    uint32_t start_line = current().location.line;
    bool result = true;  // placeholder: accept unknown expressions
    bool saw_defined = false;
    bool saw_has_include = false;

    while (true) {
        LexResult r = read_token();
        if (!r.ok) break;
        Token tok = r.token;
        // Stop at end of line (but push the token back so it can be
        // processed as the start of the next line/directive).
        if (tok.location.line != start_line) {
            push_back(tok);
            break;
        }

        if (tok.kind == TokenKind::Identifier && tok.text == "defined") {
            saw_defined = true;
            continue;
        }

        if (tok.kind == TokenKind::Identifier && tok.text == "__has_include") {
            saw_has_include = true;
            continue;
        }

        if (saw_defined) {
            if (tok.kind == TokenKind::LParen) {
                // defined(NAME)
                LexResult r2 = read_token();
                if (r2.ok && r2.token.kind == TokenKind::Identifier) {
                    result = is_macro_defined(r2.token.text);
                }
                // Consume the closing paren if present.
                LexResult r3 = read_token();
                (void)r3;
            } else if (tok.kind == TokenKind::Identifier) {
                // defined NAME
                result = is_macro_defined(tok.text);
            }
            saw_defined = false;
        }

        if (saw_has_include) {
            if (tok.kind == TokenKind::LParen) {
                // __has_include("...") / __has_include(<...>)
                LexResult r2 = read_token();
                if (!r2.ok) break;
                Token tok2 = r2.token;
                if (tok2.kind == TokenKind::StringLiteral ||
                    tok2.kind == TokenKind::Less) {
                    // __has_include("...") or __has_include(<...>)
                    std::string header;
                    if (tok2.kind == TokenKind::StringLiteral) {
                        header = std::string(tok2.text);
                        if (header.size() >= 2 && header.front() == '"' &&
                            header.back() == '"') {
                            header = header.substr(1, header.size() - 2);
                        }
                    } else {
                        // Angle-bracketed: read until '>'
                        std::ostringstream oss;
                        while (true) {
                            LexResult r3 = read_token();
                            if (!r3.ok || r3.token.kind == TokenKind::Greater)
                                break;
                            oss << r3.token.text;
                        }
                        header = oss.str();
                    }
                    result = header_found(header);
                }
                // Consume the closing paren if present.
                LexResult r3 = read_token();
                (void)r3;
            }
            saw_has_include = false;
        }
    }

    return result;
}

void Preprocessor::handle_pragma() {
    LexResult result = read_token();
    if (!result.ok) return;

    if (result.token.kind == TokenKind::Identifier &&
        std::string(result.token.text) == "once") {
        if (!include_stack_.empty()) {
            pragma_once_files_.insert(include_stack_.back());
        }
    }
}

bool Preprocessor::handle_pragma_operator(const Token& pragma_tok) {
    // Read the opening '('.
    LexResult result = read_token();
    if (!result.ok || result.token.kind != TokenKind::LParen) {
        diagnostics_.error(
            DiagnosticID{2011, "pp-error-pragma"},
            "expected '(' after _Pragma",
            pragma_tok.location);
        return false;
    }

    // Read the string literal argument.
    result = read_token();
    if (!result.ok || result.token.kind != TokenKind::StringLiteral) {
        diagnostics_.error(
            DiagnosticID{2012, "pp-error-pragma"},
            "expected string literal after _Pragma(",
            pragma_tok.location);
        return false;
    }

    // Extract the string content (without surrounding quotes).
    std::string raw = std::string(result.token.text);
    if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"') {
        diagnostics_.error(
            DiagnosticID{2013, "pp-error-pragma"},
            "malformed string literal in _Pragma",
            result.token.location);
        return false;
    }
    std::string content = raw.substr(1, raw.size() - 2);

    // Unescape C escape sequences: \\ -> \, \" -> ".
    std::string unescaped;
    unescaped.reserve(content.size());
    for (std::size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\\' && i + 1 < content.size()) {
            char next = content[i + 1];
            if (next == '\\' || next == '"') {
                unescaped.push_back(next);
                ++i;
            } else {
                // Unknown escape: keep the backslash and the character.
                unescaped.push_back(content[i]);
                unescaped.push_back(next);
                ++i;
            }
        } else {
            unescaped.push_back(content[i]);
        }
    }

    // Read the closing ')'.
    result = read_token();
    if (!result.ok || result.token.kind != TokenKind::RParen) {
        diagnostics_.error(
            DiagnosticID{2014, "pp-error-pragma"},
            "expected ')' after _Pragma string",
            pragma_tok.location);
        return false;
    }

    // Process the unescaped content as a pragma directive.
    if (unescaped == "once") {
        if (!include_stack_.empty()) {
            pragma_once_files_.insert(include_stack_.back());
        }
    }
    // Other pragmas are accepted but not acted upon (documented no-op).

    return true;
}

void Preprocessor::handle_line() {
    // #line number "filename" — adjust line tracking for diagnostics.
    LexResult result = read_token();
    if (!result.ok) return;
    if (result.token.kind != TokenKind::IntegerLiteral) return;
    std::uint32_t new_line = std::stoul(std::string(result.token.text));

    // Consume optional filename.
    result = read_token();
    if (result.ok && result.token.kind == TokenKind::StringLiteral) {
        // Optionally update current file tracking; filename is informational.
    }

    if (!lexer_stack_.empty()) {
        lexer_stack_.back()->set_line(new_line);
    }
    last_line_ = new_line;
}

void Preprocessor::handle_error(bool is_warning) {
    std::string message;
    while (true) {
        LexResult result = read_token();
        if (!result.ok) break;
        if (result.token.kind == TokenKind::Eof) break;
        message += std::string(result.token.text);
        message += " ";
    }
    if (is_warning) {
        diagnostics_.warning(
            DiagnosticID{2009, "pp-warning"},
            "#warning: " + message,
            current().location);
    } else {
        diagnostics_.error(
            DiagnosticID{2010, "pp-error"},
            "#error: " + message,
            current().location);
    }
}

// --- Macro expansion --------------------------------------------------

std::vector<Token> Preprocessor::expand_macro(const MacroDefinition& macro,
                                              const Token& name_token) {
    (void)name_token;
    std::vector<Token> result;

    if (macro.is_function_like) {
        std::vector<std::vector<Token>> args;
        if (!collect_macro_arguments(macro, args)) {
            result.push_back(Token{TokenKind::Identifier, {}, macro.name, macro.name.size()});
            return result;
        }
        result = substitute_macro_parameters(macro, args);
    } else {
        result = macro.replacement;
    }

    return result;
}

bool Preprocessor::handle_predefined_macro(const Token& tok) {
    std::string name = std::string(tok.text);

    if (name == "__FILE__") {
        queued_tokens_.clear();
        const SourceBuffer* buffer =
            source_manager_.get_buffer(current_file_id());
        std::string path = buffer ? buffer->path() : std::string("<unknown>");
        // Emit a string literal token containing the file path.
        std::string literal = "\"" + path + "\"";
        string_storage_.push_back(std::move(literal));
        Token t{TokenKind::StringLiteral, tok.location, string_storage_.back(),
                string_storage_.back().size()};
        queued_tokens_.push_back(std::move(t));
        return true;
    }

    if (name == "__LINE__") {
        queued_tokens_.clear();
        std::string line_str = std::to_string(tok.location.line);
        string_storage_.push_back(std::move(line_str));
        Token t{TokenKind::IntegerLiteral, tok.location, string_storage_.back(),
                string_storage_.back().size()};
        queued_tokens_.push_back(std::move(t));
        return true;
    }

    if (name == "__COUNTER__") {
        queued_tokens_.clear();
        std::string counter_str = std::to_string(counter_++);
        string_storage_.push_back(std::move(counter_str));
        Token t{TokenKind::IntegerLiteral, tok.location, string_storage_.back(),
                string_storage_.back().size()};
        queued_tokens_.push_back(std::move(t));
        return true;
    }

    // Predefined target macros (plan.md §4.2 / target_descriptor.h).
    // Only the macro matching the active target is defined.
    if (name == "__KR32__" && target_ == "kr32") {
        queued_tokens_.clear();
        string_storage_.push_back("1");
        Token t{TokenKind::IntegerLiteral, tok.location, string_storage_.back(),
                string_storage_.back().size()};
        queued_tokens_.push_back(std::move(t));
        return true;
    }
    if (name == "__KR16__" && target_ == "kr16") {
        queued_tokens_.clear();
        string_storage_.push_back("1");
        Token t{TokenKind::IntegerLiteral, tok.location, string_storage_.back(),
                string_storage_.back().size()};
        queued_tokens_.push_back(std::move(t));
        return true;
    }
    if (name == "__KR64__" && target_ == "kr64") {
        queued_tokens_.clear();
        string_storage_.push_back("1");
        Token t{TokenKind::IntegerLiteral, tok.location, string_storage_.back(),
                string_storage_.back().size()};
        queued_tokens_.push_back(std::move(t));
        return true;
    }

    if (name == "__STDC_VERSION__") {
        queued_tokens_.clear();
        string_storage_.push_back("201112L");
        Token t{TokenKind::IntegerLiteral, tok.location, string_storage_.back(),
                string_storage_.back().size()};
        queued_tokens_.push_back(std::move(t));
        return true;
    }

    if (name == "__cplusplus") {
        queued_tokens_.clear();
        string_storage_.push_back("201703L");
        Token t{TokenKind::IntegerLiteral, tok.location, string_storage_.back(),
                string_storage_.back().size()};
        queued_tokens_.push_back(std::move(t));
        return true;
    }

    return false;
}

bool Preprocessor::collect_macro_arguments(
    const MacroDefinition& macro,
    std::vector<std::vector<Token>>& args) {
    const std::size_t num_params = macro.params.size() +
        (macro.is_variadic ? 1 : 0);
    args.resize(num_params);

    // We've already consumed '(' after the macro name.
    int arg_index = 0;
    int paren_depth = 1;

    while (paren_depth > 0) {
        LexResult result = read_token();
        if (!result.ok) {
            return false;
        }
        Token tok = result.token;

        if (tok.kind == TokenKind::LParen) {
            paren_depth++;
            if (arg_index < static_cast<int>(args.size())) {
                args[arg_index].push_back(tok);
            }
        } else if (tok.kind == TokenKind::RParen) {
            paren_depth--;
            if (paren_depth > 0) {
                if (arg_index < static_cast<int>(args.size())) {
                    args[arg_index].push_back(tok);
                }
            }
        } else if (tok.kind == TokenKind::Comma && paren_depth == 1) {
            arg_index++;
            if (arg_index >= static_cast<int>(args.size())) {
                arg_index = static_cast<int>(args.size()) - 1;
            }
        } else {
            if (arg_index < static_cast<int>(args.size())) {
                args[arg_index].push_back(tok);
            }
        }
    }

    return true;
}

std::vector<Token> Preprocessor::substitute_macro_parameters(
    const MacroDefinition& macro,
    const std::vector<std::vector<Token>>& args) {
    std::vector<Token> substituted;

    // First pass: substitute parameters and handle stringification (#).
    for (std::size_t i = 0; i < macro.replacement.size(); ++i) {
        const Token& tok = macro.replacement[i];

        // Stringification: # followed by a parameter name.
        if (tok.kind == TokenKind::Hash && i + 1 < macro.replacement.size()) {
            const Token& next = macro.replacement[i + 1];
            if (next.kind == TokenKind::Identifier) {
                bool found_param = false;
                for (std::size_t p = 0; p < macro.params.size(); ++p) {
                    if (next.text == macro.params[p] && p < args.size()) {
                        // Build string literal from argument tokens.
                        std::string str;
                        for (const Token& arg_tok : args[p]) {
                            str += std::string(arg_tok.text);
                        }
                        string_storage_.push_back("\"" + str + "\"");
                        Token str_tok{
                            TokenKind::StringLiteral, tok.location,
                            string_storage_.back(), string_storage_.back().size()};
                        substituted.push_back(str_tok);
                        ++i;  // skip the parameter token
                        found_param = true;
                        break;
                    }
                }
                if (found_param) continue;
            }
        }

        // Regular parameter substitution.
        if (tok.kind == TokenKind::Identifier) {
            bool found_param = false;
            for (std::size_t p = 0; p < macro.params.size(); ++p) {
                if (tok.text == macro.params[p] && p < args.size()) {
                    for (const Token& arg_tok : args[p]) {
                        substituted.push_back(arg_tok);
                    }
                    found_param = true;
                    break;
                }
            }
            if (found_param) continue;
        }

        substituted.push_back(tok);
    }

    // Second pass: token pasting (##).
    std::vector<Token> result;
    for (std::size_t i = 0; i < substituted.size(); ++i) {
        const Token& tok = substituted[i];
        if (tok.kind == TokenKind::HashHash && !result.empty() &&
            i + 1 < substituted.size()) {
            // Paste previous and next tokens.
            Token& prev = result.back();
            const Token& next_tok = substituted[i + 1];
            std::string combined =
                std::string(prev.text) + std::string(next_tok.text);
            string_storage_.push_back(std::move(combined));
            TokenKind combined_kind =
                (prev.kind == TokenKind::Identifier ||
                 next_tok.kind == TokenKind::Identifier)
                    ? TokenKind::Identifier
                    : prev.kind;
            prev = Token{combined_kind, prev.location, string_storage_.back(),
                         string_storage_.back().size()};
            ++i;  // skip the next token (consumed by ##)
        } else {
            result.push_back(tok);
        }
    }

    return result;
}

} // namespace krc
