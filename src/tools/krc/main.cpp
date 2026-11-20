// krc — KRCompiler driver.
//
// Usage: krc [options] <input-file>
//
// Options:
//   -fsyntax-only     Parse and report errors only (no output).
//   -dump-ast         Dump the parsed AST to stdout.
//   -dump-ir          Dump the typed SSA IR to stdout.
//   -dump-machine     Dump machine instructions to stdout.
//   -dump-debug       Dump debug info (KDBG) to stdout.
//   -dump-symbols     Dump symbol table to stdout.
//   -c                Compile to object file (KRO).
//   -g                Emit debug info (KDBG sidecar with -c).
//   -o <file>         Output file (default: input.o for -c).
//   -I <dir>          Add include directory.
//   -isystem <dir>    Add system include directory.
//   --sysroot <dir>   Set the system root for headers/libraries.
//   -nostdinc         Do not search system include directories.
//   -D name[=def]     Define a macro.
//   -U name           Undefine a macro.
//   -target <triple>  Target VM (kr32, kr16, kr64).

#include "ast.h"
#include "assembler.h"
#include "backend/debug_info.h"
#include "frontend/debug_info.h"
#include "diagnostics.h"
#include "frame_layout.h"
#include "instruction_selection.h"
#include "ir.h"
#include "irgen.h"
#include "kdbg.h"
#include "kr16.h"
#include "kr16_assembler.h"
#include "kr32.h"
#include "lexer.h"
#include "machine.h"
#include "object.h"
#include "parser.h"
#include "preprocessor.h"
#include "register_allocation.h"
#include "sema.h"
#include "source_manager.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace krc {

namespace {

struct Options {
    bool fsyntax_only = false;
    bool dump_ast = false;
    bool dump_ir = false;
    bool dump_machine = false;
    bool dump_debug = false;
    bool dump_symbols = false;
    bool emit_object = false;
    bool emit_debug = false;
    std::string input_file;
    std::string output_file;
    std::string debug_file;
    std::string target = "kr32";
    std::string sysroot;
    bool nostdinc = false;
    std::vector<std::string> include_dirs;
    std::vector<std::string> system_include_dirs;
    std::vector<std::string> defines;
    std::vector<std::string> undefs;
};

bool parse_options(int argc, char** argv, Options& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-fsyntax-only") {
            opts.fsyntax_only = true;
        } else if (arg == "-dump-ast") {
            opts.dump_ast = true;
        } else if (arg == "-dump-ir") {
            opts.dump_ir = true;
        } else if (arg == "-dump-machine") {
            opts.dump_machine = true;
        } else if (arg == "-dump-debug") {
            opts.dump_debug = true;
        } else if (arg == "-dump-symbols") {
            opts.dump_symbols = true;
        } else if (arg == "-c") {
            opts.emit_object = true;
        } else if (arg == "-g") {
            opts.emit_debug = true;
        } else if (arg == "-o" && i + 1 < argc) {
            opts.output_file = argv[++i];
        } else if (arg == "-target" && i + 1 < argc) {
            opts.target = argv[++i];
        } else if (arg == "--sysroot" && i + 1 < argc) {
            opts.sysroot = argv[++i];
        } else if (arg == "-nostdinc") {
            opts.nostdinc = true;
        } else if (arg == "-I" && i + 1 < argc) {
            opts.include_dirs.push_back(argv[++i]);
        } else if (arg == "-isystem" && i + 1 < argc) {
            opts.system_include_dirs.push_back(argv[++i]);
        } else if (arg == "-D" && i + 1 < argc) {
            opts.defines.push_back(argv[++i]);
        } else if (arg.substr(0, 2) == "-D") {
            opts.defines.push_back(std::string(arg.substr(2)));
        } else if (arg == "-U" && i + 1 < argc) {
            opts.undefs.push_back(argv[++i]);
        } else if (arg.substr(0, 2) == "-U") {
            opts.undefs.push_back(std::string(arg.substr(2)));
        } else if (arg == "-h" || arg == "--help") {
            printf("Usage: krc [options] <input-file>\n");
            printf("  -fsyntax-only   Parse and report errors only\n");
            printf("  -dump-ast       Dump the parsed AST to stdout\n");
            printf("  -dump-ir        Dump the typed SSA IR to stdout\n");
            printf("  -dump-machine   Dump machine instructions to stdout\n");
            printf("  -dump-debug     Dump debug info (KDBG) to stdout\n");
            printf("  -dump-symbols   Dump symbol table to stdout\n");
            printf("  -c              Compile to object file (KRO)\n");
            printf("  -g              Emit debug info (KDBG sidecar with -c)\n");
            printf("  -o <file>       Output file\n");
            printf("  -I <dir>        Add include directory\n");
            printf("  --sysroot <dir> Set system root\n");
            printf("  -nostdinc       Do not search system include directories\n");
            printf("  -D name[=def]   Define a macro\n");
            printf("  -U name         Undefine a macro\n");
            printf("  -target <t>     Target VM (kr32, kr16, kr64)\n");
            return false;
        } else if (arg[0] == '-' && arg.size() > 1) {
            fprintf(stderr, "krc: unknown option: %s\n", std::string(arg).c_str());
            return false;
        } else {
            opts.input_file = std::string(arg);
        }
    }
    if (opts.input_file.empty()) {
        fprintf(stderr, "krc: no input file\n");
        return false;
    }
    return true;
}

} // namespace

} // namespace krc

