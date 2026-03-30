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

// TODO: The define the other substructs

struct Program {
  std::vector<std::string> components; // Would be a Component type later, string for now
};

} // namespace gate::ast
