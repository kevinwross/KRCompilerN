// C++ name mangling (Itanium-like, plan.md §10.3).
//
// Encoding rules implemented here:
//   <mangled-name>  ::= "_Z" <encoding>
//   <function-name> ::= <qualified-name>
//   <data-name>     ::= <qualified-name>
//   <qualified-name>::= <source-name>          (unqualified)
//                      | "N" <qualified-name-component>* "E"  (qualified)
//   <qualified-name-component> ::= <source-name>
//   <source-name>   ::= <length> <identifier>
//
//   <type>           ::= <builtin>
//                      | <qualified-name>
//                      | "P" <type>          // pointer
//                      | "R" <type>          // lvalue reference
//                      | "A" <dimension> "_" <type>   // array
//                      | "z"                 // ellipsis
//
// Top-level encoding: _Z + <qualified-name> + <params directly>.
// No "F"/"E" wrapper, no return type at top level (C++ forbids
// overloading by return type). Empty params are encoded as "v".
//
// Builtin single-letter codes:
//   v void, w wchar_t, b bool, c char, a signed char, h unsigned char,
//   s short, t unsigned short, i int, j unsigned int, l long, m unsigned
//   long, x long long, y unsigned long long, f float, d double,
//   n long double, z ellipsis

#include "mangle.h"

#include <cstdint>
#include <string>

namespace krc {

namespace {

std::string decimal(std::uint64_t value) {
    if (value == 0) {
        return "0";
    }
    std::string out;
    while (value > 0) {
        out.insert(out.begin(), static_cast<char>('0' + (value % 10)));
        value /= 10;
    }
    return out;
}

} // namespace

std::string Mangler::encode_builtin_type(TypeKind kind) {
    switch (kind) {
        case TypeKind::Void: return "v";
        case TypeKind::Bool: return "b";
        case TypeKind::Char: return "c";
        case TypeKind::Short: return "s";
        case TypeKind::Int: return "i";
        case TypeKind::Long: return "l";
        case TypeKind::LongLong: return "x";
        case TypeKind::Float: return "f";
        case TypeKind::Double: return "d";
        case TypeKind::LongDouble: return "n";
        default: return "v"; // fallback, should not happen for builtins
    }
}

std::string Mangler::encode_source_name(const std::string& name) {
    return decimal(name.size()) + name;
}

std::string Mangler::encode_qualified_name(
    const std::vector<std::string>& scope,
    const std::string& name) {
    std::string out;
    if (scope.empty()) {
        // Unqualified name: just the source name, no N...E wrapper.
        out += encode_source_name(name);
    } else {
        out += "N";
        for (const auto& component : scope) {
            out += encode_source_name(component);
        }
        out += encode_source_name(name);
        out += "E";
    }
    return out;
}

std::string Mangler::encode_type(const std::shared_ptr<Type>& type) {
    switch (type->kind) {
        case TypeKind::Void:
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::Short:
        case TypeKind::Int:
        case TypeKind::Long:
        case TypeKind::LongLong:
        case TypeKind::Float:
        case TypeKind::Double:
        case TypeKind::LongDouble:
            return encode_builtin_type(type->kind);

        case TypeKind::Pointer: {
            if (type->pointee) {
                return "P" + encode_type(type->pointee);
            }
            return "Pv"; // void* fallback
        }
        case TypeKind::Reference: {
            if (type->pointee) {
                return "R" + encode_type(type->pointee);
            }
            return "Rv";
        }
        case TypeKind::Array: {
            std::string dim = decimal(type->array_length);
            if (type->element_type) {
                return "A" + dim + "_" + encode_type(type->element_type);
            }
            return "A" + dim + "_v";
        }
        case TypeKind::Function: {
            std::string out = "F";
            for (const auto& p : type->param_types) {
                out += encode_type(p);
            }
            out += "E";
            if (type->return_type) {
                out += encode_type(type->return_type);
            } else {
                out += "v";
            }
            return out;
        }
        case TypeKind::Struct:
        case TypeKind::Union:
        case TypeKind::Enum:
        case TypeKind::Typedef: {
            // User-defined type: encode as a qualified name. The type's name
            // is stored in `name`; scope is not tracked on the Type node yet,
            // so encode it as a top-level name.
            return encode_qualified_name({}, std::string(type->name));
        }
        default:
            // Incomplete or unknown: encode as void.
            return "v";
    }
}

std::string Mangler::mangle_type(const std::shared_ptr<Type>& type) {
    return encode_type(type);
}

std::string Mangler::mangle_function(
    const std::vector<std::string>& scope,
    const std::string& name,
    const std::vector<std::shared_ptr<Type>>& param_types,
    const std::shared_ptr<Type>& /*return_type*/,
    bool is_constructor,
    bool is_destructor) {
    std::string out = "_Z";

    if (is_constructor || is_destructor) {
        // C1/C2/D1/D2: complete object constructor/destructor.
        // Encoded as N + scope components + C1/D1 + E, then params directly.
        out += "N";
        for (const auto& component : scope) {
            out += encode_source_name(component);
        }
        out += (is_constructor ? "C1" : "D1");
        out += "E";
        if (param_types.empty()) {
            out += "v";
        } else {
            for (const auto& p : param_types) {
                out += encode_type(p);
            }
        }
        return out;
    }

    // Regular function: _Z + <qualified-name> + <params directly>.
    out += encode_qualified_name(scope, name);
    if (param_types.empty()) {
        out += "v";
    } else {
        for (const auto& p : param_types) {
            out += encode_type(p);
        }
    }
    return out;
}

std::string Mangler::mangle_data(
    const std::vector<std::string>& scope,
    const std::string& name) {
    return "_Z" + encode_qualified_name(scope, name);
}

} // namespace krc
