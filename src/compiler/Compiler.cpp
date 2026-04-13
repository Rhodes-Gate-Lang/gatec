/**
 * File: Compiler.cpp
 * Purpose: AST -> Circuit compilation with semantic validation.
 *
 * Architecture:
 *   - CompilationSession: memoizes compiled components, detects cycles.
 *   - ComponentCompiler:  compiles one component scope into a Circuit.
 *   - inline_circuit:     copies a pre-compiled callee into a target Circuit,
 *                          remapping node indices and binding argument signals.
 *
 * All error paths throw CompileError subclasses. No bools, no optionals.
 */
#include "compiler/Compiler.hpp"
#include "compiler/CompileError.hpp"
#include "compiler/ComponentRegistry.hpp"
#include "compiler/SymbolTable.hpp"
#include "core/Ast.hpp"
#include "core/Circuit.hpp"

#include <cassert>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace gate {

namespace {

// ── Helpers ─────────────────────────────────────────────────────────────────

/// Readable std::visit for std::variant — each lambda handles one type.
/// Usage: std::visit(overload{ [](const A&){ ... }, [](const B&){ ... } }, v);
template <typename... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};
template <typename... Ts>
overload(Ts...) -> overload<Ts...>;

GateType binop_to_gatetype(ast::BinOp op) {
  switch (op) {
  case ast::BinOp::And: return GateType::And;
  case ast::BinOp::Or:  return GateType::Or;
  case ast::BinOp::Xor: return GateType::Xor;
  }
  return GateType::And;
}

class CompilationSession; // forward declaration

// ── inline_circuit ──────────────────────────────────────────────────────────
/// Copies a pre-compiled callee Circuit into a target Circuit, remapping all
/// node indices. Callee Input nodes are replaced by the caller's argument
/// signals; non-Input nodes are appended to the target with adjusted
/// references. Returns the remapped output signals.
std::vector<Signal> inline_circuit(Circuit &target, const Circuit &callee,
                                   const std::vector<Signal> &args) {
  assert(args.size() == callee.num_inputs);

  std::vector<size_t> remap(callee.nodes.size());

  for (size_t i = 0; i < callee.num_inputs; ++i)
    remap[i] = args[i].node;

  for (size_t i = callee.num_inputs; i < callee.nodes.size(); ++i) {
    std::vector<size_t> remapped_inputs;
    remapped_inputs.reserve(callee.nodes[i].inputs.size());
    for (size_t inp : callee.nodes[i].inputs)
      remapped_inputs.push_back(remap[inp]);

    Signal sig = target.add_node(callee.nodes[i].type,
                                 std::move(remapped_inputs),
                                 callee.nodes[i].width);
    remap[i] = sig.node;
  }

  std::vector<Signal> outputs;
  outputs.reserve(callee.outputs.size());
  for (const auto &out : callee.outputs)
    outputs.push_back(Signal{remap[out.signal.node], out.signal.width});
  return outputs;
}

// ── CompilationSession ──────────────────────────────────────────────────────
/// Manages memoized compilation of components. Each component is compiled at
/// most once; the result is cached for future inlining. Detects cyclic
/// dependencies via an in-progress set.

class CompilationSession {
public:
  explicit CompilationSession(const ComponentRegistry &registry)
      : registry_(registry) {}

  /// Returns a reference to the compiled Circuit for the named component.
  /// Compiles it (and any transitive dependencies) on first request.
  const Circuit &compile_or_get(const std::string &name);

  const ComponentRegistry &registry() const { return registry_; }

private:
  const ComponentRegistry &registry_;
  std::unordered_map<std::string, Circuit> cache_;
  std::unordered_set<std::string> in_progress_;
};

// ── ComponentCompiler ───────────────────────────────────────────────────────
/// Compiles a single component scope. Owns a SymbolTable for local names
/// and writes gate nodes into the Circuit passed at construction. Component
/// calls are resolved through the CompilationSession (pre-compiled DAGs
/// inlined by index remapping — no child compilers).

class ComponentCompiler {
public:
  ComponentCompiler(Circuit &circuit, CompilationSession &session)
      : circuit_(circuit), session_(session) {}

  /// Creates Input nodes for parameters, compiles the body, and populates
  /// circuit_.outputs from the return statement.
  void compile_top_level(const ast::Comp &comp);

private:
  Circuit &circuit_;
  CompilationSession &session_;
  SymbolTable symbols_;
  std::vector<NamedSignal> return_signals_;
  bool has_returned_ = false;

  void compile_body(const std::vector<ast::Statement> &body);

  // Statement handlers — each processes one AST statement type.
  void handle_init(const ast::InitAssign &stmt);
  void handle_mutation(const ast::MutAssign &stmt);
  void handle_comp_call(const ast::CompCall &stmt);
  void handle_return(const ast::ReturnStmt &stmt);

  /// Recursively compiles an expression into gate nodes. Returns the Signal
  /// (node index + width) for the expression's result.
  Signal compile_expr(const ast::Expr &expr);
};

// ── CompilationSession implementation ───────────────────────────────────────

