#include "debug_info.h"

#include <cstring>

namespace krc {

namespace {

inline constexpr std::uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
inline constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;

} // namespace

//------------------------------------------------------------------------------
// DebugInfoCollector
//------------------------------------------------------------------------------

DebugInfoCollector::DebugInfoCollector(const TranslationUnit& tu,
                                       const SourceManager& source_manager)
    : tu_(tu), source_manager_(source_manager) {}

bool DebugInfoCollector::collect() {
    // Collect source files first so file_index mapping is established.
    for (int file_id : source_manager_.file_ids()) {
        const SourceBuffer* buffer = source_manager_.get_buffer(file_id);
        if (!buffer)
            continue;

        std::uint32_t file_index =
            static_cast<std::uint32_t>(info_.sources.size());

        DebugSourceFile src{};
        src.file_id = file_id;
        src.path = buffer->path();
        src.checksum = fnv1a_64(std::string_view(buffer->contents()));

        info_.sources.push_back(std::move(src));
        file_id_to_index_[file_id] = file_index;
    }

    // Collect function definitions and line table entries.
    for (const auto& decl : tu_.declarations) {
        if (!decl || !decl->function_def)
            continue;

        const auto& fn = decl->function_def;
        if (!fn || !fn->declaration || !fn->body)
            continue;

        // Get the function name from the first declarator.
        std::string func_name;
        if (!decl->declarators.empty())
            func_name = decl->declarators[0]->name;

        // Register the source file for this function.
        std::uint32_t file_index = 0;
        if (decl->location.file_id >= 0) {
            file_index = register_source(decl->location.file_id);
        }

        DebugFunction func{};
        func.name = std::move(func_name);
        func.location = decl->location;
        func.is_static = decl->is_static;
        func.is_extern = decl->is_extern;
        info_.functions.push_back(std::move(func));

        // Collect line table entries from the function body.
        if (fn->body) {
            std::vector<std::shared_ptr<Statement>> block_items;
            // block_items is private to Statement; we access via public member
            // (it's public in the struct). Actually it's a public member.
            collect_statements(fn->body->block_items, file_index);
        }
    }

    // Collect line table entries from global-level declarations.
    for (const auto& decl : tu_.declarations) {
        if (!decl || decl->function_def)
            continue;
        if (decl->location.file_id < 0)
            continue;
        register_source(decl->location.file_id);

        DebugLineEntry line{};
        line.location = decl->location;
        info_.lines.push_back(line);
    }

    return true;
}

std::uint32_t DebugInfoCollector::register_source(int file_id) {
    auto it = file_id_to_index_.find(file_id);
    if (it != file_id_to_index_.end())
        return it->second;

    const SourceBuffer* buffer = source_manager_.get_buffer(file_id);
    if (!buffer)
        return 0;

    std::uint32_t file_index =
        static_cast<std::uint32_t>(info_.sources.size());

    DebugSourceFile src{};
    src.file_id = file_id;
    src.path = buffer->path();
    src.checksum = fnv1a_64(std::string_view(buffer->contents()));

    info_.sources.push_back(std::move(src));
    file_id_to_index_[file_id] = file_index;
    return file_index;
}

void DebugInfoCollector::collect_statements(
    const std::vector<std::shared_ptr<Statement>>& stmts,
    std::uint32_t file_index) {
    (void)file_index; // file_index tracked per-entry via location.file_id

    for (const auto& stmt : stmts) {
        if (!stmt)
            continue;

        if (stmt->location.file_id >= 0 && stmt->location.line > 0) {
            DebugLineEntry line{};
            line.location = stmt->location;
            info_.lines.push_back(line);
        }

        // Recurse into compound statement block items.
        if (stmt->kind == StatementKind::Compound) {
            collect_statements(stmt->block_items, file_index);
        }

        // Recurse into if/else branches.
        if (stmt->kind == StatementKind::If) {
            if (stmt->then_branch)
                collect_statements({stmt->then_branch}, file_index);
            if (stmt->else_branch)
                collect_statements({stmt->else_branch}, file_index);
        }

        // Recurse into while/do-while loop bodies.
        if (stmt->kind == StatementKind::While ||
            stmt->kind == StatementKind::DoWhile) {
            if (stmt->loop_body)
                collect_statements({stmt->loop_body}, file_index);
        }
        if (stmt->kind == StatementKind::For) {
            if (stmt->loop_body)
                collect_statements({stmt->loop_body}, file_index);
        }

        // Collect expressions from the statement.
        if (stmt->expression)
            collect_expression(*stmt->expression);
        if (stmt->condition)
            collect_expression(*stmt->condition);
        if (stmt->for_condition)
            collect_expression(*stmt->for_condition);
        if (stmt->for_increment)
            collect_expression(*stmt->for_increment);
        if (stmt->return_value)
            collect_expression(*stmt->return_value);
        if (stmt->case_value)
            collect_expression(*stmt->case_value);
    }
}

void DebugInfoCollector::collect_expression(const Expression& expr) {
    if (expr.location.file_id >= 0 && expr.location.line > 0) {
        DebugLineEntry line{};
        line.location = expr.location;
        info_.lines.push_back(line);
    }

    if (expr.unary_operand)
        collect_expression(*expr.unary_operand);
    if (expr.binary_left)
        collect_expression(*expr.binary_left);
    if (expr.binary_right)
        collect_expression(*expr.binary_right);
    if (expr.assign_left)
        collect_expression(*expr.assign_left);
    if (expr.assign_right)
        collect_expression(*expr.assign_right);
    if (expr.cond_condition)
        collect_expression(*expr.cond_condition);
    if (expr.cond_true)
        collect_expression(*expr.cond_true);
    if (expr.cond_false)
        collect_expression(*expr.cond_false);
    if (expr.callee)
        collect_expression(*expr.callee);
    for (const auto& arg : expr.call_args)
        if (arg)
            collect_expression(*arg);
    if (expr.cast_operand)
        collect_expression(*expr.cast_operand);
    if (expr.sizeof_operand)
        collect_expression(*expr.sizeof_operand);
    if (expr.member_base)
        collect_expression(*expr.member_base);
    if (expr.array_base)
        collect_expression(*expr.array_base);
    if (expr.array_index)
        collect_expression(*expr.array_index);
}

std::uint64_t DebugInfoCollector::fnv1a_64(std::string_view data) {
    std::uint64_t hash = kFnvOffsetBasis;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= kFnvPrime;
    }
    return hash;
}

} // namespace krc
