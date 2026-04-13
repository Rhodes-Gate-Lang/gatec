#include "compiler/statement/stmt_return.hpp"
#include "compiler/GateoAliases.hpp"

namespace gate {

void stmt_return(const ast::ReturnStmt &stmt,
                 const SymbolTable &symbols,
                 NodeEmitter &emitter,
                 std::uint32_t parent_component) {
  // Loop through the outputs, add output nodes for each
  for (const std::string& symbol : stmt.names) {
    uint32_t node_ind = symbols.resolve(symbol);

    Node out_node;
    out_node.type = GateType::Output;
    out_node.inputs = {node_ind};
    out_node.width = emitter.node_at(node_ind).width;
    out_node.parent = parent_component;
    out_node.name = symbol; // does this need std::move
    out_node.literal_value = std::nullopt; // not applicable for output nodes

    emitter.emit_node(out_node);
  }
}

} // namespace gate
