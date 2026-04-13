/**
 * File: Simulation.cpp
 * Purpose: Circuit preparation (Kahn's algorithm) and gate evaluation loop.
 */
#include "simulation/Simulation.hpp"

#include <cassert>
#include <cstdint>
#include <queue>
#include <vector>

namespace gate {

// ── prepare ─────────────────────────────────────────────────────────────────
/// Topologically sorts the circuit nodes via Kahn's algorithm so that every
/// node is evaluated after all of its inputs.
PreparedCircuit prepare(Circuit circuit) {
  PreparedCircuit pc;
  pc.circuit = std::move(circuit);

  const auto &nodes = pc.circuit.nodes;
  const size_t n = nodes.size();

  std::vector<size_t> in_degree(n, 0);
  std::vector<std::vector<size_t>> consumers(n);

  for (size_t v = 0; v < n; ++v) {
    in_degree[v] = nodes[v].inputs.size();
    for (size_t u : nodes[v].inputs) {
      assert(u < n && "node input index out of bounds");
      consumers[u].push_back(v);
    }
  }

  std::queue<size_t> ready;
  for (size_t v = 0; v < n; ++v) {
    if (in_degree[v] == 0) ready.push(v);
  }

  pc.eval_order.clear();
  pc.eval_order.reserve(n);

  while (!ready.empty()) {
    const size_t u = ready.front();
    ready.pop();
    pc.eval_order.push_back(u);

    for (size_t v : consumers[u]) {
      assert(in_degree[v] > 0 && "in_degree underflow (corrupt graph?)");
      --in_degree[v];
      if (in_degree[v] == 0) ready.push(v);
    }
  }

  assert(pc.eval_order.size() == n && "circuit contains a cycle");

  return pc;
}

// ── simulate ────────────────────────────────────────────────────────────────
/// Evaluates the circuit against concrete input values. Walks eval_order,
/// computing each gate from its input nodes' values. Returns one uint64_t
/// per named output.
std::vector<uint64_t> simulate(const PreparedCircuit &pc,
                                const std::vector<uint64_t> &inputs) {
  const auto &nodes = pc.circuit.nodes;
  std::vector<uint64_t> values(nodes.size(), 0);

  for (size_t i = 0; i < pc.circuit.num_inputs; ++i) {
    uint64_t raw = (i < inputs.size()) ? inputs[i] : 0;
    uint64_t mask = (nodes[i].width >= 64) ? ~0ULL : (1ULL << nodes[i].width) - 1;
    values[i] = raw & mask;
  }

  for (size_t idx : pc.eval_order) {
    const Node &node = nodes[idx];
    if (node.type == GateType::Input) continue;

    uint64_t mask = (node.width >= 64) ? ~0ULL : (1ULL << node.width) - 1;

    switch (node.type) {
    case GateType::Not:
      values[idx] = ~values[node.inputs[0]] & mask;
      break;
    case GateType::And:
      values[idx] = (values[node.inputs[0]] & values[node.inputs[1]]) & mask;
      break;
    case GateType::Or:
      values[idx] = (values[node.inputs[0]] | values[node.inputs[1]]) & mask;
      break;
    case GateType::Xor:
      values[idx] = (values[node.inputs[0]] ^ values[node.inputs[1]]) & mask;
      break;
    case GateType::Input:
      break;
    }
  }

  std::vector<uint64_t> results;
  results.reserve(pc.circuit.outputs.size());
  for (const auto &out : pc.circuit.outputs) {
    results.push_back(values[out.signal.node]);
  }
  return results;
}

} // namespace gate
