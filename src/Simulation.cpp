/**
 * File: Simulation.cpp
 * Purpose: Circuit preparation (Kahn's algorithm) and gate evaluation loop.
 */
#include "Simulation.hpp"

#include <cassert>
#include <queue>
#include <sstream>

namespace gate {

PreparedCircuit prepare(Circuit circuit) {
  // TODO: Kahn's algorithm.
  //
  // 1. Build in-degree count: in_degree[i] = nodes[i].inputs.size().
  // 2. Build consumers adjacency list: for each node, which nodes read from it?
  // 3. Seed queue with all nodes where in_degree == 0 (Input nodes).
  // 4. Process: dequeue → append to eval_order → decrement consumers' in_degree
  //    → enqueue any that reach zero.
  // 5. Assert eval_order.size() == nodes.size() (no cycles).

  PreparedCircuit pc;
  pc.circuit = std::move(circuit);
  return pc;
}

std::vector<uint64_t> simulate(const PreparedCircuit &pc,
                                const std::vector<uint64_t> &inputs) {
  // TODO: Evaluate the circuit.
  //
  // 1. Allocate values buffer: vector<uint64_t>(nodes.size(), 0).
  // 2. Set input values: values[i] = inputs[i] & mask(width) for i < num_inputs.
  // 3. Walk pc.eval_order, skip Input nodes:
  //    - Read input values from values[node.inputs[0]], etc.
  //    - Compute: Not ~in0, And in0&in1, Or in0|in1, Xor in0^in1.
  //    - Mask: values[idx] &= (1ULL << width) - 1.
  // 4. Collect: for each output, push values[output.signal.node].

  (void)pc;
  (void)inputs;
  return {};
}

std::vector<std::string> format_outputs(const PreparedCircuit &pc,
                                         const std::vector<uint64_t> &results) {
  // TODO: Pair each result with the corresponding output name.
  // Return strings like "sum = 1", "cout = 0".

  (void)pc;
  (void)results;
  return {};
}

} // namespace gate