// Build a KDBG container from frontend and backend debug info.
krc::KdbgContainer build_kdbg_container(
    const std::string& target_id,
    const krc::DebugInfo& frontend_info,
    const krc::BackendDebugInfo& backend_info) {
    krc::KdbgContainer kdbg;
    kdbg.target_id = target_id;
    kdbg.target_numeric_id = 1; // KR32
    kdbg.pointer_width = 32;
    kdbg.endianness = 0;
    kdbg.build_info.compiler_major = 0;
    kdbg.build_info.compiler_minor = 1;

    // Add source files.
    for (std::uint32_t i = 0; i < frontend_info.sources.size(); ++i) {
        const auto& src = frontend_info.sources[i];
        krc::KdbgSrcRecord rec{};
        rec.file_index = i;
        rec.path_offset = krc::kdbg_add_string(kdbg, src.path);
        rec.build_dir_offset = 0;
        rec.language = 0; // C
        rec.checksum = src.checksum;
        kdbg.sources.push_back(rec);
    }

    // Add functions (from frontend, with backend frame info merged).
    for (const auto& fn : frontend_info.functions) {
        krc::KdbgFuncRecord rec{};
        rec.address = 0;
        rec.size = 0;
        rec.name_offset = krc::kdbg_add_string(kdbg, fn.name);
        rec.mangled_offset = 0;
        rec.return_type_id = 0;
        rec.frame_size = 0;
        rec.param_count = 0;
        rec.flags = (fn.is_static ? 0x01 : 0x00) |
                    (fn.is_extern ? 0x02 : 0x00);
        kdbg.functions.push_back(rec);
    }

    // Add backend function records (overrides/merges with frontend).
    for (const auto& fn : backend_info.functions) {
        krc::KdbgFuncRecord rec{};
        rec.address = fn.start_address;
        rec.size = fn.size;
        rec.name_offset = krc::kdbg_add_string(kdbg, fn.name);
        rec.mangled_offset = 0;
        rec.return_type_id = 0;
        rec.frame_size = 0; // filled by linker
        rec.param_count = 0;
        rec.flags = 0;
        kdbg.functions.push_back(rec);
    }

    // Add line table entries.
    for (const auto& line : frontend_info.lines) {
        krc::KdbgLineRecord rec{};
        rec.address = 0;
        rec.size = 0;
        rec.file_index = 0; // TODO: map file_id to index
        rec.line = line.location.line;
        rec.column = line.location.column;
        kdbg.lines.push_back(rec);
    }

    // Add frame records (from backend).
    for (const auto& fn : backend_info.functions) {
        krc::KdbgFrameRecord rec{};
        rec.address = fn.start_address;
        rec.saved_fp_offset = fn.saved_fp_offset;
        rec.saved_ra_offset = fn.saved_ra_offset;
        rec.cfa_rule = fn.cfa_rule;
        rec.flags = 0;
        kdbg.frames.push_back(rec);
    }

    // Add variable records (from backend).
    for (const auto& var : backend_info.variables) {
        krc::KdbgVarRecord rec{};
        rec.address = 0; // filled by linker
        rec.name_offset = krc::kdbg_add_string(kdbg, var.name);
        rec.type_id = var.type_id;
        rec.scope_id = 0; // TODO: map to scope
        rec.decl_line = 0;
        rec.location_kind = var.location_kind;
        rec.location_reg = var.location_reg;
        rec.flags = var.flags;
        rec.reserved = var.reserved;
        // Store stack offset in address field as frame-relative offset.
        rec.address = static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(var.stack_offset & 0xFFFFFFFF));
        kdbg.variables.push_back(rec);
    }

    return kdbg;
}

