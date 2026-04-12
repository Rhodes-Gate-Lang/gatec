/**
 * File: CompilationSession.hpp
 * Purpose: Memoized compilation of each component name to a full GateObject.
 */
#pragma once

#include "compiler/ComponentRegistry.hpp"
#include "compiler/GateoAliases.hpp"

#include <string>
#include <unordered_map>

namespace gate {

class CompilationSession {
public:
  explicit CompilationSession(const ComponentRegistry &registry);

  /// // TODO: Return the fully compiled GateObject for `name` (same artifact as a .gateo file).
  /// // TODO: On first request, run ComponentCompiler and cache; on later requests return cache.
  /// // TODO: Circular dependency detection when mixing cached .gateo inputs — deferred (GitHub issue).
  [[nodiscard]] const GateObject &compile_or_get(const std::string &name);

  [[nodiscard]] const ComponentRegistry &registry() const { return registry_; }

private:
  const ComponentRegistry &registry_;
  std::unordered_map<std::string, GateObject> cache_;
};

} // namespace gate
