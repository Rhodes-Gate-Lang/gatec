/**
 * File: Repl.hpp
 * Purpose: Interactive REPL for compiling gate-lang components.
 */
#pragma once

#include "compiler/ComponentRegistry.hpp"
#include "compiler/GateoAliases.hpp"
#include "core/Ast.hpp"

#include <string>
#include <unordered_map>

namespace gate {

class Repl {
public:
  explicit Repl(const ast::Program &program);
  void run();

private:
  ComponentRegistry registry_;
  /// // TODO: Once simulation reads GateObject, use compiled objects here (or drop cache).
  std::unordered_map<std::string, GateObject> cache_;

  void handle_run(const std::string &args);
  void handle_list() const;
  void handle_help() const;
};

} // namespace gate
