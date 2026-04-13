/**
 * File: Circuit.hpp
 * Purpose: Core data structures for the compiled circuit graph.
 *
 * These types are the shared vocabulary for the gate-lang pipeline.
 * GateType, Signal, Node, and Circuit belong together — they all describe
 * what a circuit IS. PreparedCircuit (the evaluation-ready form) lives in
 * simulation/PreparedCircuit.hpp since it's a simulation-phase concept.
 */
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace gate {

enum class GateType { Input, And, Or, Xor, Not };

struct Signal {
  size_t node;
  int width;
};

struct NamedSignal {
  std::string name;
  Signal signal;
};

struct Node {
  GateType type;
  std::vector<size_t> inputs;
  int width;
};

struct Circuit {
  std::vector<Node> nodes;
  size_t num_inputs = 0;
  std::vector<NamedSignal> outputs;

  Signal add_node(GateType type, std::vector<size_t> node_inputs, int width) {
    size_t idx = nodes.size();
    nodes.push_back(Node{type, std::move(node_inputs), width});
    return Signal{idx, width};
  }
};

} // namespace gate
