/**
 * File: InlineGateo.cpp
 * Purpose: Implementation of GateObject inlining (see InlineGateo.hpp).
 */
#include "compiler/InlineGateo.hpp"

namespace gate {

InlineGateoResult inline_gateo_object(
    GateObject &caller,
    const GateObject &callee,
    const std::uint32_t caller_instance_index,
    const std::vector<std::uint32_t> &argument_nodes) {
  (void)caller;
  (void)callee;
  (void)caller_instance_index;
  (void)argument_nodes;

  // TODO: Implement merge per design: input binding, node append + remap, component table merge,
  //       output_node_indices filled for each callee return / Output node.

  return InlineGateoResult{};
}

} // namespace gate
