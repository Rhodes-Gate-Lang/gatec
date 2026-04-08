/**
 * File: Compiler.hpp
 * Purpose: AST → Circuit compilation. Flattens component hierarchies into
 *          pure gate graphs.
 */
#pragma once

#include "Ast.hpp"
#include "Circuit.hpp"
#include "Error.hpp"

#include <optional>
#include <string>
#include <unordered_map>

namespace gate {

using ComponentRegistry = std::unordered_map<std::string, ast::Comp>;
using SymbolTable = std::unordered_map<std::string, Signal>;

ComponentRegistry build_registry(const ast::Program &program);

std::optional<Circuit> compile_component(const std::string &name,
                                          const ComponentRegistry &registry,
                                          ErrorReporter &errors);

} // namespace gate
