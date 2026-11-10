// krld — KRCompiler linker.
//
// Usage: krld [options] <object-file>...
//
// Options:
//   --target=NAME       Target VM (kr32, kr16, kr64)
//   --script=FILE       Linker layout script
//   --map=FILE          Write map file
//   --size              Print size report
//   --gc-sections       Garbage-collect unused sections
//   --strip             Strip symbols/debug from output
//   --debug-file=FILE   Write sidecar debug file
//   --whole-archive     Include all archive members
//   --library-path=DIR  Add library search path
//   --library=NAME      Link library
//   --verbose           Verbose output
//   -o <file>           Output file (default: a.out)
//   -h, --help          Show this help

#include "linker.h"
#include "object.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace krc {

namespace {

struct Options {
    std::string target = "kr32";
    std::string script;
    std::string map_file;
    std::string debug_file;
    std::string output_file = "a.out";
    std::vector<std::string> input_files;
    std::vector<std::string> library_paths;
    std::vector<std::string> libraries;
    bool size = false;
    bool gc_sections = false;
    bool strip = false;
    bool whole_archive = false;
    bool verbose = false;
    std::string entry_symbol;
};

bool parse_options(int argc, char** argv, Options& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printf("Usage: krld [options] <object-file>...\n");
            printf("  --target=NAME       Target VM (kr32, kr16, kr64)\n");
            printf("  --script=FILE       Linker layout script\n");
            printf("  --map=FILE          Write map file\n");
            printf("  --size              Print size report\n");
            printf("  --gc-sections       Garbage-collect unused sections\n");
            printf("  --strip             Strip symbols/debug from output\n");
            printf("  --debug-file=FILE   Write sidecar debug file\n");
            printf("  --whole-archive     Include all archive members\n");
            printf("  --library-path=DIR  Add library search path\n");
            printf("  --library=NAME      Link library\n");
            printf("  --verbose           Verbose output\n");
            printf("  -o <file>           Output file (default: a.out)\n");
            return false;
        } else if (arg == "--size") {
            opts.size = true;
        } else if (arg == "--gc-sections") {
            opts.gc_sections = true;
        } else if (arg == "--strip") {
            opts.strip = true;
        } else if (arg == "--whole-archive") {
            opts.whole_archive = true;
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-o" && i + 1 < argc) {
            opts.output_file = argv[++i];
        } else if (arg.substr(0, 3) == "--o" && arg[2] == '=') {
            opts.output_file = std::string(arg.substr(3));
        } else if (arg.substr(0, 8) == "--target" && arg[8] == '=') {
            opts.target = std::string(arg.substr(9));
        } else if (arg.substr(0, 8) == "--script" && arg[8] == '=') {
            opts.script = std::string(arg.substr(9));
        } else if (arg.substr(0, 6) == "--map=") {
            opts.map_file = std::string(arg.substr(6));
        } else if (arg.substr(0, 12) == "--debug-file" && arg[12] == '=') {
            opts.debug_file = std::string(arg.substr(13));
        } else if (arg.substr(0, 14) == "--library-path" && arg[14] == '=') {
            opts.library_paths.push_back(std::string(arg.substr(15)));
        } else if (arg.substr(0, 10) == "--library=") {
            opts.libraries.push_back(std::string(arg.substr(10)));
        } else if (arg[0] == '-' && arg.size() > 1) {
            fprintf(stderr, "krld: unknown option: %s\n", std::string(arg).c_str());
            return false;
        } else {
            opts.input_files.push_back(std::string(arg));
        }
    }
    if (opts.input_files.empty()) {
        fprintf(stderr, "krld: no input files\n");
        return false;
    }
    return true;
}

} // namespace

} // namespace krc

int main(int argc, char** argv) {
    krc::Options opts;
    if (!krc::parse_options(argc, argv, opts)) {
        return 1;
    }

    krc::LinkerOptions link_opts;
    link_opts.target_id = opts.target;
    link_opts.abi_version = 1;
    link_opts.pointer_width = 32;
    link_opts.endianness = 0;
    link_opts.entry_symbol = opts.entry_symbol;
    link_opts.gc_sections = opts.gc_sections;
    link_opts.strip = opts.strip;
    link_opts.verbose = opts.verbose;
    link_opts.debug_file = opts.debug_file;

    krc::Linker linker(link_opts);

    // Read all input object files.
    for (const auto& path : opts.input_files) {
        std::ifstream is(path, std::ios::binary);
        if (!is) {
            fprintf(stderr, "krld: cannot open input file: %s\n", path.c_str());
            return 1;
        }
        krc::ObjectFile obj;
        if (!krc::read_kro_object(is, obj)) {
            fprintf(stderr, "krld: failed to read object file: %s\n", path.c_str());
            return 1;
        }
        if (opts.verbose) {
            fprintf(stderr, "krld: read object '%s': %zu sections, %zu symbols, %zu relocations\n",
                    path.c_str(), obj.sections.size(), obj.symbols.size(), obj.relocations.size());
        }
        if (!linker.add_object(obj)) {
            fprintf(stderr, "krld: failed to add object file: %s\n", path.c_str());
            return 1;
        }
    }

    // Run the link.
    if (!linker.link()) {
        for (const auto& err : linker.errors()) {
            fprintf(stderr, "krld: %s\n", err.c_str());
        }
        return 1;
    }
    if (opts.verbose) {
        fprintf(stderr, "krld: link succeeded, %zu sections, %zu symbols\n",
                linker.output().sections.size(), linker.symbols().size());
    }

    // Write the output.
    if (!linker.write_kro(opts.output_file)) {
        for (const auto& err : linker.errors()) {
            fprintf(stderr, "krld: %s\n", err.c_str());
        }
        return 1;
    }

    if (opts.size) {
        std::uint64_t total_size = 0;
        for (const auto& sec : linker.output().sections)
            total_size += sec.payload.size();
        printf("total size: %llu bytes\n",
               static_cast<unsigned long long>(total_size));
    }

    if (!opts.map_file.empty()) {
        std::ofstream ms(opts.map_file);
        if (ms) {
            ms << "krld map file\n";
            ms << "target: " << opts.target << "\n";
            ms << "sections:\n";
            for (const auto& sec : linker.output().sections) {
                ms << "  " << sec.name
                   << " addr=0x" << std::hex << sec.address << std::dec
                   << " size=" << sec.payload.size() << "\n";
            }
            ms << "symbols:\n";
            for (const auto& sym : linker.symbols()) {
                ms << "  " << sym.name
                   << " value=0x" << std::hex << sym.value << std::dec
                   << " size=" << sym.size
                   << " section=" << sym.section_index
                   << " defined=" << (sym.defined ? "yes" : "no") << "\n";
            }
        }
    }

    return 0;
}