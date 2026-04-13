/**
 * File: Compiler.hpp
 * Purpose: AST -> Circuit compilation.
 *
 * Compiles a named component (and any components it depends on) into a flat
 * gate graph. Each dependency is compiled once and inlined by index remapping.
 *
 * Throws CompileError on any compilation failure.
 */
#pragma once

#include "compiler/ComponentRegistry.hpp"
#include "core/Circuit.hpp"

#include <string>

namespace gate {

Circuit compile_component(const std::string &name,
                          const ComponentRegistry &registry);

} // namespace gate
