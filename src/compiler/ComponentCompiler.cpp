/**
 * File: ComponentCompiler.cpp
 * Purpose: Per-component lowering from AST to GateObject.
 */
#include "compiler/ComponentCompiler.hpp"
#include "compiler/CompilationSession.hpp"
#include "compiler/CompileError.hpp"
#include "compiler/InlineGateo.hpp"

#include <string>
#include <variant>

namespace gate {

namespace {

template <typename... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};
template <typename... Ts>
overload(Ts...) -> overload<Ts...>;

// TODO: Map ast::BinOp to gateo::v2::view::GateType for And/Or/Xor (protobuf enum in view layer).

} // namespace

ComponentCompiler::ComponentCompiler(GateObject &object,
                                     CompilationSession &session)
    : object_(object), session_(session) {}

void ComponentCompiler::compile_top_level(const ast::Comp &comp) {
  (void)object_;
  (void)session_;
  (void)comp;
  // TODO: Ensure object_.components has root entry (index 0) with comp.name and parent 0.
  // TODO: For each param: append Input node, define in symbols_ with GateNodeRef.
  // TODO: compile_body(comp.body).
  // TODO: Wire return / Output nodes per schema (boundary renamers).
}

void ComponentCompiler::compile_body(const std::vector<ast::Statement> &body) {
  for (const auto &stmt : body) {
    if (has_returned_) break;

    std::visit(
        overload{
            [this](const ast::InitAssign &s) { handle_init(s); },
            [this](const ast::MutAssign &s) { handle_mutation(s); },
            [this](const ast::CompCall &s) { handle_comp_call(s); },
            [this](const ast::ReturnStmt &s) { handle_return(s); },
        },
        stmt);
  }
}

void ComponentCompiler::handle_init(const ast::InitAssign &stmt) {
  (void)stmt;
  // TODO: compile_expr, width check, symbols_.define.
}

void ComponentCompiler::handle_mutation(const ast::MutAssign &stmt) {
  (void)stmt;
  // TODO: resolve, compile_expr, width check, symbols_.update.
}

void ComponentCompiler::handle_comp_call(const ast::CompCall &stmt) {
  (void)stmt;
  // TODO: Resolve args; arity/width checks vs callee AST.
  // TODO: session_.compile_or_get(stmt.comp) for callee GateObject.
  // TODO: Allocate caller-side instance row for this call (components push + parent link).
  // TODO: inline_gateo_object(object_, callee, caller_instance_index, arg_nodes).
  // TODO: Bind stmt.outputs to inline result output_node_indices via GateNodeRef.
}

void ComponentCompiler::handle_return(const ast::ReturnStmt &stmt) {
  (void)stmt;
  // TODO: Resolve signals; record as named outputs / Output nodes; set has_returned_.
}

GateNodeRef ComponentCompiler::compile_expr(const ast::Expr &expr) {
  (void)expr;
  // TODO: Ident → resolve; Unary → Not node; Binary → And/Or/Xor; append to object_.nodes.
  return {};
}

} // namespace gate
