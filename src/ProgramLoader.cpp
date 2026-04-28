/**
 * File: ProgramLoader.cpp
 * Purpose: Abstract the parser call and import resolution
 */

#include "ProgramLoader.hpp"
#include "parser/Parser.hpp"

#include <deque>
#include <filesystem>
#include <string>
#include <unordered_set>

namespace gate {

std::optional<ast::Program> load_program(const std::filesystem::path &entry,
                                         std::string *error_out) {
  std::string err;
  auto program = parse_program(entry, &err);
  if (!program) {
    if (error_out) {
      *error_out = err.empty() ? "failed to parse: " + entry.string() : err;
    }
    return std::nullopt;
  }
  return program;
}


bool resolve_imports(ast::Program &program, std::string *error_out) {
  std::deque<std::filesystem::path> import_queue;
  std::unordered_set<std::string> resolved_imports;
  for (const ast::Import &imp : program.imports)
    import_queue.push_back(imp.path);

  while (!import_queue.empty()) {
    const std::filesystem::path import_path = import_queue.front();
    import_queue.pop_front();

    const std::string import_key =
        import_path.lexically_normal().string();
    if (resolved_imports.count(import_key))
      continue;

    auto imported_program = load_program(import_path, error_out);
    if (!imported_program)
      return false;

    resolved_imports.insert(import_key);
    for (const ast::Comp &comp : imported_program->components)
      program.components.push_back(comp);

    for (const ast::Import &imported : imported_program->imports) {
      const std::string imported_key =
          std::filesystem::path(imported.path).lexically_normal().string();
      if (!resolved_imports.count(imported_key))
        import_queue.push_back(imported.path);
      program.imports.push_back(imported);
    }
  }

  return true;
}

} // namespace gate
