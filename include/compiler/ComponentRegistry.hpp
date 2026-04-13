/**
 * File: ComponentRegistry.hpp
 * Purpose: Lookup table from component name to its AST definition.
 *
 * Built once from a parsed Program, then used read-only during compilation.
 * Throws on lookup failures so callers don't need manual checks.
 */
#pragma once

#include "compiler/CompileError.hpp"
#include "core/Ast.hpp"

#include <string>
#include <unordered_map>

namespace gate {

class ComponentRegistry {
public:
  explicit ComponentRegistry(const ast::Program &program) {
    for (const auto &comp : program.components)
      map_.emplace(comp.name, comp);
  }

  /// Find a component by name. Throws UndefinedComponentError if not found.
  const ast::Comp &lookup(const std::string &name) const {
    auto it = map_.find(name);
    if (it == map_.end())
      throw UndefinedComponentError(name);
    return it->second;
  }

  bool contains(const std::string &name) const {
    return map_.count(name) > 0;
  }

  bool empty() const { return map_.empty(); }

  auto begin() const { return map_.begin(); }
  auto end() const { return map_.end(); }

private:
  std::unordered_map<std::string, ast::Comp> map_;
};

} // namespace gate
