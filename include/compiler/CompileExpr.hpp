#pragma once

#include "compiler/NodeEmitter.hpp"
#include "compiler/SymbolTable.hpp"
#include "parser/Ast.hpp"

#include <cstdint>

namespace gate {

/// Compiles an expression tree into nodes. Returns the node index of the
/// result. Only needs symbol resolution and node emission — no CompCache.
std::uint32_t compile_expr(const ast::Expr &expr,
                           const SymbolTable &symbols,
                           NodeEmitter &emitter,
                           std::uint32_t parent_component);

} // namespace gate
