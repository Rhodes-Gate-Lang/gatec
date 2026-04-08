/**
 * File: Simulation.cpp
 * Purpose: DAG topological sort (Kahn's algorithm) and gate evaluation loop.
 */
#include "Simulation.hpp"

#include <cassert>
#include <queue>
#include <sstream>
#include <stdexcept>

namespace gate {

// ── DAG member implementations ──────────────────────────────────────────────

size_t DAG::add_node(GateType type, std::vector<size_t> inputs, int width) {
  size_t idx = nodes.size();
  nodes.push_back(Node{type, std::move(inputs), width, 0});
  return idx;
}

void DAG::topological_sort() {
  // TODO(4): Kahn's algorithm.
  //
  // 1. Build an in-degree count for each node.
  //    For every node, increment in-degree of each node that lists it as an input.
  //    (Careful: "inputs" on a node means "nodes I read from", so iterate each
  //     node's inputs vector to find its *dependencies*, then build a reverse
  //     adjacency list or just count consumers.)
  //
  //    Actually, in-degree here means: how many of this node's *own* inputs
  //    haven't been processed yet. So:
  //      in_degree[i] = nodes[i].inputs.size()
  //
  //    And you need a "consumers" adjacency list: for each node, which other
  //    nodes list it in their inputs?
  //
  // 2. Seed a queue with all nodes that have in_degree == 0 (the Input nodes).
  //
  // 3. While queue is non-empty:
  //    - Dequeue node, append to topo_order.
  //    - For each consumer of that node, decrement its in_degree.
  //      If it reaches 0, enqueue it.
  //
  // 4. Assert topo_order.size() == nodes.size() (no cycles).

  topo_order.clear();
}

std::vector<uint64_t> DAG::operator()(const std::vector<uint64_t> &inputs) {
  // TODO(5): Evaluate the circuit.
  //
  // 1. Set input node values:
  //    for i in [0, num_inputs): nodes[i].val = inputs[i] & mask(nodes[i].width)
  //
  // 2. Walk topo_order. For each node (skip Input nodes):
  //    - Read input values from nodes[n.inputs[0]].val, etc.
  //    - Compute:
  //        Not: ~in0
  //        And: in0 & in1
  //        Or:  in0 | in1
  //        Xor: in0 ^ in1
  //    - Mask result: val &= (1ULL << width) - 1
  //    - Store in node.val
  //
  // 3. Collect outputs:
  //    for each (name, idx) in this->outputs: push nodes[idx].val

  (void)inputs;
  return {};
}

// ── Utilities ───────────────────────────────────────────────────────────────

std::vector<std::string> format_outputs(const DAG &dag,
                                        const std::vector<uint64_t> &results) {
  // TODO(6): Pair each result with the corresponding output name from dag.outputs.
  // Return strings like "sum = 1", "cout = 0".

  (void)dag;
  (void)results;
  return {};
}

} // namespace gate
