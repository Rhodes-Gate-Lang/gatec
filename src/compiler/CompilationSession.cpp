/**
 * File: CompilationSession.cpp
 * Purpose: CompilationSession implementation.
 */
#include "compiler/CompilationSession.hpp"
#include "compiler/ComponentCompiler.hpp"
#include "core/Ast.hpp"

namespace gate {

CompilationSession::CompilationSession(const ComponentRegistry &registry)
    : registry_(registry) {}

const GateObject &CompilationSession::compile_or_get(
    const std::string &name) {
  // TODO: Detect compile-in-progress cycles for the active session (registry-only graph),
  //       similar to the old in_progress_ set, unless/until replaced by file-cache graph checks.

  if (const auto it = cache_.find(name); it != cache_.end())
    return it->second;

  const ast::Comp &comp = registry_.lookup(name);

  GateObject object;
  // TODO: Stamp object.version from a single project-wide constant / gateo-schema pin.
  ComponentCompiler compiler(object, *this);
  compiler.compile_top_level(comp);

  const auto ins = cache_.emplace(name, std::move(object));
  return ins.first->second;
}

} // namespace gate
