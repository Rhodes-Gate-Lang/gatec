/**
 * File: Compiler.cpp
 * Purpose: AST → DAG compilation with semantic validation.
 */
#include "Compiler.hpp"
#include "Ast.hpp"
#include "DAG.hpp"

#include <locale>
#include <optional>
#include <variant>

namespace gate {

namespace {

// ── Error helper ────────────────────────────────────────────────────────────

bool report(std::string *out, const std::string &msg) {
  if (out) *out = msg;
  return false;
}

GateType binop_to_gatetype(ast::BinOp binop) {
  switch (binop) {
    case ast::BinOp::And:
      return GateType::And;
    case ast::BinOp::Or:
      return GateType::Or;
    case ast::BinOp::Xor:
      return GateType::Xor;
    default:
      fprintf(stderr, "Missing a BinOp type in binop_to_gatetype\n");
  }
}

// ── Expression compilation ──────────────────────────────────────────────────

// Recursively compile an expression into the DAG.
// Returns the Signal (node index + width) for the expression's result.
// On error, writes to error_out and returns std::nullopt.
std::optional<Signal> compile_expr(const ast::Expr &expr, DAG &dag,
                                   const SymbolTable &symtab,
                                   std::string *error_out) {
  // Switch on the expression type
  if (const std::string* id = std::get_if<std::string>(&expr.data)) {
    if (symtab.find(*id) == symtab.end()) {
      // TODO: Unsure how to use error_out
      return std::nullopt;
    }
    return symtab.at(*id);
  }

  else if (const ast::UnaryExpr* unary = std::get_if<ast::UnaryExpr>(&expr.data)) {
    // Use *unary
    std::optional<Signal> subsignal = compile_expr(*unary->operand, dag, symtab, error_out);
    if (subsignal) {
      Signal val = subsignal.value();
      size_t new_node = dag.add_node(GateType::Not, {val.node}, val.width);
      return Signal{new_node, val.width};
    }
    // Error handling?
    return std::nullopt; // Subexpr failed
  }

  else if (const ast::BinExpr* bin = std::get_if<ast::BinExpr>(&expr.data)) {
    std::optional<Signal> lSubsig = compile_expr(*bin->lhs, dag, symtab, error_out);
    std::optional<Signal> rSubSig = compile_expr(*bin->rhs, dag, symtab, error_out);
    if (lSubsig && rSubSig) {
      Signal lVal = lSubsig.value();
      Signal rVal = rSubSig.value();

      // TODO: Error check here for width mismatch

      size_t new_node = dag.add_node(binop_to_gatetype(bin->op), {lVal.node, rVal.node}, lVal.width);
      return Signal{new_node, lVal.width};
    }
    // Error handling?
    return std::nullopt;
  }

  else {
    // TODO: Throw error for unhandled expression type in compiler
  }
  return std::nullopt;
}

// ── Body compilation ────────────────────────────────────────────────────────

// Process one component's body, adding gates into `dag`.
// `arg_nodes` maps each formal parameter (by position) to an existing node
// index in the DAG — either an Input node (top-level) or a caller's node
// (inlined call).
// Returns the node indices for the component's return values.
std::optional<std::vector<size_t>> compile_body(const ast::Comp &comp,
                                                const std::vector<size_t> &arg_nodes,
                                                DAG &dag,
                                                const ComponentRegistry &registry,
                                                std::string *error_out) {
  SymbolTable symtab;

  // ── Bind formal parameters to argument nodes ──
  if (arg_nodes.size() != comp.params.size()) {
    // TODO: Throw error for comp input num mismatch
  }

  for (int i = 0; i < static_cast<int>(comp.params.size()); ++i) {
    const auto &param = comp.params[i];
    symtab[param.ident] = Signal{arg_nodes[i], param.width};
  }

  std::vector<size_t> return_nodes;

  for (const auto &stmt : comp.body) {
    // Switch on the different types of statements
    if (const ast::InitAssign* initStmt = std::get_if<ast::InitAssign>(&stmt)) {
      if (symtab.find(initStmt->target.ident) != symtab.end()) {
        // TODO: Error for symbol already exists
      }

      auto sig = compile_expr(initStmt->value, dag, symtab, error_out);
      if (sig) {
        Signal val = sig.value();
        if (val.width != initStmt->target.width) {
          // TODO: Error for width mismatch
        }
        symtab[initStmt->target.ident] = val;
      }
      // Error check for nullopt?
    }

    else if (const ast::MutAssign* mutStmt = std::get_if<ast::MutAssign>(&stmt)) {
      if (symtab.find(initStmt->target.ident) == symtab.end()) {
        // TODO: Error for symbol does not exist
      }

      auto sig = compile_expr(mutStmt->value, dag, symtab, error_out);
      if (sig) {
        Signal val = sig.value();
        int target_width = symtab.at(mutStmt->target).width;
        if (val.width != target_width) {
          // TODO: Error for width mismatch
        }
        symtab[initStmt->target.ident] = val;
      }
      // Error check for nullopt?
    }

    else if (const ast::CompCall* compCallStmt = std::get_if<ast::CompCall>(&stmt)) {
      if (registry.find(compCallStmt->comp) == registry.end()) {
        // Error for unknown component
      }

      std::vector<size_t> input_nodes;
      for (const std::string& arg : compCallStmt->args) {
        if (symtab.find(arg) == symtab.end()) {
          // TODO: Error for missing symbol
        }
        input_nodes.push_back(symtab[arg].node);
      }

      if (input_nodes.size() != compCallStmt->args.size()) {
        // TODO: Throw for incorrect param inputs
      }

      for (int i = 0; i < input_nodes.size(); ++i) {
        // TODO: Verify widths
      }

      std::optional<DAG> sub_dag = compile_component(compCallStmt->comp, registry, error_out);
      // How to handle optional here?
      // TODO: Bind outputs
    }

    else if (const ast::ReturnStmt* returnStmt = std::get_if<ast::ReturnStmt>(&stmt)) {
      for (const std::string& name : returnStmt->names) {
        if (symtab.find(name) == symtab.end()) {
          // TODO: Throw for unknown symbol
        }
        size_t node_ind = symtab[name].node;
        dag.outputs.push_back(std::make_pair(name, node_ind));
      }
    }

    else {
      // TODO: Error for unhandled statement type in compiler
    }
    // CompCall:
    //   Look up stmt.comp in registry — error if not found.
    //   Resolve each arg name in symtab to get node indices.
    //   Validate arg count and widths against the called comp's params.
    //   Call compile_body recursively with the sub-comp's AST and arg node indices.
    //   Bind each output name (stmt.outputs[i].ident) to the returned node indices.
    //
    // ReturnStmt:
    //   For each name, look up in symtab and push node index to return_nodes.

    (void)stmt;
  }

  return return_nodes;
}

} // namespace

// ── Public API ──────────────────────────────────────────────────────────────

ComponentRegistry build_registry(const ast::Program &program) {
  ComponentRegistry reg;
  for (const auto &comp : program.components) {
    reg[comp.name] = comp;
  }
  return reg;
}

std::optional<DAG> compile_component(const std::string &name,
                                     const ComponentRegistry &registry,
                                     std::string *error_out) {
  auto it = registry.find(name);
  if (it == registry.end()) {
    report(error_out, "undefined component: " + name);
    return std::nullopt;
  }

  const ast::Comp &comp = it->second;
  DAG dag;

  // Create input nodes (first n nodes in the DAG, by convention).
  std::vector<size_t> input_nodes;
  for (const auto &param : comp.params) {
    size_t idx = dag.add_node(GateType::Input, {}, param.width);
    input_nodes.push_back(idx);
  }
  dag.num_inputs = input_nodes.size();

  auto ret = compile_body(comp, input_nodes, dag, registry, error_out);
  if (!ret) return std::nullopt;

  // Populate named outputs from the return statement.
  // compile_body fills return_nodes; pair them with names from the comp's
  // return statement (the last statement in body should be ReturnStmt).
  // NOTE: compile_body already built dag.outputs via ReturnStmt handling,
  // OR you can pair them here — your choice on where to put this logic.

  dag.topological_sort();
  return dag;
}

} // namespace gate
