#ifndef KRC_PREPROCESSOR_H
#define KRC_PREPROCESSOR_H

#include "diagnostics.h"
#include "lexer.h"
#include "source_manager.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace krc {

// A single macro definition.
struct MacroDefinition {
    std::string name;
    bool is_function_like = false;
    std::vector<std::string> params;       // parameter names (function-like only)
    bool is_variadic = false;              // true if ... is the last parameter
    std::vector<Token> replacement;        // replacement token list
    bool is_defined = true;                // false after #undef
};

// The preprocessor: handles # directives, macro expansion, and file includes.
//
// The preprocessor wraps a SourceManager and a DiagnosticEngine.  It lazily
// creates Lexers for each source file on the include stack.  Callers pull
// tokens via next(); the preprocessor transparently handles directives,
// macro expansion, and include file transitions.
//
// Macro expansion is performed eagerly as tokens are produced.  The
// preprocessor tracks the set of macros currently being expanded to prevent
// infinite recursion (a macro cannot expand within its own expansion).
class Preprocessor {
public:
    Preprocessor(SourceManager& source_manager, DiagnosticEngine& diagnostics);

    // Begin preprocessing the given file.  Returns false if the file cannot
    // be loaded.
    bool push_file(std::string_view path);

    // Pop the current file and resume the previous one.  Returns false if
    // there is no file to pop.
    bool pop_file();

    // Lex the next preprocessed token.  Returns ok=false on unrecoverable
    // error (e.g. unterminated conditional).
    LexResult next();

    // Push a token back so the next read_token() returns it.
    void push_back(const Token& tok);

    // Peek at the next token without consuming it.
    const Token& peek() const { return current_; }

    // Return the current (last-returned) token.
    const Token& current() const { return current_; }

    bool at_eof() const { return current_.kind == TokenKind::Eof; }

    // True if the given name is currently defined as a macro.
    bool is_macro_defined(std::string_view name) const;

    // Define a macro from the command line (-Dname or -Dname=definition).
    void define_from_command_line(std::string_view definition);

    // Undefine a macro (-Uname).
    void undefine(std::string_view name);

    // Add an include directory searched when a file is not found by path.
    void add_include_dir(std::string_view dir);

    // Set the sysroot directory.  When set, `sysroot/include/` and
    // `sysroot/<target>/include/` are searched after user include directories
    // (unless `-nostdinc` is set).
    void set_sysroot(std::string_view sysroot);

    // Set the target ID used for sysroot include path construction
    // (e.g. "kr32", "kr16", "kr64").
    void set_target(std::string_view target);

    // When true, sysroot include directories are not searched.
    void set_nostdinc(bool value);

    // Build the ordered list of include directories (user + sysroot).
    std::vector<std::string> search_dirs() const;

private:
    // --- File / lexer management ------------------------------------------

    // Start lexing a new file (called by push_file and #include).
    bool start_file(int file_id);

    // Finish the current file and return to the parent (if any).
    void end_file();

    // --- Directive handling ----------------------------------------------

    // Read the next token, honoring any pushed-back token first.
    LexResult read_token();

    // Handle a predefined macro (__FILE__, __LINE__, __COUNTER__, target
    // macros like __KR32__, __STDC_VERSION__, etc.).
    // Returns true if the token was a predefined macro and its expansion
    // was queued.
    bool handle_predefined_macro(const Token& tok);

    // Evaluate the condition expression after #if or #elif (supports
    // defined(NAME) / defined NAME / __has_include(...)).
    bool evaluate_defined_expression();

    // Check whether a header file can be found via the include search path.
    bool header_found(std::string_view header) const;

    // Handle a # directive that has just been recognized.  The '#' token has
    // already been consumed.  Returns true on success.
    bool handle_directive();

    // Parse #define: name token-list or name ( params ) token-list
    void handle_define();

    // Handle #undef name
    void handle_undef();

    // Handle #include "..." or #include <...>
    void handle_include();

    // Handle #if / #ifdef / #ifndef / #elif / #else / #endif
    void handle_if();
    void handle_ifdef();
    void handle_ifndef();
    void handle_elif();
    void handle_else();
    void handle_endif();

