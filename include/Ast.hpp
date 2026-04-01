/**
 * File: Ast.hpp
 * Purpose: AST shape for GateLang — expand nested types as the grammar grows.
 */
#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace gate::ast {

// ── Leaves ──────────────────────────────────────────────────────────────────

struct VarInit {
  std::string ident;
  int width;
};

enum class BinOp { And, Or, Xor };

// ── Expressions (recursive) ────────────────────────────────────────────────

struct Expr;

struct UnaryExpr {
  std::shared_ptr<Expr> operand;
};

struct BinExpr {
  std::shared_ptr<Expr> lhs;
  BinOp op;
  std::shared_ptr<Expr> rhs;
};

struct Expr {
  std::variant<std::string, UnaryExpr, BinExpr> data;
};

// ── Assignments ─────────────────────────────────────────────────────────────

struct InitAssign {
  VarInit target;
  Expr value;
};

struct MutAssign {
  std::string target;
  Expr value;
};

struct CompCall {
  std::vector<VarInit> outputs;
  std::string comp;
  std::vector<std::string> args;
};

// ── Statements ──────────────────────────────────────────────────────────────

struct ReturnStmt {
  std::vector<std::string> names;
};

using Statement = std::variant<InitAssign, MutAssign, CompCall, ReturnStmt>;

// ── Top-level ───────────────────────────────────────────────────────────────

struct Comp {
  std::string name;
  std::vector<VarInit> params;
  std::vector<Statement> body;
};

struct Import {
  std::string path;
};

struct Program {
  std::vector<Import> imports;
  std::vector<Comp> components;
};

} // namespace gate::ast