int main(int argc, char** argv) {
    krc::Options opts;
    if (!krc::parse_options(argc, argv, opts)) {
        return 1;
    }

    krc::SourceManager source_manager;
    krc::DiagnosticEngine diagnostics;

    // Apply command-line macro definitions.
    krc::Preprocessor pp(source_manager, diagnostics);
    for (const auto& d : opts.defines) {
        pp.define_from_command_line(d);
    }
    for (const auto& u : opts.undefs) {
        pp.undefine(u);
    }
    for (const auto& dir : opts.include_dirs) {
        pp.add_include_dir(dir);
    }
    for (const auto& dir : opts.system_include_dirs) {
        pp.add_include_dir(dir);
    }

    // Configure sysroot and target for include search.
    if (!opts.sysroot.empty()) {
        pp.set_sysroot(opts.sysroot);
    }
    pp.set_target(opts.target);
    pp.set_nostdinc(opts.nostdinc);

    // Push the input file.
    if (!pp.push_file(opts.input_file)) {
        diagnostics.error(
            krc::DiagnosticID{3000, "krc-error"},
            "cannot open input file: " + opts.input_file,
            krc::SourceLocation{});
        return 1;
    }

    // Parse.
    krc::Parser parser(pp, diagnostics);
    auto tu = parser.parse_translation_unit();

    if (!tu) {
        diagnostics.error(
            krc::DiagnosticID{3001, "krc-error"},
            "failed to parse translation unit",
            krc::SourceLocation{});
        return 1;
    }

    if (diagnostics.has_errors()) {
        return 1;
    }

    // Semantic analysis.
    krc::Sema sema(diagnostics);
    if (!sema.analyze(*tu)) {
        return 1;
    }

    if (diagnostics.has_errors()) {
        return 1;
    }

    // Dump AST if requested (works with -fsyntax-only).
    if (opts.dump_ast) {
        krc::dump_ast(*tu, std::cout);
    }

    // With -fsyntax-only, stop after semantic analysis: no IR lowering,
    // no backend, no object emission.
    if (opts.fsyntax_only) {
        return 0;
    }

    // Lower to IR.
    krc::IRModule module(opts.target);
    krc::IRGen irgen(module, diagnostics);
    if (!irgen.lower(*tu)) {
        return 1;
    }

    if (diagnostics.has_errors()) {
        return 1;
    }

    // Backend: instruction selection -> register allocation -> frame layout.
    krc::InstructionSelection isel(module);
    krc::MachineModule mm = isel.run();

    if (opts.target == "kr16") {
        krc::RegisterAllocator ra(krc::kr16_allocatable_gprs());
        for (auto& mfn : mm.functions) {
            ra.run(*mfn);
        }

        krc::FrameLayout frame_layout(krc::kr16_target_descriptor());
        for (auto& mfn : mm.functions) {
            frame_layout.run(*mfn);
        }
    } else {
        krc::RegisterAllocator ra(krc::kr32_allocatable_gprs());
        for (auto& mfn : mm.functions) {
            ra.run(*mfn);
        }

        krc::FrameLayout frame_layout(krc::kr32_target_descriptor());
        for (auto& mfn : mm.functions) {
            frame_layout.run(*mfn);
        }
    }

    // Collect backend debug info (function ranges, frame records, variable
    // locations) from the machine module.
    krc::MachineDebugInfoCollector backend_debug_collector(mm);
    krc::BackendDebugInfo backend_debug_info;
    if (!backend_debug_collector.collect()) {
        diagnostics.error(
            krc::DiagnosticID{3006, "krc-error"},
            "failed to collect backend debug info",
            krc::SourceLocation{});
        return 1;
    }
    backend_debug_info = backend_debug_collector.info();

    // Collect debug info from the front end (AST source locations).
    krc::DebugInfoCollector debug_collector(*tu, source_manager);
    krc::DebugInfo debug_info;
    if (!debug_collector.collect()) {
        diagnostics.error(
            krc::DiagnosticID{3005, "krc-error"},
            "failed to collect debug info",
            krc::SourceLocation{});
        return 1;
    }
    debug_info = debug_collector.info();

    if (opts.dump_ir) {
        krc::dump_module(module, std::cout);
    }

    if (opts.dump_machine) {
        krc::dump_machine_module(mm, std::cout);
    }

    if (opts.dump_debug) {
        krc::KdbgContainer kdbg = build_kdbg_container(opts.target, debug_info,
                                                       backend_debug_info);
        krc::dump_kdbg(std::cout, kdbg);
    }

    // Emit object file if requested.
    if (opts.emit_object) {
        krc::ObjectFile obj;
        if (opts.target == "kr16") {
            krc::Kr16Assembler assembler(krc::kr16_target_descriptor());
            if (!assembler.assemble(mm, obj)) {
                diagnostics.error(
                    krc::DiagnosticID{3002, "krc-error"},
                    "failed to assemble object file",
                    krc::SourceLocation{});
                return 1;
            }
        } else {
            krc::Kr32Assembler assembler(krc::kr32_target_descriptor());
            if (!assembler.assemble(mm, obj)) {
                diagnostics.error(
                    krc::DiagnosticID{3002, "krc-error"},
                    "failed to assemble object file",
                    krc::SourceLocation{});
                return 1;
            }
        }

        std::string out_name = opts.output_file;
        if (out_name.empty()) {
            out_name = opts.input_file;
            auto dot = out_name.rfind('.');
            if (dot != std::string::npos)
                out_name = out_name.substr(0, dot);
            out_name += ".o";
        }

        std::ofstream os(out_name, std::ios::binary);
        if (!os) {
            diagnostics.error(
                krc::DiagnosticID{3003, "krc-error"},
                "cannot open output file: " + out_name,
                krc::SourceLocation{});
            return 1;
        }
        if (!krc::write_kro_object(os, obj)) {
            diagnostics.error(
                krc::DiagnosticID{3004, "krc-error"},
                "failed to write object file: " + out_name,
                krc::SourceLocation{});
            return 1;
        }

        // Dump symbol table if requested.
        if (opts.dump_symbols) {
            std::cout << "Symbol table for " << out_name << ":\n";
            for (const auto& sym : obj.symbols) {
                std::cout << "  " << sym.name
                          << " value=0x" << std::hex << sym.value << std::dec
                          << " size=" << sym.size
                          << " section=" << sym.section_index
                          << " binding=" << static_cast<int>(sym.binding)
                          << " type=" << static_cast<int>(sym.type)
                          << " visibility=" << static_cast<int>(sym.visibility)
                          << "\n";
            }
        }

        // Emit KDBG debug sidecar if requested.
        if (opts.emit_debug) {
            std::string dbg_name = opts.debug_file;
            if (dbg_name.empty()) {
                dbg_name = out_name;
                auto dot = dbg_name.rfind('.');
                if (dot != std::string::npos)
                    dbg_name = dbg_name.substr(0, dot);
                dbg_name += ".kdbg";
            }

            krc::KdbgContainer kdbg = build_kdbg_container(opts.target,
                                                           debug_info,
                                                           backend_debug_info);

            std::ofstream dos(dbg_name, std::ios::binary);
            if (!dos) {
                diagnostics.error(
                    krc::DiagnosticID{3006, "krc-error"},
                    "cannot open debug file: " + dbg_name,
                    krc::SourceLocation{});
                return 1;
            }
            if (!krc::write_kdbg(dos, kdbg)) {
                diagnostics.error(
                    krc::DiagnosticID{3007, "krc-error"},
                    "failed to write debug file: " + dbg_name,
                    krc::SourceLocation{});
                return 1;
            }
        }
    }

    return 0;
}
