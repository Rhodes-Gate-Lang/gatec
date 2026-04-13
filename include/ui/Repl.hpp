/**
 * File: Repl.hpp
 * Purpose: Interactive REPL for compiling and simulating gate-lang components.
 */
#pragma once

#include "compiler/ComponentRegistry.hpp"
#include "simulation/PreparedCircuit.hpp"

#include <string>
#include <unordered_map>

namespace gate {

class Repl {
public:
  explicit Repl(const ast::Program &program);
  void run();

private:
  ComponentRegistry registry_;
  std::unordered_map<std::string, PreparedCircuit> cache_;

  void handle_run(const std::string &args);
  void handle_list() const;
  void handle_help() const;
};

} // namespace gate
