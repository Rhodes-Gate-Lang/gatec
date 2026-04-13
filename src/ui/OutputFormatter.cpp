/**
 * File: OutputFormatter.cpp
 * Purpose: Format simulation results for human-readable display.
 */
#include "ui/OutputFormatter.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace gate {

namespace {

std::string to_binary(uint64_t val, int width) {
  std::string result;
  result.reserve(width);
  for (int b = width - 1; b >= 0; --b) {
    result += ((val >> b) & 1) ? '1' : '0';
  }
  return result;
}

} // namespace

std::vector<std::string> format_outputs(const PreparedCircuit &pc,
                                         const std::vector<uint64_t> &results) {
  std::vector<std::string> lines;
  lines.reserve(pc.circuit.outputs.size());
  for (size_t i = 0; i < pc.circuit.outputs.size() && i < results.size(); ++i) {
    const auto &out = pc.circuit.outputs[i];
    std::ostringstream ss;
    ss << out.name << " = " << results[i]
       << " (0b" << to_binary(results[i], out.signal.width) << ")";
    lines.push_back(ss.str());
  }
  return lines;
}

} // namespace gate
