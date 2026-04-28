#include "compiler/CompileExpr.hpp"
#include "compiler/CompileError.hpp"
#include "compiler/GateoAliases.hpp"
#include <cstdint>
#include <optional>
#include <type_traits>

namespace gate {

namespace {

std::uint32_t compile_var_ref(const ast::VarRef &expr,
                              const SymbolTable &symbols) {
  return symbols.resolve(expr.name);
}

std::uint32_t compile_literal(const ast::Literal &expr,
                              NodeEmitter &emitter,
                              std::uint32_t parent_component) {
  // Derive the minimum number of bits needed to represent the value.
  std::uint32_t w = 1;
  if (expr.bits != 0)
    w = 64u - static_cast<std::uint32_t>(__builtin_clzll(expr.bits));

  Node node;
  node.type   = GateType::Literal;
  node.inputs = {};
  node.width  = w;
  node.parent = parent_component;
  node.name   = std::nullopt;
  node.value  = expr.bits;

  return emitter.emit_node(node);
}

std::uint32_t compile_not(const ast::NotExpr &expr,
                          const SymbolTable &symbols,
                          NodeEmitter &emitter,
                          std::uint32_t parent_component) {
  std::uint32_t operand = compile_expr(*expr.operand, symbols, emitter, parent_component);

  Node node;
  node.type   = GateType::Not;
  node.inputs = {operand};
  node.width  = emitter.node_at(operand).width;
  node.parent = parent_component;
  node.name   = std::nullopt;
  node.value  = std::nullopt;

  return emitter.emit_node(node);
}

std::uint32_t compile_shift(GateType type,
                            const ast::Expr &operand_expr,
                            const ast::Expr &shift_expr,
                            const SymbolTable &symbols,
                            NodeEmitter &emitter,
                            std::uint32_t parent_component) {
  std::uint32_t operand = compile_expr(operand_expr, symbols, emitter, parent_component);
  std::uint32_t shift   = compile_expr(shift_expr,   symbols, emitter, parent_component);

  Node node;
  node.type   = type;
  node.inputs = {operand, shift};
  node.width  = emitter.node_at(operand).width;
  node.parent = parent_component;
  node.name   = std::nullopt;
  node.value  = std::nullopt;

  return emitter.emit_node(node);
}

// Shared handler for And / Or / Xor — all carry `lhs` and `rhs` members.
template <typename BinExprT>
std::uint32_t compile_binary(GateType type,
                             const BinExprT &expr,
                             const SymbolTable &symbols,
                             NodeEmitter &emitter,
                             std::uint32_t parent_component) {
  std::uint32_t lhs = compile_expr(*expr.lhs, symbols, emitter, parent_component);
  std::uint32_t rhs = compile_expr(*expr.rhs, symbols, emitter, parent_component);

  std::uint32_t lhs_width = emitter.node_at(lhs).width;
  std::uint32_t rhs_width = emitter.node_at(rhs).width;
  if (lhs_width != rhs_width)
    throw WidthMismatchError("Binary expression", lhs_width, rhs_width);

  Node node;
  node.type   = type;
  node.inputs = {lhs, rhs};
  node.width  = lhs_width;
  node.parent = parent_component;
  node.name   = std::nullopt;
  node.value  = std::nullopt;

  return emitter.emit_node(node);
}

std::uint32_t compile_split(const ast::SplitExpr &expr,
                            const SymbolTable &symbols,
                            NodeEmitter &emitter,
                            std::uint32_t parent_component) {
  if (expr.lo < 0 || expr.hi < expr.lo)
    throw CompileError("split range [" + std::to_string(expr.lo) + "," +
                       std::to_string(expr.hi) + ") is invalid");

  std::uint32_t operand = compile_expr(*expr.operand, symbols, emitter, parent_component);

  std::uint32_t source_width = emitter.node_at(operand).width;
  if (static_cast<std::uint32_t>(expr.hi) > source_width)
    throw WidthMismatchError("split upper bound", static_cast<int>(source_width), expr.hi);

  std::uint32_t width = static_cast<std::uint32_t>(expr.hi - expr.lo + 1);

  Node node;
  node.type     = GateType::Split;
  node.inputs   = {operand};
  node.width    = width;
  node.parent   = parent_component;
  node.name     = std::nullopt;
  node.value    = std::nullopt;
  node.split_lo = static_cast<std::uint32_t>(expr.lo);

  return emitter.emit_node(node);
}

std::uint32_t compile_merge(const ast::MergeExpr &expr,
                            const SymbolTable &symbols,
                            NodeEmitter &emitter,
                            std::uint32_t parent_component) {
  std::vector<std::uint32_t> inputs;
  inputs.reserve(expr.operands.size());

  std::uint32_t total_width = 0;
  for (const auto &operand : expr.operands) {
    std::uint32_t idx = compile_expr(*operand, symbols, emitter, parent_component);
    total_width += emitter.node_at(idx).width;
    if (total_width > 64)
      throw BusWidthLimitError("merge expression", total_width);
    inputs.push_back(idx);
  }

  Node node;
  node.type   = GateType::Merge;
  node.inputs = std::move(inputs);
  node.width  = total_width;
  node.parent = parent_component;
  node.name   = std::nullopt;
  node.value  = std::nullopt;

  return emitter.emit_node(node);
}

} // anon namespace

std::uint32_t compile_expr(const ast::Expr &expr,
                           const SymbolTable &symbols,
                           NodeEmitter &emitter,
                           std::uint32_t parent_component) {
  return std::visit([&](const auto &e) -> std::uint32_t {
    using T = std::decay_t<decltype(e)>;

    if constexpr (std::is_same_v<T, ast::VarRef>)
      return compile_var_ref(e, symbols);

    else if constexpr (std::is_same_v<T, ast::Literal>)
      return compile_literal(e, emitter, parent_component);

    else if constexpr (std::is_same_v<T, ast::NotExpr>)
      return compile_not(e, symbols, emitter, parent_component);

    else if constexpr (std::is_same_v<T, ast::SplitExpr>)
      return compile_split(e, symbols, emitter, parent_component);

    else if constexpr (std::is_same_v<T, ast::LeftShift>)
      return compile_shift(GateType::Lsl, *e.operand, *e.shift_amount, symbols, emitter, parent_component);

    else if constexpr (std::is_same_v<T, ast::RightShift>)
      return compile_shift(GateType::Lsr, *e.operand, *e.shift_amount, symbols, emitter, parent_component);

    else if constexpr (std::is_same_v<T, ast::AndExpr>)
      return compile_binary(GateType::And, e, symbols, emitter, parent_component);

    else if constexpr (std::is_same_v<T, ast::OrExpr>)
      return compile_binary(GateType::Or, e, symbols, emitter, parent_component);

    else if constexpr (std::is_same_v<T, ast::XorExpr>)
      return compile_binary(GateType::Xor, e, symbols, emitter, parent_component);

    else if constexpr (std::is_same_v<T, ast::MergeExpr>)
      return compile_merge(e, symbols, emitter, parent_component);

  }, expr.data);
}

} // namespace gate
