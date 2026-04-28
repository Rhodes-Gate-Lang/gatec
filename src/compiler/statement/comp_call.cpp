#include "compiler/GateoAliases.hpp"
#include "compiler/statement/stmt_comp_call.hpp"
#include "compiler/CompCache.hpp"
#include "compiler/CompileError.hpp"
#include "compiler/CompileExpr.hpp"
#include "compiler/InlineGateo.hpp"

#include <cstdint>
#include <vector>

namespace gate {

void stmt_comp_call(const ast::CompCall &stmt,
                    SymbolTable &symbols,
                    NodeEmitter &emitter,
                    std::uint32_t parent_component,
                    CompCache &cache) {
  // Grab the subcomp
  GateObject subcomp = cache.resolve(stmt.comp);

  // Formal input node indices in the cached child (same criterion as InlineGateo:
  // component-local Input ports before parent remapping).
  std::vector<std::uint32_t> child_formal_input_indices;
  child_formal_input_indices.reserve(subcomp.nodes.size());
  for (std::uint32_t ci = 0; ci < subcomp.nodes.size(); ++ci) {
    const Node &cn = subcomp.nodes[ci];
    if (cn.type == GateType::Input && cn.parent == 0)
      child_formal_input_indices.push_back(ci);
  }
  if (child_formal_input_indices.size() != stmt.args.size())
    throw ArityMismatchError(stmt.comp + " parameters", child_formal_input_indices.size(), stmt.args.size());

  // Compile actual arguments *before* inlining so every driver net has a lower
  // node index than the instance input ports (.gateo requires a topological order).
  std::vector<std::uint32_t> actual_wire_indices;
  actual_wire_indices.reserve(stmt.args.size());
  for (size_t i = 0; i < stmt.args.size(); ++i) {
    const std::uint32_t wire_in_node_ind =
        compile_expr(stmt.args[i], symbols, emitter, parent_component);
    const Node &wire_in_node = emitter.node_at(wire_in_node_ind);
    const Node &formal = subcomp.nodes[child_formal_input_indices[i]];
    if (wire_in_node.width != formal.width)
      throw WidthMismatchError(stmt.comp + ", argument " + std::to_string(i + 1), formal.width, wire_in_node.width);
    actual_wire_indices.push_back(wire_in_node_ind);
  }

  InlineResult subcomp_io_info = inline_gate_object(subcomp, emitter, parent_component);

  if (subcomp_io_info.input_node_indices.size() != actual_wire_indices.size())
    throw ArityMismatchError(stmt.comp + " parameters (inline)", subcomp_io_info.input_node_indices.size(), actual_wire_indices.size());

  for (size_t i = 0; i < actual_wire_indices.size(); ++i) {
    emitter.node_at(subcomp_io_info.input_node_indices[i]).inputs = {actual_wire_indices[i]};
  }

  // Ensure correct amount of outputs were assigned
  if (subcomp_io_info.output_node_indices.size() != stmt.outputs.size())
    throw ArityMismatchError(stmt.comp + ", outputs", subcomp_io_info.output_node_indices.size(), stmt.outputs.size());

  // Loop through output nodes to rename and bind
  for (size_t i = 0; i < stmt.outputs.size(); ++i) {
    Node& subcomp_output_node = emitter.node_at(subcomp_io_info.output_node_indices[i]);

    // Check width
    if (stmt.outputs[i].width != subcomp_output_node.width)
      throw WidthMismatchError(stmt.comp + ", output " + stmt.outputs[i].ident, subcomp_output_node.width, stmt.outputs[i].width);

    // Rename the output node
    subcomp_output_node.name = stmt.outputs[i].ident;

    // Bind in symbol table
    symbols.bind(stmt.outputs[i].ident, subcomp_io_info.output_node_indices[i]);
  }
}

} // namespace gate
