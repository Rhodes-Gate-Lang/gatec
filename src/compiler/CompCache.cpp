#include "compiler/CompCache.hpp"
#include "compiler/CompileComponent.hpp"
#include "compiler/CompileError.hpp"

#include "gateo/io.hpp"

namespace gate {

CompCache::CompCache(const std::vector<ast::Comp> &program_comps) {
  for (const ast::Comp &c : program_comps)
    ast_comps_[c.name] = &c;

  // TODO: (future) Look in the build-cache dir (with matching hash)
  // TODO: (future) Support an stdlib path for lookups as well
}

const GateObject &CompCache::resolve(const std::string &name) {
  // Check cache for already compiled
  if (auto it = cache_.find(name); it != cache_.end())
    return it->second;

  // TODO: Check file_cached once implemented

  // Find it in the AST and compile it
  auto ast_it = ast_comps_.find(name);
  if (ast_it == ast_comps_.end())
    throw UndefinedComponentError(name);

  if (in_progress_.count(name))
    throw CyclicDependencyError(name);

  in_progress_.insert(name);
  cache_.emplace(name, compile_component(*ast_it->second, *this));
  in_progress_.erase(name);

  return cache_.at(name);
}

void CompCache::write_cached_gateo_files(const std::filesystem::path &out_dir) const {
  std::filesystem::create_directories(out_dir);
  for (const auto &entry : cache_) {
    const std::filesystem::path path = out_dir / (entry.first + ".gateo");
    gateo::write_file(path, entry.second);
  }
}

std::size_t CompCache::cache_size() const { return cache_.size(); }

} // namespace gate
