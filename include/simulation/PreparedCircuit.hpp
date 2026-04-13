/**
 * File: PreparedCircuit.hpp
 * Purpose: Circuit augmented with a topological evaluation order.
 *
 * This is the boundary type between compilation and simulation: the compiler
 * produces a Circuit, prepare() adds the eval_order, and simulate() consumes
 * the result.
 */
#pragma once

#include "core/Circuit.hpp"

#include <cstddef>
#include <vector>

namespace gate {

struct PreparedCircuit {
  Circuit circuit;
  std::vector<size_t> eval_order;
};

} // namespace gate
