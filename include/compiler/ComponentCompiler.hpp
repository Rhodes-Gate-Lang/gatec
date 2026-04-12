/**
 * File: ComponentCompiler.hpp
 * Purpose: Lower one ast::Comp into a GateObject (full compiled artifact).
 */
#pragma once

#include "compiler/GateoAliases.hpp"
#include "compiler/SymbolTable.hpp"
#include "core/Ast.hpp"

#include <string>
#include <vector>

namespace gate {

class CompilationSession;

class ComponentCompiler {
public:
  ComponentCompiler(GateObject &object, CompilationSession &session);

  /// // TODO: Create Input nodes (named) for each parameter, set component root metadata.
  /// // TODO: Compile body; populate return boundary via Output nodes (or agreed convention).
  void compile_top_level(const ast::Comp &comp);

private:
  GateObject &object_;
  CompilationSession &session_;
  SymbolTable symbols_;
  // TODO: Track return Output nodes / names; gate after first return like has_returned_.
  // TODO: Current instance index for Node::parent while lowering (root = 0 until nested calls).
  bool has_returned_{false};

  void compile_body(const std::vector<ast::Statement> &body);

  void handle_init(const ast::InitAssign &stmt);
  void handle_mutation(const ast::MutAssign &stmt);
  void handle_comp_call(const ast::CompCall &stmt);
  void handle_return(const ast::ReturnStmt &stmt);

  /// // TODO: Lower expression to new or existing nodes; map gate::GateType to gateo::v2::view::GateType (see gateo view).
  [[nodiscard]] GateNodeRef compile_expr(const ast::Expr &expr);
};

} // namespace gate
