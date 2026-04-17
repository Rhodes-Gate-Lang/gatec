#include "compiler/GateoAliases.hpp"
#include "compiler/statement/stmt_comp_call.hpp"
#include "compiler/CompCache.hpp"
#include "compiler/CompileError.hpp"
#include "compiler/InlineGateo.hpp"

namespace gate {

void stmt_comp_call(const ast::CompCall &stmt,
                    SymbolTable &symbols,
                    NodeEmitter &emitter,
                    std::uint32_t parent_component,
                    CompCache &cache) {
  // Grab the subcomp
  GateObject subcomp = cache.resolve(stmt.comp);

  InlineResult subcomp_io_info = inline_gate_object(subcomp, emitter, parent_component);

  // Ensure correct amount of parameters were given
  if (subcomp_io_info.input_node_indices.size() != stmt.args.size())
    throw ArityMismatchError(stmt.comp + " parameters", subcomp_io_info.input_node_indices.size(), stmt.args.size());

  // Loop through parameter inputs to resolve and wire
  for (size_t i = 0; i < stmt.args.size(); ++i) {
    uint32_t wire_in_node_ind = symbols.resolve(stmt.args[i]);

    const Node wire_in_node = emitter.node_at(wire_in_node_ind);
    Node& subcomp_input_node = emitter.node_at(subcomp_io_info.input_node_indices[i]);

    // Verify width match
    if (wire_in_node.width != subcomp_input_node.width)
      throw WidthMismatchError(stmt.comp + ", parameter " + stmt.args[i], subcomp_input_node.width, wire_in_node.width);

    // Wire input
    subcomp_input_node.inputs = {wire_in_node_ind};
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
