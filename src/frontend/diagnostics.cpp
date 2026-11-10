#include "diagnostics.h"

#include <cstdio>
#include <iostream>

namespace krc {

namespace {

const char* severity_prefix(DiagnosticSeverity severity) {
    switch (severity) {
        case DiagnosticSeverity::Remark:
            return "remark";
        case DiagnosticSeverity::Warning:
            return "warning";
        case DiagnosticSeverity::Error:
            return "error";
        case DiagnosticSeverity::Fatal:
            return "fatal error";
    }
    return "unknown";
}

void print_location(std::ostream& out, const SourceLocation& loc) {
    if (loc.file_id < 0) {
        out << "<unknown>";
        return;
    }
    out << loc.file_id << ':' << loc.line << ':' << loc.column;
}

} // namespace

DiagnosticEngine::DiagnosticEngine(std::uint32_t error_limit)
    : error_limit_(error_limit) {}

bool DiagnosticEngine::report(const Diagnostic& diag) {
    if (hit_error_limit_) {
        return false;
    }

    if (diag.severity == DiagnosticSeverity::Error) {
        if (error_limit_ != 0 && error_count_ >= error_limit_) {
            hit_error_limit_ = true;
            return false;
        }
        ++error_count_;
    } else if (diag.severity == DiagnosticSeverity::Warning) {
        ++warning_count_;
    }

    diagnostics_.push_back(diag);
    emit(diag);
    return true;
}

bool DiagnosticEngine::error(const DiagnosticID& id, std::string message,
                             SourceLocation location) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.id = id;
    diag.message = std::move(message);
    diag.location = location;
    return report(diag);
}

bool DiagnosticEngine::warning(const DiagnosticID& id, std::string message,
                             SourceLocation location) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Warning;
    diag.id = id;
    diag.message = std::move(message);
    diag.location = location;
    return report(diag);
}

bool DiagnosticEngine::note(const DiagnosticID& id, std::string message,
                            SourceLocation location) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Remark;
    diag.id = id;
    diag.message = std::move(message);
    diag.location = location;
    return report(diag);
}

void DiagnosticEngine::set_error_limit(std::uint32_t limit) {
    error_limit_ = limit;
}

void DiagnosticEngine::emit(const Diagnostic& diag) {
    std::ostream& out = std::cerr;
    out << severity_prefix(diag.severity) << ": ";
    print_location(out, diag.location);
    out << ": " << diag.message << '\n';

    for (const auto& note : diag.notes) {
        out << "  note: ";
        print_location(out, note.location);
        out << ": " << note.message << '\n';
    }

    for (const auto& fix : diag.fix_its) {
        out << "  fix-it: ";
        print_location(out, fix.location);
        out << ": replace [" << fix.offset << ", " << fix.offset + fix.length
            << ") with \"" << fix.replacement << "\"\n";
    }
}

} // namespace krc
