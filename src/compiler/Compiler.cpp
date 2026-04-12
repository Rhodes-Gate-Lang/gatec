/**
 * File: Compiler.cpp
 * Purpose: Public compile API — delegates to CompilationSession.
 */
#include "compiler/Compiler.hpp"
#include "compiler/CompilationSession.hpp"

namespace gate {

GateObject compile_component(const std::string &name,
                                              const ComponentRegistry &registry) {
  CompilationSession session(registry);
  return session.compile_or_get(name);
}

} // namespace gate
