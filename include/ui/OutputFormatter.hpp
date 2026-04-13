/**
 * File: OutputFormatter.hpp
 * Purpose: Format simulation results for human-readable display.
 */
#pragma once

#include "simulation/PreparedCircuit.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gate {

std::vector<std::string> format_outputs(const PreparedCircuit &pc,
                                         const std::vector<uint64_t> &results);

} // namespace gate
