#ifndef KRC_DIAGNOSTICS_H
#define KRC_DIAGNOSTICS_H

#include "source_manager.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace krc {

// Severity of a diagnostic.
enum class DiagnosticSeverity : std::uint8_t {
    Remark = 0,
    Warning = 1,
    Error = 2,
    Fatal = 3,
};

// A stable diagnostic identifier. Each diagnostic has a numeric ID and a
// human-readable name used in documentation and suppressions.
struct DiagnosticID {
    std::uint32_t code = 0;
    std::string_view name;
};

// A single note attached to a diagnostic.
struct DiagnosticNote {
    std::string message;
    SourceLocation location; // may be invalid (file_id == -1)
};

// A single fix-it suggestion: replace the text at [offset, offset+length)
// with `replacement`.
struct DiagnosticFixIt {
    SourceLocation location;
    std::uint64_t offset = 0;
    std::uint64_t length = 0;
    std::string replacement;
};

// One diagnostic produced by the compiler.
struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    DiagnosticID id;
    std::string message;
    SourceLocation location; // may be invalid
    std::vector<DiagnosticNote> notes;
    std::vector<DiagnosticFixIt> fix_its;
};

// DiagnosticEngine collects diagnostics and emits them to a sink.
// Callers report diagnostics via report(); the engine decides whether to
// continue based on the error limit.
class DiagnosticEngine {
public:
    explicit DiagnosticEngine(std::uint32_t error_limit = 100);

    // Report a diagnostic. Returns true if the diagnostic was emitted,
    // false if it was suppressed (e.g. past the error limit).
    bool report(const Diagnostic& diag);

    // Convenience: emit an error at a location.
    bool error(const DiagnosticID& id, std::string message,
               SourceLocation location);

    // Convenience: emit a warning at a location.
    bool warning(const DiagnosticID& id, std::string message,
                 SourceLocation location);

    // Convenience: emit a note at a location.
    bool note(const DiagnosticID& id, std::string message,
              SourceLocation location);

    [[nodiscard]] bool has_errors() const noexcept { return error_count_ > 0; }
    [[nodiscard]] std::uint32_t error_count() const noexcept { return error_count_; }
    [[nodiscard]] std::uint32_t warning_count() const noexcept { return warning_count_; }
    [[nodiscard]] bool hit_error_limit() const noexcept { return hit_error_limit_; }

    // The set of all diagnostics emitted so far, in emission order.
    const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }

    // Set a new error limit. 0 means no limit.
    void set_error_limit(std::uint32_t limit);

private:
    void emit(const Diagnostic& diag);

    std::uint32_t error_limit_;
    std::uint32_t error_count_ = 0;
    std::uint32_t warning_count_ = 0;
    bool hit_error_limit_ = false;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace krc

#endif // KRC_DIAGNOSTICS_H
