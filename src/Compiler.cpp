/**
 * File: Compiler.cpp
 * Purpose: AST → Circuit compilation with semantic validation.
 *
 * The internal ComponentCompiler class handles one scope (one component body).
 * Inlining a CompCall creates a child ComponentCompiler with a fresh SymbolTable
 * but the same shared Circuit, registry, and error reporter.
 */
#include "Compiler.hpp"
#include "Ast.hpp"
#include "Circuit.hpp"
#include "Error.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace gate {

namespace {

// ── Helpers ────────────────────────────────────────────────────────────────

/// Variadic string builder — streams all arguments into one string,
/// avoiding manual std::to_string() + concatenation noise.
template <typename... Args>
std::string concat(Args &&...args) {
  std::ostringstream ss;
  (ss << ... << std::forward<Args>(args));
  return ss.str();
}

GateType binop_to_gatetype(ast::BinOp op) {
  switch (op) {
  case ast::BinOp::And: return GateType::And;
  case ast::BinOp::Or:  return GateType::Or;
  case ast::BinOp::Xor: return GateType::Xor;
  }
  return GateType::And;
}

// ── ComponentCompiler ──────────────────────────────────────────────────────
//
// Compiles one component scope. Each instance owns a SymbolTable for its scope
// and shares a Circuit, ComponentRegistry, and ErrorReporter with its caller.
// Inlining a CompCall creates a child instance with a fresh SymbolTable.

class ComponentCompiler {
  using Kind = CompileError::Kind;

public:
  ComponentCompiler(Circuit &circuit, const ComponentRegistry &registry,
                    ErrorReporter &errors)
      : circuit_(circuit), registry_(registry), errors_(errors) {}

  // ── compile_top_level ──────────────────────────────────────────────────
  /// Creates Input nodes for each parameter, compiles the body, and
  /// populates circuit_.outputs from the return statement.
  bool compile_top_level(const ast::Comp &comp) {
    for (const auto &param : comp.params) {
      Signal sig = circuit_.add_node(GateType::Input, {}, param.width);
      symbols_[param.ident] = sig;
    }
    circuit_.num_inputs = comp.params.size();

    if (!compile_body(comp.body)) return false;

    for (size_t i = 0; i < return_signals_.size(); ++i) {
      circuit_.outputs.push_back(NamedSignal{return_names_[i], return_signals_[i]});
    }
    return true;
  }

  // ── compile_inline ─────────────────────────────────────────────────────
  /// Binds provided argument signals to formal parameters, compiles the
  /// body, and returns the output signals to the caller for binding.
  std::optional<std::vector<Signal>>
  compile_inline(const ast::Comp &comp, const std::vector<Signal> &args) {
    if (args.size() != comp.params.size()) {
      errors_.report(Kind::ArityMismatch,
          concat("component '", comp.name, "' expects ", comp.params.size(),
                 " arguments but got ", args.size()));
      return std::nullopt;
    }

    for (size_t i = 0; i < comp.params.size(); ++i) {
      if (args[i].width != comp.params[i].width) {
        errors_.report(Kind::WidthMismatch,
            concat("argument '", comp.params[i].ident, "' expects width ",
                   comp.params[i].width, " but got width ", args[i].width));
        return std::nullopt;
      }
      symbols_[comp.params[i].ident] = args[i];
    }

    if (!compile_body(comp.body)) return std::nullopt;

    return return_signals_;
  }

private:
  Circuit &circuit_;
  const ComponentRegistry &registry_;
  ErrorReporter &errors_;
  SymbolTable symbols_;
  std::vector<Signal> return_signals_;
  std::vector<std::string> return_names_;

  // ── compile_body ───────────────────────────────────────────────────────
  /// Walks the statement list, dispatching each to its handler.
  bool compile_body(const std::vector<ast::Statement> &body) {
    for (const auto &stmt : body) {
      // TODO: Reject statements after ReturnStmt (return_signals_ non-empty).
      bool ok = std::visit(
          [this](const auto &s) -> bool {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, ast::InitAssign>) return handle_init(s);
            else if constexpr (std::is_same_v<T, ast::MutAssign>) return handle_mutation(s);
            else if constexpr (std::is_same_v<T, ast::CompCall>) return handle_comp_call(s);
            else if constexpr (std::is_same_v<T, ast::ReturnStmt>) return handle_return(s);
            else return false;
          },
          stmt);
      if (!ok) return false;
    }
    return true;
  }

  // ── handle_init ────────────────────────────────────────────────────────
  /// Compiles an InitAssign: rejects duplicates, compiles the RHS expression,
  /// validates width, and inserts into the symbol table.
  bool handle_init(const ast::InitAssign &stmt) {
    if (symbols_.count(stmt.target.ident)) {
      errors_.report(Kind::DuplicateSymbol, concat("'", stmt.target.ident, "' is already defined"));
      return false;
    }

    auto sig = compile_expr(stmt.value);
    if (!sig) return false;

    if (sig->width != stmt.target.width) {
      errors_.report(Kind::WidthMismatch,
          concat("'", stmt.target.ident, "' declared as width ", stmt.target.width,
                 " but expression has width ", sig->width));
      return false;
    }

    symbols_[stmt.target.ident] = *sig;
    return true;
  }

  // ── handle_mutation ────────────────────────────────────────────────────
  /// Compiles a MutAssign: looks up the existing symbol, compiles the RHS,
  /// validates width matches, and updates the symbol table entry.
  bool handle_mutation(const ast::MutAssign &stmt) {
    auto target_sym = symbols_.find(stmt.target);
    if (target_sym == symbols_.end()) {
      errors_.report(Kind::UndefinedSymbol, concat("'", stmt.target, "' is not defined"));
      return false;
    }

    int target_width = target_sym->second.width;
    auto sig = compile_expr(stmt.value);
    if (!sig) return false;

    if (sig->width != target_width) {
      errors_.report(Kind::WidthMismatch,
          concat("'", stmt.target, "' has width ", target_width,
                 " but expression has width ", sig->width));
      return false;
    }

    symbols_[stmt.target] = *sig;
    return true;
  }

