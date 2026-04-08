/**
 * File: Simulation.hpp
 * Purpose: Circuit preparation (topological sort) and evaluation.
 */
#pragma once

#include "Circuit.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gate {

PreparedCircuit prepare(Circuit circuit);

std::vector<uint64_t> simulate(const PreparedCircuit &pc,
                                const std::vector<uint64_t> &inputs);

std::vector<std::string> format_outputs(const PreparedCircuit &pc,
                                         const std::vector<uint64_t> &results);

} // namespace gate