const Circuit &CompilationSession::compile_or_get(const std::string &name) {
  auto cached = cache_.find(name);
  if (cached != cache_.end())
    return cached->second;

  if (in_progress_.count(name))
    throw CyclicDependencyError(name);

  in_progress_.insert(name);

  const ast::Comp &comp = registry_.lookup(name);

  Circuit circuit;
  ComponentCompiler compiler(circuit, *this);
  compiler.compile_top_level(comp);

  in_progress_.erase(name);
  return cache_.emplace(name, std::move(circuit)).first->second;
}

// ── ComponentCompiler implementation ────────────────────────────────────────

void ComponentCompiler::compile_top_level(const ast::Comp &comp) {
  for (const auto &param : comp.params) {
    Signal sig = circuit_.add_node(GateType::Input, {}, param.width);
    symbols_.define(param.ident, sig);
  }
  circuit_.num_inputs = comp.params.size();

  compile_body(comp.body);

  circuit_.outputs = std::move(return_signals_);
}

void ComponentCompiler::compile_body(
    const std::vector<ast::Statement> &body) {
  for (const auto &stmt : body) {
    if (has_returned_) break; // Don't accept anything done after a return
    // TODO: This would be a great first thing if we add warnings

    std::visit(overload{
      [this](const ast::InitAssign &s)  { handle_init(s); },
      [this](const ast::MutAssign &s)   { handle_mutation(s); },
      [this](const ast::CompCall &s)    { handle_comp_call(s); },
      [this](const ast::ReturnStmt &s)  { handle_return(s); },
    }, stmt);
  }
}

void ComponentCompiler::handle_init(const ast::InitAssign &stmt) {
  Signal sig = compile_expr(stmt.value);

  if (sig.width != stmt.target.width)
    throw WidthMismatchError("declaration of '" + stmt.target.ident + "'",
                             stmt.target.width, sig.width);

  symbols_.define(stmt.target.ident, sig);
}

void ComponentCompiler::handle_mutation(const ast::MutAssign &stmt) {
  int expected_width = symbols_.resolve(stmt.target).width;
  Signal sig = compile_expr(stmt.value);

  if (sig.width != expected_width)
    throw WidthMismatchError("assignment to '" + stmt.target + "'",
                             expected_width, sig.width);

  symbols_.update(stmt.target, sig);
}

void ComponentCompiler::handle_comp_call(const ast::CompCall &stmt) {
  std::vector<Signal> args;
  for (const auto &arg : stmt.args)
    args.push_back(symbols_.resolve(arg));

  const ast::Comp &callee_ast = session_.registry().lookup(stmt.comp);

  if (args.size() != callee_ast.params.size())
    throw ArityMismatchError("'" + stmt.comp + "' arguments",
                             callee_ast.params.size(), args.size());

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i].width != callee_ast.params[i].width)
      throw WidthMismatchError("'" + stmt.comp + "' argument '" +
                                   callee_ast.params[i].ident + "'",
                               callee_ast.params[i].width, args[i].width);
  }

  const Circuit &callee = session_.compile_or_get(stmt.comp);
  std::vector<Signal> outputs = inline_circuit(circuit_, callee, args);

  if (stmt.outputs.size() != outputs.size())
    throw ArityMismatchError("'" + stmt.comp + "' outputs",
                             outputs.size(), stmt.outputs.size());

  for (size_t i = 0; i < outputs.size(); ++i) {
    if (stmt.outputs[i].width != outputs[i].width)
      throw WidthMismatchError("'" + stmt.comp + "' output '" +
                                   stmt.outputs[i].ident + "'",
                               outputs[i].width, stmt.outputs[i].width);

    symbols_.define(stmt.outputs[i].ident, outputs[i]);
  }
}

void ComponentCompiler::handle_return(const ast::ReturnStmt &stmt) {
  for (const auto &name : stmt.names) {
    Signal sig = symbols_.resolve(name);
    return_signals_.push_back(NamedSignal{name, sig});
  }
  has_returned_ = true;
}

Signal ComponentCompiler::compile_expr(const ast::Expr &expr) {
  if (const auto *ident = std::get_if<std::string>(&expr.data)) {
    return symbols_.resolve(*ident);
  }

  if (const auto *unary = std::get_if<ast::UnaryExpr>(&expr.data)) {
    Signal operand = compile_expr(*unary->operand);
    return circuit_.add_node(GateType::Not, {operand.node}, operand.width);
  }

  if (const auto *bin = std::get_if<ast::BinExpr>(&expr.data)) {
    Signal lhs = compile_expr(*bin->lhs);
    Signal rhs = compile_expr(*bin->rhs);

    if (lhs.width != rhs.width)
      throw WidthMismatchError("binary operands", lhs.width, rhs.width);

    return circuit_.add_node(binop_to_gatetype(bin->op), {lhs.node, rhs.node}, lhs.width);
  }

  throw CompileError("unhandled expression variant");
}

} // namespace

// ── Public API ──────────────────────────────────────────────────────────────

Circuit compile_component(const std::string &name,
                          const ComponentRegistry &registry) {
  CompilationSession session(registry);
  return session.compile_or_get(name);
}

} // namespace gate