  // ── handle_comp_call ───────────────────────────────────────────────────
  /// Compiles a CompCall: resolves the callee and argument signals, inlines the
  /// callee's body via a child ComponentCompiler, and binds the returned output
  /// signals into the caller's symbol table.
  bool handle_comp_call(const ast::CompCall &stmt) {
    auto callee_entry = registry_.find(stmt.comp);
    if (callee_entry == registry_.end()) {
      errors_.report(Kind::UndefinedComponent, concat("unknown component '", stmt.comp, "'"));
      return false;
    }

    std::vector<Signal> arg_signals;
    for (const auto &arg_name : stmt.args) {
      auto arg_sym = symbols_.find(arg_name);
      if (arg_sym == symbols_.end()) {
        errors_.report(Kind::UndefinedSymbol, concat("'", arg_name, "' is not defined"));
        return false;
      }
      arg_signals.push_back(arg_sym->second);
    }

    // TODO: Detect recursive/cyclic component calls (e.g. track a
    // "currently compiling" set on the registry or compilation context).
    ComponentCompiler child(circuit_, registry_, errors_);
    auto callee_outputs = child.compile_inline(callee_entry->second, arg_signals);
    if (!callee_outputs) return false;

    if (callee_outputs->size() != stmt.outputs.size()) {
      errors_.report(Kind::ArityMismatch,
          concat("component '", stmt.comp, "' returns ", callee_outputs->size(),
                 " values but ", stmt.outputs.size(), " outputs declared"));
      return false;
    }

    for (size_t i = 0; i < stmt.outputs.size(); ++i) {
      const auto &out_decl = stmt.outputs[i];
      Signal out_sig = (*callee_outputs)[i];

      if (out_sig.width != out_decl.width) {
        errors_.report(Kind::WidthMismatch,
            concat("output '", out_decl.ident, "' declared as width ",
                   out_decl.width, " but component returns width ", out_sig.width));
        return false;
      }
      if (symbols_.count(out_decl.ident)) {
        errors_.report(Kind::DuplicateSymbol, concat("'", out_decl.ident, "' is already defined"));
        return false;
      }

      symbols_[out_decl.ident] = out_sig;
    }

    return true;
  }

  // ── handle_return ──────────────────────────────────────────────────────
  /// Collects the named signals from the return statement. compile_top_level
  /// converts these to NamedSignals in circuit_.outputs; compile_inline
  /// returns the signals directly to the caller.
  bool handle_return(const ast::ReturnStmt &stmt) {
    for (const auto &name : stmt.names) {
      auto name_sym = symbols_.find(name);
      if (name_sym == symbols_.end()) {
        errors_.report(Kind::UndefinedSymbol, concat("'", name, "' is not defined"));
        return false;
      }
      return_signals_.push_back(name_sym->second);
      return_names_.push_back(name);
    }
    return true;
  }

  // ── compile_expr ───────────────────────────────────────────────────────
  /// Recursively compiles an expression into the Circuit. Returns the
  /// Signal (node index + width) for the expression's result.
  std::optional<Signal> compile_expr(const ast::Expr &expr) {
    if (const auto *ident = std::get_if<std::string>(&expr.data)) {
      auto ident_sym = symbols_.find(*ident);
      if (ident_sym == symbols_.end()) {
        errors_.report(Kind::UndefinedSymbol, concat("'", *ident, "' is not defined"));
        return std::nullopt;
      }
      return ident_sym->second;
    }

    if (const auto *unary = std::get_if<ast::UnaryExpr>(&expr.data)) {
      auto operand = compile_expr(*unary->operand);
      if (!operand) return std::nullopt;
      return circuit_.add_node(GateType::Not, {operand->node}, operand->width);
    }

    if (const auto *bin = std::get_if<ast::BinExpr>(&expr.data)) {
      auto lhs = compile_expr(*bin->lhs);
      auto rhs = compile_expr(*bin->rhs);
      if (!lhs || !rhs) return std::nullopt;

      if (lhs->width != rhs->width) {
        errors_.report(Kind::WidthMismatch,
            concat("binary operands have different widths: ", lhs->width, " vs ", rhs->width));
        return std::nullopt;
      }

      return circuit_.add_node(binop_to_gatetype(bin->op), {lhs->node, rhs->node}, lhs->width);
    }

    errors_.report(Kind::InternalError, "unhandled expression variant in compile_expr");
    return std::nullopt;
  }
};

} // namespace

// ── build_registry ──────────────────────────────────────────────────────────
/// Creates a lookup table from component name to its AST definition.
ComponentRegistry build_registry(const ast::Program &program) {
  ComponentRegistry reg;
  for (const auto &comp : program.components) {
    reg[comp.name] = comp;
  }
  return reg;
}

// ── compile_component ───────────────────────────────────────────────────────
/// Public entry point: looks up the named component in the registry, creates a
/// fresh Circuit, and compiles the component into it.
std::optional<Circuit> compile_component(const std::string &name,
                                          const ComponentRegistry &registry,
                                          ErrorReporter &errors) {
  auto comp_entry = registry.find(name);
  if (comp_entry == registry.end()) {
    errors.report(CompileError::Kind::UndefinedComponent, "undefined component: " + name);
    return std::nullopt;
  }

  Circuit circuit;
  ComponentCompiler compiler(circuit, registry, errors);
  if (!compiler.compile_top_level(comp_entry->second)) return std::nullopt;
  return circuit;
}

} // namespace gate
