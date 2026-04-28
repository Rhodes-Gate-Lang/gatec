#pragma once

#include "compiler/GateoAliases.hpp"
#include "parser/Ast.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gate {

/// Caches compiled GateObjects by component name. Provides three resolution
/// paths: in-memory cache, filesystem (future), and on-demand AST compilation.
/// Tracks in-progress compilations for cycle detection.
class CompCache {
public:
  explicit CompCache(const std::vector<ast::Comp> &program_comps);

  /// Returns a compiled GateObject for the named component.
  /// Throws CyclicDependencyError if currently being compiled.
  /// Throws UndefinedComponentError if not found anywhere.
  const GateObject &resolve(const std::string &name);

  /// Writes one `.gateo` file per entry in the in-memory cache (`<name>.gateo`).
  void write_cached_gateo_files(const std::filesystem::path &out_dir) const;

  [[nodiscard]] std::size_t cache_size() const;

private:
  std::unordered_map<std::string, const ast::Comp *> ast_comps_;
  std::unordered_map<std::string, GateObject> cache_;
  std::unordered_map<std::string, std::filesystem::path> file_cached_;
  std::unordered_set<std::string> in_progress_;
};

} // namespace gate
