#pragma once

#include "compiler/NodeEmitter.hpp"
#include "compiler/SymbolTable.hpp"
#include "parser/Ast.hpp"

#include <cstdint>

namespace gate {

class CompCache;

/// Dispatches on the Statement variant and compiles one statement.
/// CompCache is needed only for the CompCall case.
void compile_statement(const ast::Statement &stmt,
                       SymbolTable &symbols,
                       NodeEmitter &emitter,
                       std::uint32_t parent_component,
                       CompCache &cache);

} // namespace gate
