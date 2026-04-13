/**
 * File: SymbolTable.hpp
 * Purpose: Maps signal names to their compiled Signals within a single scope.
 *
 * Throws on misuse (undefined lookups, duplicate definitions) so callers
 * don't need manual error checking at every access.
 */
#pragma once

#include "compiler/CompileError.hpp"
#include "core/Circuit.hpp"

#include <string>
#include <unordered_map>

namespace gate {

class SymbolTable {
public:
  /// Insert a new symbol. Throws DuplicateSymbolError if it already exists.
  void define(const std::string &name, Signal signal) {
    if (map_.count(name))
      throw DuplicateSymbolError(name);
    map_.emplace(name, signal);
  }

  /// Look up an existing symbol. Throws UndefinedSymbolError if not found.
  Signal resolve(const std::string &name) const {
    auto it = map_.find(name);
    if (it == map_.end())
      throw UndefinedSymbolError(name);
    return it->second;
  }

  /// Overwrite an existing symbol (for mutation). Throws UndefinedSymbolError
  /// if the name hasn't been defined yet.
  void update(const std::string &name, Signal signal) {
    auto it = map_.find(name);
    if (it == map_.end())
      throw UndefinedSymbolError(name);
    it->second = signal;
  }

  bool contains(const std::string &name) const {
    return map_.count(name) > 0;
  }

private:
  std::unordered_map<std::string, Signal> map_;
};

} // namespace gate
