/**
 * File: Ast.hpp
 * Purpose: AST shape for GateLang — expand nested types as the grammar grows.
 */
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace gate::ast {

// ── Leaves ──────────────────────────────────────────────────────────────────

struct VarInit {
  std::string ident;
  uint32_t width;
};

// ── Expressions (recursive) ────────────────────────────────────────────────

struct Expr;

// Nullary Expressions
struct VarRef { std::string name; };
struct Literal { uint64_t bits; };

// Unary Expressions
struct NotExpr { std::shared_ptr<Expr> operand; };
struct SplitExpr { std::shared_ptr<Expr> operand; int lo; int hi; };

// Binary Expressions
struct LeftShift  { std::shared_ptr<Expr> operand; std::shared_ptr<Expr> shift_amount; };
struct RightShift { std::shared_ptr<Expr> operand; std::shared_ptr<Expr> shift_amount; };
struct AndExpr    { std::shared_ptr<Expr> lhs; std::shared_ptr<Expr> rhs; };
struct OrExpr     { std::shared_ptr<Expr> lhs; std::shared_ptr<Expr> rhs; };
struct XorExpr    { std::shared_ptr<Expr> lhs; std::shared_ptr<Expr> rhs; };

// N-Ary Expressions
struct MergeExpr  { std::vector<std::shared_ptr<Expr>> operands; };

// Actual EXPR splitter
struct Expr {
  std::variant<VarRef, Literal, NotExpr, SplitExpr, LeftShift, RightShift, AndExpr, OrExpr, XorExpr, MergeExpr> data;
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
  std::vector<Expr> args;
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
  std::filesystem::path path;
};

struct Program {
  std::vector<Import> imports;
  std::vector<Comp> components;
};

} // namespace gate::ast
