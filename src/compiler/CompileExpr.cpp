#include "compiler/CompileExpr.hpp"
#include "compiler/CompileError.hpp"
#include "compiler/GateoAliases.hpp"
#include <cstdint>
#include <optional>

namespace gate {

namespace {

GateType binop_to_gatetype(ast::BinOp op) {
  switch (op) {
    case ast::BinOp::And:
      return GateType::And;
    case ast::BinOp::Or:
      return GateType::Or;
    case ast::BinOp::Xor:
      return GateType::Xor;
    default:
      throw CompileError("Hit an unknown gatetype in binop_to_gatetype");
  }
}


std::uint32_t compile_var_ref(const std::string &name,
                              const SymbolTable &symbols) {
  return symbols.resolve(name);
}


std::uint32_t compile_unary(const ast::UnaryExpr &expr,
                            const SymbolTable &symbols,
                            NodeEmitter &emitter,
                            std::uint32_t parent_component) {
  uint32_t subexpr = compile_expr(*expr.operand, symbols, emitter, parent_component);

  Node node;
  node.type = GateType::Not;
  node.inputs = {subexpr};
  node.width = emitter.node_at(subexpr).width;
  node.parent = parent_component;
  node.name = std::nullopt; // Could be subexpr. Renamed in compile_statement if has a symbol
  node.literal_value = std::nullopt; // not applicable to unary op

  return emitter.emit_node(node);
}


std::uint32_t compile_binary(const ast::BinExpr &expr,
                             const SymbolTable &symbols,
                             NodeEmitter &emitter,
                             std::uint32_t parent_component) {
  uint32_t lhs = compile_expr(*expr.lhs, symbols, emitter, parent_component);
  uint32_t rhs = compile_expr(*expr.rhs, symbols, emitter, parent_component);

  // Check lhs and rhs are the same width
  Node lh_node = emitter.node_at(lhs);
  Node rh_node = emitter.node_at(rhs);
  if (lh_node.width != rh_node.width)
    throw WidthMismatchError("Binary Expression evaluation", lh_node.width, rh_node.width);

  // Construct the new node
  Node node;
  node.type = binop_to_gatetype(expr.op);
  node.inputs = {lhs, rhs};
  node.width = lh_node.width;
  node.parent = parent_component;
  node.name = std::nullopt; // Could be subexpr. Renamed in compile_statement if has a symbol
  node.literal_value = std::nullopt; // literal_value not applicable to a binary operation

  return emitter.emit_node(node);
}

} // anon namespace

std::uint32_t compile_expr(const ast::Expr &expr,
                           const SymbolTable &symbols,
                           NodeEmitter &emitter,
                           std::uint32_t parent_component) {
  // TODO: Switch to std::visit at some point
  switch (expr.data.index()) {
    case 0:
      return compile_var_ref(std::get<std::string>(expr.data), symbols);
    case 1:
      return compile_unary(std::get<ast::UnaryExpr>(expr.data), symbols, emitter, parent_component);
    case 2:
      return compile_binary(std::get<ast::BinExpr>(expr.data), symbols, emitter, parent_component);
    default:
      throw CompileError("Invalid expression type"); // TODO: add error for uncaught expression types
  }
}

} // namespace gate
