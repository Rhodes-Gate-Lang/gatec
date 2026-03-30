/**
 * File: Ast.hpp
 * Purpose: AST shape for GateLang — expand nested types as the grammar grows.
 */
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace gate::ast {

struct Program {
  std::vector<std::string> tokens;
};

} // namespace gate::ast
