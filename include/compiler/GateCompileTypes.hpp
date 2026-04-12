/**
 * File: GateCompileTypes.hpp
 * Purpose: Small compile-time types for lowering AST into GateObject (no Circuit).
 */
#pragma once

#include <cstdint>

namespace gate {

/// Reference to a node index in the GateObject under construction, plus signal width.
struct GateNodeRef {
  std::uint32_t node_index{};
  int width{};
};

} // namespace gate
