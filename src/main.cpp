/**
 * File: main.cpp
 * Purpose: CLI entry — load a `.gate` file (prototype syntax), parse, print AST to stdout.
 */
#include "Parser.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read_stream(std::istream &in) {
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <file.gate>\n";
    return 1;
  }

  const char *path = argv[1];
  std::ifstream file(path);
  if (!file) {
    std::cerr << "cannot open: " << path << '\n';
    return 1;
  }

  const std::string source = read_stream(file);
  std::string err;
  auto program = gate::parse_program(source, path, &err);
  if (!program) {
    std::cerr << "parse error";
    if (!err.empty()) {
      std::cerr << ": " << err;
    }
    std::cerr << '\n';
    return 1;
  }

  std::cout << "parse ok: " << program->tokens.size() << " tokens\n";
  return 0;
}
