#pragma once

#include "compiler/GateoAliases.hpp"
#include "parser/Ast.hpp"

namespace gate {

class CompCache;

/// Compiles an ast::Comp into a self-contained GateObject.
/// Creates a local NodeEmitter and SymbolTable, processes the body,
/// and returns the finished object. Subcomponent calls go through cache.
GateObject compile_component(const ast::Comp &comp, CompCache &cache);

} // namespace gate
