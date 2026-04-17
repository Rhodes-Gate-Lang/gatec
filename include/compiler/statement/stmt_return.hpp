#pragma once

#include "compiler/NodeEmitter.hpp"
#include "compiler/SymbolTable.hpp"
#include "parser/Ast.hpp"

#include <cstdint>

namespace gate {

void stmt_return(const ast::ReturnStmt &stmt,
                 const SymbolTable &symbols,
                 NodeEmitter &emitter,
                 std::uint32_t parent_component);

} // namespace gate
