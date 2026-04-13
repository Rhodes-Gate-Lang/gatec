#pragma once

#include "compiler/NodeEmitter.hpp"
#include "compiler/SymbolTable.hpp"
#include "parser/Ast.hpp"

#include <cstdint>

namespace gate {

class CompCache;

void stmt_comp_call(const ast::CompCall &stmt,
                    SymbolTable &symbols,
                    NodeEmitter &emitter,
                    std::uint32_t parent_component,
                    CompCache &cache);

} // namespace gate
