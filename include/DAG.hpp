/**
 * File: DAG.hpp
 * Purpose: Core data structures for the compiled circuit graph.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace gate {

enum class GateType { Input, And, Or, Xor, Not };

struct Node {
  GateType type;
  std::vector<size_t> inputs;
  int width;
  uint64_t val = 0;
};

struct Signal {
  size_t node;
  int width;
};

struct DAG {
  std::vector<Node> nodes;
  size_t num_inputs = 0;
  std::vector<std::pair<std::string, size_t>> outputs; // TODO: This should be Signal rather than size_t, right?
  std::vector<size_t> topo_order;

  size_t add_node(GateType type, std::vector<size_t> inputs, int width);

  void topological_sort();

  std::vector<uint64_t> operator()(const std::vector<uint64_t> &inputs);
};

} // namespace gate
