/**
 * File: Simulation.hpp
 * Purpose: Circuit preparation (topological sort) and gate evaluation.
 */
#pragma once

#include "simulation/PreparedCircuit.hpp"

#include <cstdint>
#include <vector>

namespace gate {

PreparedCircuit prepare(Circuit circuit);

std::vector<uint64_t> simulate(const PreparedCircuit &pc,
                                const std::vector<uint64_t> &inputs);

} // namespace gate
