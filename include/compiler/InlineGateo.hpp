/**
 * File: InlineGateo.hpp
 * Purpose: Splice one fully compiled GateObject into another (same shape as on-disk .gateo).
 *
 * Both caller and callee are complete GateObjects — e.g. produced by this compiler or
 * loaded via gateo::read_file. There is no separate "fragment" type.
 */
#pragma once

#include "compiler/GateoAliases.hpp"

#include <cstdint>
#include <vector>

namespace gate {

/// Node indices in the caller’s `GateObject::nodes` after the inline, in callee return order.
struct InlineGateoResult {
  std::vector<std::uint32_t> output_node_indices;
};

/**
 * Inline `callee` into `caller` at the current call site.
 *
 * // TODO: Define and document callee boundary convention (parameter Input nodes,
 * //       Output nodes for returns, ordering) so callers know how to build `argument_nodes`.
 * // TODO: Remap callee non-input nodes into caller, shift component indices, update Node::parent.
 * // TODO: Merge GateObject::components trees (offset indices, attach callee root under caller_instance_index).
 * // TODO: Enforce width/arity checks before mutating caller (mirror old handle_comp_call).
 */
[[nodiscard]] InlineGateoResult inline_gateo_object(
    GateObject &caller,
    const GateObject &callee,
    std::uint32_t caller_instance_index,
    const std::vector<std::uint32_t> &argument_nodes);

} // namespace gate
