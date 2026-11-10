/// Fixture extractor for KRCompiler (plan.md §5.4).
///
/// Scans host doctest files (../kr32/tests/unit, ../kr32/tests/cxx) for
/// escaped C/C++ source strings passed to compile()/compile_and_run(),
/// extracts each into tests/corpus/extracted/<feature>/<name>.c|.cpp, and
/// writes a manifest recording file:line provenance and feature tags.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct ExtractedFixture {
    std::string source_file;   // original doctest file path
    std::uint32_t line = 0;    // line of the first string literal
    std::string feature;       // feature tag
    std::string name;          // fixture file name
    std::string source;        // unescaped C/C++ source
};

// Returns true if ch is a C++ identifier character.
bool is_ident_char(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) ||
           ch == '_' || std::isdigit(static_cast<unsigned char>(ch));
}

// Returns true if ch is whitespace (space, tab, CR, LF, form feed, vertical tab).
bool is_ws(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ||
           ch == '\f' || ch == '\v';
}

// Unescape a C++ string literal's contents (without surrounding quotes).
// Handles \n, \t, \r, \\, \", \', \?, \0, \xHH, \ooo.
std::string unescape(std::string_view lit) {
    std::string out;
    out.reserve(lit.size());
    std::size_t i = 0;
    while (i < lit.size()) {
        char ch = lit[i];
        if (ch == '\\' && i + 1 < lit.size()) {
            char esc = lit[++i];
            switch (esc) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case '\'': out.push_back('\''); break;
                case '?': out.push_back('?'); break;
                case '0': out.push_back('\0'); break;
                case 'a': out.push_back('\a'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'v': out.push_back('\v'); break;
                case 'x': {
                    std::uint32_t val = 0;
                    int digits = 0;
                    while (i + 1 < lit.size() && digits < 2 &&
                           std::isxdigit(static_cast<unsigned char>(lit[i + 1]))) {
                        char dc = lit[++i];
                        val = val * 16 + (dc >= 'A' && dc <= 'F' ? dc - 'A' + 10 :
                                           dc >= 'a' && dc <= 'f' ? dc - 'a' + 10 :
                                           dc - '0');
                        ++digits;
                    }
                    out.push_back(static_cast<char>(val & 0xFFu));
                    break;
                }
                default: {
                    // Octal escape \ooo (up to 3 digits).
                    std::uint32_t val = 0;
                    int digits = 0;
                    if (esc >= '0' && esc <= '7') {
                        val = esc - '0';
                        ++digits;
                        while (i + 1 < lit.size() && digits < 3 &&
                               lit[i + 1] >= '0' && lit[i + 1] <= '7') {
                            val = val * 8 + (lit[++i] - '0');
                            ++digits;
                        }
                    }
                    out.push_back(static_cast<char>(val & 0xFFu));
                    break;
                }
            }
        } else {
            out.push_back(ch);
        }
        ++i;
    }
    return out;
}

// Guess a feature tag from the source content.
std::string guess_feature(std::string_view source) {
    if (source.find("class ") != std::string_view::npos ||
        source.find("struct ") != std::string_view::npos) {
        return "classes";
    }
    if (source.find("template") != std::string_view::npos) {
        return "templates";
    }
    if (source.find("lambda") != std::string_view::npos ||
        source.find("[]") != std::string_view::npos) {
        return "lambdas";
    }
    if (source.find("constexpr") != std::string_view::npos) {
        return "constexpr";
    }
    if (source.find("virtual") != std::string_view::npos) {
        return "inheritance";
    }
    if (source.find("new ") != std::string_view::npos ||
        source.find("delete") != std::string_view::npos) {
        return "memory";
    }
    if (source.find("auto ") != std::string_view::npos ||
        source.find("decltype") != std::string_view::npos) {
        return "autodecltype";
    }
    if (source.find("range") != std::string_view::npos ||
        source.find("for (") != std::string_view::npos) {
        return "range-for";
    }
    if (source.find("typedef") != std::string_view::npos) {
        return "typedefs";
    }
    if (source.find("struct") != std::string_view::npos) {
        return "structs";
    }
    if (source.find("pointer") != std::string_view::npos ||
        source.find('*') != std::string_view::npos) {
        return "pointers";
    }
    return "c-core";
}

// Sanitize a fixture name for use as a file name.
std::string sanitize_name(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (char ch : name) {
        if (std::isalpha(static_cast<unsigned char>(ch)) ||
            std::isdigit(static_cast<unsigned char>(ch)) || ch == '_' ||
            ch == '-') {
            out.push_back(ch);
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) {
        out = "fixture";
    }
    return out;
}

// Find the line number of a byte offset within a file's contents.
std::uint32_t line_of(std::string_view contents, std::size_t offset) {
    std::uint32_t line = 1;
    for (std::size_t i = 0; i < offset && i < contents.size(); ++i) {
        if (contents[i] == '\n') {
            ++line;
        }
    }
    return line;
}

} // namespace

