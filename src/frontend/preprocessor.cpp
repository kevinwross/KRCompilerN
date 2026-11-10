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
    has_pushed_back_ = false;
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
    has_pushed_back_ = false;
    return true;
}

// Read the next token, honoring any pushed-back token first.
LexResult Preprocessor::read_token() {
    if (has_pushed_back_) {
        has_pushed_back_ = false;
        return {pushed_back_, true};
    }
    if (lexer_stack_.empty()) {
        return {Token{TokenKind::Eof, {}, "", 0}, true};
    }
    return lexer_stack_.back()->next();
}

void Preprocessor::push_back(const Token& tok) {
    pushed_back_ = tok;
    has_pushed_back_ = true;
}

LexResult Preprocessor::next() {
    // If we have queued tokens (from macro expansion), return them first.
    if (!queued_tokens_.empty()) {
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
                        current_ = queued_tokens_.front();
                        queued_tokens_.erase(queued_tokens_.begin());
                        return {current_, true};
                    }
                }
            }
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

// --- Directive handling ----------------------------------------------

bool Preprocessor::handle_directive() {
    LexResult result = read_token();
    if (!result.ok) {
        return false;
    }
    Token directive_name = result.token;

    if (directive_name.kind != TokenKind::Identifier) {
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
    LexResult peek_result = read_token();
    if (!peek_result.ok) {
        return;
    }

    MacroDefinition macro;
    macro.name = name;

    if (peek_result.token.kind == TokenKind::LParen) {
        macro.is_function_like = true;
        // Parse parameter list until ')'.
        while (true) {
            LexResult p_result = read_token();
            if (!p_result.ok) break;
            if (p_result.token.kind == TokenKind::RParen) break;
            if (p_result.token.kind == TokenKind::Ellipsis) {
                macro.is_variadic = true;
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
        // Placeholder: accept all #if conditions.
        // A full implementation would evaluate the constant expression.
        frame.active = true;
        frame.saw_true = true;
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
        frame.active = true;
        frame.saw_true = true;
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

void Preprocessor::handle_line() {
    // #line number "filename" — placeholder, skip remaining tokens.
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
    std::vector<Token> result;

    for (const Token& tok : macro.replacement) {
        bool substituted = false;
        if (tok.kind == TokenKind::Identifier) {
            for (std::size_t i = 0; i < macro.params.size(); ++i) {
                if (tok.text == macro.params[i]) {
                    if (i < args.size()) {
                        for (const Token& arg_tok : args[i]) {
                            result.push_back(arg_tok);
                        }
                    }
                    substituted = true;
                    break;
                }
            }
        }
        if (!substituted) {
            result.push_back(tok);
        }
    }

    return result;
}

} // namespace krc
