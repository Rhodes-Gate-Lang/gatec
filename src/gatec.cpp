/**
 * File: gatec.cpp
 * Purpose: CLI — parse a `.gate` file, compile components through `CompCache`,
 *          emit one `.gateo` per cached `GateObject`.
 */
#include "compiler/CompCache.hpp"
#include "compiler/CompileError.hpp"
#include "ProgramLoader.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

void print_usage(const char *argv0) {
  std::cerr << "usage: " << argv0 << " <file.gate> [gateo-out-dir]\n"
            << "  gateo-out-dir defaults to the build directory gateo-cache "
               "(see GATEC_DEFAULT_GATEO_DIR at compile time).\n";
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const std::filesystem::path entry_path = argv[1];
  std::string err;
  auto program = gate::load_program(entry_path, &err);
  if (!program) {
    std::cerr << "parse error";
    if (!err.empty())
      std::cerr << ": " << err;
    std::cerr << '\n';
    return 1;
  }

  if (!gate::resolve_imports(*program, &err)) {
    std::cerr << "import resolution error";
    if (!err.empty())
      std::cerr << ": " << err;
    std::cerr << '\n';
    return 1;
  }

  std::filesystem::path gateo_dir =
      (argc >= 3) ? std::filesystem::path(argv[2])
                  : std::filesystem::path(GATEC_DEFAULT_GATEO_DIR);

  try {
    gate::CompCache cache(program->components);
    for (const gate::ast::Comp &c : program->components)
      cache.resolve(c.name);

    cache.write_cached_gateo_files(gateo_dir);
    std::cout << "wrote " << cache.cache_size() << " .gateo file(s) to " << gateo_dir.string() << '\n';
  }
  catch (const gate::CompileError &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  catch (const std::exception &e) {
    std::cerr << "compile / emit error: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
