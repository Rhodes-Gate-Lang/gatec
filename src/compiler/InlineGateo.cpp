#include "compiler/InlineGateo.hpp"
#include "compiler/GateoAliases.hpp"
#include <cstdint>

namespace gate {

InlineResult inline_gate_object(const GateObject &child,
                                NodeEmitter &parent_emitter,
                                std::uint32_t parent_component) {
  InlineResult res;

  uint32_t node_base = parent_emitter.node_count();
  uint32_t comp_base = parent_emitter.component_count();

  // Remap the nodes
  for (Node node : child.nodes) { // Copy sematics on purpose
    for (uint32_t& input : node.inputs)
      input += node_base;

    node.parent += comp_base;

    uint32_t ind = parent_emitter.emit_node(node);

    if (node.type == GateType::Input && node.parent == comp_base)
      res.input_node_indices.push_back(ind);

    if (node.type == GateType::Output && node.parent == comp_base)
      res.output_node_indices.push_back(ind);
  }

  // Link subcomp root to its caller
  ComponentInstance subcomp_root = child.components[0];
  parent_emitter.emit_component(subcomp_root.name, parent_component);

  // Shift the rest of the components of the subcomp by the existing base
  for (size_t i = 1; i < child.components.size(); ++i) {
    const auto &src = child.components[i];
    parent_emitter.emit_component(src.name, src.parent + comp_base);
  }

  return res;
}

} // namespace gate
