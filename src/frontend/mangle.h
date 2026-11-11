#ifndef KRC_MANGLE_H
#define KRC_MANGLE_H

#include "ast.h"

#include <string>
#include <vector>

namespace krc {

// Itanium-like C++ name mangling (plan.md §10.3).
//
// Produces `_Z`-prefixed mangled names for C++ symbols. C symbols and
// `extern "C"` names are left unmangled (handled by the caller: Sema only
// invokes the mangler for C++ linkage symbols).
//
// Supported (M5 subset):
//   - nested/qualified names (namespaces, classes)
//   - builtin types (void, bool, char, short, int, long, long long,
//     float, double, long double, wchar_t)
//   - pointers, references, arrays, function types
//   - constructors (C) and destructors (D)
//   - ellipsis (z)
//   - cv-qualifiers are not yet encoded (post-M5: const/volatile methods)
//
// Not yet: template arguments, substitution/compression, operator names,
// RTTI/vtable/thunk names, exception specs. These land in M6/M11.

class Mangler {
public:
    // Mangle a function name with its parameter types and return type.
    // `name` is the final (unqualified) identifier; `scope` is the list of
    // enclosing scope names from outermost to innermost (e.g. {"A","B"} for
    // A::B::f). Returns the mangled string including the leading "_Z".
    static std::string mangle_function(
        const std::vector<std::string>& scope,
        const std::string& name,
        const std::vector<std::shared_ptr<Type>>& param_types,
        const std::shared_ptr<Type>& return_type,
        bool is_constructor = false,
        bool is_destructor = false);

    // Mangle a data/object name.
    static std::string mangle_data(
        const std::vector<std::string>& scope,
        const std::string& name);

    // Mangle a type into its encoding (no leading "_Z").
    static std::string mangle_type(const std::shared_ptr<Type>& type);

private:
    static std::string encode_builtin_type(TypeKind kind);
    static std::string encode_source_name(const std::string& name);
    static std::string encode_qualified_name(
        const std::vector<std::string>& scope,
        const std::string& name);
    static std::string encode_type(const std::shared_ptr<Type>& type);
};

} // namespace krc

#endif // KRC_MANGLE_H