    // Handle #pragma (mostly a no-op; #pragma once is tracked).
    void handle_pragma();

    // Handle the _Pragma operator: _Pragma("string") is equivalent to
    // #pragma string.  Reads the parenthesized string literal and processes
    // its contents as a pragma directive.
    bool handle_pragma_operator(const Token& pragma_tok);

    // Handle #line
    void handle_line();

    // Handle #error and #warning
    void handle_error(bool is_warning);

    // --- Conditional compilation state -----------------------------------

    struct ConditionalFrame {
        bool active = true;      // true if this branch's text is being passed through
        bool parent_active = true; // true if an enclosing branch is active
        bool saw_true = false;   // true if a prior #if/#elif in this group succeeded
        bool saw_else = false;   // true if #else was already seen in this group
    };

    std::vector<ConditionalFrame> conditional_stack_;

    bool conditional_active() const {
        for (const auto& frame : conditional_stack_) {
            if (!frame.active) return false;
        }
        return true;
    }

    // --- Macro management -------------------------------------------------

    // Macro definitions, name -> definition.
    std::unordered_map<std::string, MacroDefinition> macros_;

    // Macros currently being expanded (to prevent recursion).
    std::unordered_map<std::string, int> expanding_;

    // Look up a macro by name.  Returns nullptr if not defined.
    const MacroDefinition* find_macro(std::string_view name) const;

    // Define a macro (used by #define and -D).
    void define_macro(MacroDefinition macro);

    // Undefine a macro.
    void undefine_macro(std::string_view name);

    // --- Macro expansion --------------------------------------------------

    // Expand a function-like or object-like macro at the current position.
    // Returns the expanded tokens (caller queues them) or an empty vector on
    // failure.
    std::vector<Token> expand_macro(const MacroDefinition& macro,
                                    const Token& name_token);

    // Collect the argument tokens for a function-like macro invocation.
    // Returns true on success, false on error.
    bool collect_macro_arguments(const MacroDefinition& macro,
                                 std::vector<std::vector<Token>>& args);

    // Substitute macro parameters into the replacement list.
    std::vector<Token> substitute_macro_parameters(
        const MacroDefinition& macro,
        const std::vector<std::vector<Token>>& args);

    // --- Token helpers ----------------------------------------------------

    // --- State ------------------------------------------------------------

    SourceManager& source_manager_;
    DiagnosticEngine& diagnostics_;

    // The lexer stack: back() is the current file's lexer.
    std::vector<std::unique_ptr<Lexer>> lexer_stack_;

    // The include stack of file_ids (mirrors source_manager_.include_stack()).
    std::vector<int> include_stack_;

    // The last-returned token.
    Token current_;

    // Queued tokens from macro expansion (front = next to return).
    std::vector<Token> queued_tokens_;

    // Storage for dynamically generated token text (string literals,
    // integer literals from __LINE__/__COUNTER__, etc.).  Tokens reference
    // into this storage via string_view; deque keeps references stable on
    // push_back.
    std::deque<std::string> string_storage_;

    // One-token pushback for lookahead (function-like macro detection).
    std::vector<Token> pushed_back_;

    // Line number of the last returned token, used to detect line starts
    // for directive recognition.
    std::uint32_t last_line_ = 0;

    // Predefined macro counter for __COUNTER__.
    std::uint32_t counter_ = 0;

    // The current file_id (top of the include stack), or -1 if none.
    int current_file_id() const noexcept;

    // Set of file_ids already included with #pragma once.
    std::unordered_set<int> pragma_once_files_;

    // Include directories searched when resolving #include paths.
    std::vector<std::string> include_dirs_;

    // Sysroot directory (set via --sysroot).
    std::string sysroot_;

    // Target ID used for sysroot include path construction (e.g. "kr32").
    std::string target_ = "kr32";

    // When true, sysroot include directories are not searched.
    bool nostdinc_ = false;
};

} // namespace krc

#endif // KRC_PREPROCESSOR_H
