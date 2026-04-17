#include "compiler/statement/stmt_init.hpp"
#include "compiler/CompileError.hpp"
#include "compiler/CompileExpr.hpp"

namespace gate {

void stmt_init(const ast::InitAssign &stmt,
               SymbolTable &symbols,
               NodeEmitter &emitter,
               std::uint32_t parent_component) {
  // compile the expression
  uint32_t node_ind = compile_expr(stmt.value, symbols, emitter, parent_component);

  // Name the symbol for that node. Cant be done in compile_expr because that handles subexprs too
  Node& node = emitter.node_at(node_ind);
  node.name = stmt.target.ident;

  // Check width matches expected
  if (stmt.target.width != node.width)
    throw WidthMismatchError("Initialization of " + stmt.target.ident, stmt.target.width, node.width);

  // Create symbol
  symbols.bind(stmt.target.ident, node_ind);
}

} // namespace gate
