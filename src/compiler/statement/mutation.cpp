#include "compiler/statement/stmt_mutation.hpp"
#include "compiler/CompileError.hpp"
#include "compiler/CompileExpr.hpp"
#include <cstdint>

namespace gate {

void stmt_mutation(const ast::MutAssign &stmt,
                   SymbolTable &symbols,
                   NodeEmitter &emitter,
                   std::uint32_t parent_component) {
  // compile the expression
  uint32_t new_node_ind = compile_expr(stmt.value, symbols, emitter, parent_component);

  // Name the node. Can't be done in compile_expr because it handles subexprs too
  Node& new_node = emitter.node_at(new_node_ind);
  new_node.name = stmt.target;

  // Check that the new expression's width matches the old one
  Node& existing_node = emitter.node_at(symbols.resolve(stmt.target));
  if (existing_node.width != new_node.width)
    throw WidthMismatchError("Mutation of " + stmt.target, existing_node.width, new_node.width);

  // Rebind the symbol to the new node
  symbols.rebind(stmt.target, new_node_ind);
}

} // namespace gate
