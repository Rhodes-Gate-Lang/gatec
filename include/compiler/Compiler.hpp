/**
 * File: Compiler.hpp
 * Purpose: AST → full GateObject compilation (same shape as a .gateo file).
 *
 * Each dependency is compiled once and cached; inlining splices callee GateObjects into the caller.
 *
 * Throws CompileError on compilation failure.
 */
#pragma once

#include "compiler/ComponentRegistry.hpp"
#include "compiler/GateoAliases.hpp"

#include <string>

namespace gate {

[[nodiscard]] GateObject compile_component(
    const std::string &name, const ComponentRegistry &registry);

} // namespace gate
