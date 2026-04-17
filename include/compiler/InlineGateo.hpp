#pragma once

#include "compiler/GateoAliases.hpp"
#include "compiler/NodeEmitter.hpp"

#include <cstdint>
#include <vector>

namespace gate {

struct InlineResult {
  /// Node indices of the child's Input nodes, remapped into the parent's space.
  std::vector<std::uint32_t> input_node_indices;
  /// Node indices of the child's Output nodes, remapped into the parent's space.
  std::vector<std::uint32_t> output_node_indices;
};

/// Splices a self-contained child GateObject into a parent NodeEmitter.
/// Remaps all node and component-instance indices so they coexist in the
/// parent's index space. Returns the remapped input/output boundary indices.
InlineResult inline_gate_object(const GateObject &child,
                                NodeEmitter &parent_emitter,
                                std::uint32_t parent_component);

} // namespace gate