// Scan one file for compile()/compile_and_run() string-literal argument groups.
std::vector<ExtractedFixture> extract_from_file(const fs::path& file_path) {
    std::vector<ExtractedFixture> fixtures;

    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        std::cerr << "warning: cannot open " << file_path << '\n';
        return fixtures;
    }
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    std::size_t pos = 0;
    while (pos < contents.size()) {
        // Find the next occurrence of compile( or compile_and_run(
        std::size_t call_pos = contents.find("compile(", pos);
        std::size_t call_pos2 = contents.find("compile_and_run(", pos);
        std::size_t chosen = std::string::npos;
        std::size_t call_len = 0;

        if (call_pos != std::string::npos &&
            (call_pos2 == std::string::npos || call_pos < call_pos2)) {
            chosen = call_pos;
            call_len = 8; // strlen("compile(")
        } else if (call_pos2 != std::string::npos) {
            chosen = call_pos2;
            call_len = 16; // strlen("compile_and_run(")
        }

        if (chosen == std::string::npos) {
            break;
        }

        // Verify this is a real call (not inside a comment or string).
        // Simple check: the character before must not be an identifier char.
        if (chosen > 0 && is_ident_char(contents[chosen - 1])) {
            pos = chosen + call_len;
            continue;
        }

        // Move past the opening paren and skip whitespace.
        std::size_t cursor = chosen + call_len;
        while (cursor < contents.size() && is_ws(contents[cursor])) {
            ++cursor;
        }

        // Collect adjacent string literals.
        std::string combined;
        std::uint32_t first_line = line_of(contents, chosen);
        bool found_any = false;

        while (cursor < contents.size()) {
            while (cursor < contents.size() && is_ws(contents[cursor])) {
                ++cursor;
            }
            if (cursor >= contents.size() || contents[cursor] != '"') {
                break;
            }

            // Read a string literal.
            ++cursor; // skip opening quote
            std::size_t lit_start = cursor;
            while (cursor < contents.size()) {
                if (contents[cursor] == '\\' && cursor + 1 < contents.size()) {
                    cursor += 2; // skip escaped char
                    continue;
                }
                if (contents[cursor] == '"') {
                    break;
                }
                ++cursor;
            }
            if (cursor >= contents.size()) {
                break; // unterminated string
            }
            std::size_t lit_end = cursor; // position of closing quote
            ++cursor; // skip closing quote

            std::string_view lit(contents.data() + lit_start,
                                 lit_end - lit_start);
            combined += unescape(lit);
            found_any = true;
        }

        if (found_any && !combined.empty()) {
            ExtractedFixture fx;
            fx.source_file = file_path.string();
            fx.line = first_line;
            fx.feature = guess_feature(combined);
            fx.name = sanitize_name(fx.feature) + "_" +
                      std::to_string(fixtures.size() + 1) + ".cpp";
            fx.source = std::move(combined);
            fixtures.push_back(std::move(fx));
        }

        pos = chosen + call_len;
    }

    return fixtures;
}

int main(int argc, char** argv) {
    fs::path input_dir;
    fs::path output_dir;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--input-dir" && i + 1 < argc) {
            input_dir = fs::path(argv[++i]);
        } else if (arg == "--output-dir" && i + 1 < argc) {
            output_dir = fs::path(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: extract_fixtures --input-dir DIR --output-dir DIR\n";
            return 0;
        } else {
            std::cerr << "error: unknown argument: " << arg << '\n';
            return 2;
        }
    }

    if (input_dir.empty() || output_dir.empty()) {
        std::cerr << "error: --input-dir and --output-dir are required\n";
        return 2;
    }

    if (!fs::is_directory(input_dir)) {
        std::cerr << "error: input directory does not exist: " << input_dir << '\n';
        return 1;
    }

    fs::create_directories(output_dir);

    std::vector<ExtractedFixture> all_fixtures;
    for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".cpp") {
            auto found = extract_from_file(entry.path());
            all_fixtures.insert(all_fixtures.end(),
                                std::make_move_iterator(found.begin()),
                                std::make_move_iterator(found.end()));
        }
    }

    // Write extracted fixtures grouped by feature.
    std::unordered_map<std::string, std::uint32_t> feature_count;
    for (const auto& fx : all_fixtures) {
        auto& count = feature_count[fx.feature];
        fs::path feature_dir = output_dir / fx.feature;
        fs::create_directories(feature_dir);

        // Use a stable name based on feature + count.
        std::string name = sanitize_name(fx.feature) + "_" +
                           std::to_string(++count) + ".cpp";
        fs::path out_path = feature_dir / name;

        std::ofstream out(out_path);
        if (!out) {
            std::cerr << "error: cannot write " << out_path << '\n';
            return 1;
        }
        out << fx.source;
    }

    // Write manifest.
    fs::path manifest_path = output_dir / "manifest.txt";
    std::ofstream manifest(manifest_path);
    if (!manifest) {
        std::cerr << "error: cannot write manifest " << manifest_path << '\n';
        return 1;
    }
    manifest << "source_file\tline\tfeature\tname\n";
    for (const auto& fx : all_fixtures) {
        manifest << fx.source_file << '\t' << fx.line << '\t' << fx.feature
                 << '\t' << fx.name << '\n';
    }

    std::cout << "extracted " << all_fixtures.size()
              << " fixtures from " << input_dir << " to " << output_dir << '\n';
    return 0;
}
