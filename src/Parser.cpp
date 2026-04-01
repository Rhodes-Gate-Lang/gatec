/**
 * File: Parser.cpp
 * Purpose: Minimal parser skeleton — tokenize identifiers with whitespace/comment skipping.
 */
#include "Parser.hpp"
#include "Ast.hpp"

#include <peglib.h>

#include <any>

namespace gate {

namespace {

using namespace peg;
using ast::Program;


void attach_actions(peg::parser &pg) {
  /*
   * Notes for understanding how this stuff works:
   *  - Everything is a lambda that tells the parser how to deal with certain rules in the grammar
   *  - Things with multiple parts are array based, so with IDENT ':' INT, you'd reference IDENT with vs[0] and INT with vs[1]
   *  - 
   */

  // ----- Helpers to translate basic types -----
  pg["IDENT"] = [](const SemanticValues &vs) -> std::string {
    return vs.token_to_string();
  };

  pg["INT"] = [](const SemanticValues &vs) -> int {
    return vs.token_to_number<int>();
  };


  // ----- Placing raw syntax as formatted data into the structs in include/Ast.hpp -----

  // -- Leafs --

  pg["var_init"] = [](const SemanticValues &vs) -> ast::VarInit {
    return ast::VarInit{
        std::any_cast<std::string>(vs[0]),
        std::any_cast<int>(vs[1]),
    };
  };

  // transform turns them all into the same type
  pg["param_list"] = [](const SemanticValues &vs) -> std::vector<ast::VarInit> {
    return vs.transform<ast::VarInit>();
  };

  // import_stmt: the < [^"]+ > capture gives us the filename via token_to_string
  pg["import_stmt"] = [](const SemanticValues &vs) -> ast::Import {
    return ast::Import{vs.token_to_string()};
  };

  // -- Expressions --

  // bin_operator <- 'AND' / 'OR' / 'XOR'
  // vs.choice() returns which alternative matched (0, 1, or 2).
  pg["bin_operator"] = [](const SemanticValues &vs) -> ast::BinOp {
    switch (vs.choice()) {
    case 0: return ast::BinOp::And;
    case 1: return ast::BinOp::Or;
    default: return ast::BinOp::Xor;
    }
  };

  // atom <- '(' expr ')' / IDENT
  // choice 0: parenthesized expr — just unwrap and pass through.
  // choice 1: bare identifier — wrap the string in an Expr.
  pg["atom"] = [](const SemanticValues &vs) -> ast::Expr {
    switch (vs.choice()) {
    case 0:
      return std::any_cast<ast::Expr>(vs[0]);
    default:
      return ast::Expr{std::any_cast<std::string>(vs[0])};
    }
  };

  // unary <- 'NOT' unary / atom
  // choice 0: the 'NOT' literal is invisible, vs[0] is the inner Expr.
  // choice 1: pass through the atom.
  pg["unary"] = [](const SemanticValues &vs) -> ast::Expr {
    switch (vs.choice()) {
    case 0: {
      auto inner = std::any_cast<ast::Expr>(vs[0]);
      return ast::Expr{
          ast::UnaryExpr{std::make_shared<ast::Expr>(std::move(inner))}};
    }
    default:
      return std::any_cast<ast::Expr>(vs[0]);
    }
  };

  // expr <- unary (bin_operator unary)*
  //
  // Children arrive flat and alternating:
  //   vs[0]=Expr, vs[1]=BinOp, vs[2]=Expr, vs[3]=BinOp, vs[4]=Expr, ...
  //
  // We left-fold them into a tree: for "a AND b OR c",
  //   step 0: acc = Expr("a")
  //   step 1: acc = BinExpr(acc, AND, Expr("b"))
  //   step 2: acc = BinExpr(acc, OR,  Expr("c"))
  pg["expr"] = [](const SemanticValues &vs) -> ast::Expr {
    auto acc = std::any_cast<ast::Expr>(vs[0]);

    for (size_t i = 1; i < vs.size(); i += 2) {
      auto op = std::any_cast<ast::BinOp>(vs[i]);
      auto rhs = std::any_cast<ast::Expr>(vs[i + 1]);

      acc = ast::Expr{ast::BinExpr{
          std::make_shared<ast::Expr>(std::move(acc)),
          op,
          std::make_shared<ast::Expr>(std::move(rhs)),
      }};
    }

    return acc;
  };

  // -- Helpers (same pattern as param_list) --

  pg["comp_outputs"] = [](const SemanticValues &vs) -> std::vector<ast::VarInit> {
    return vs.transform<ast::VarInit>();
  };

  pg["arg_list"] = [](const SemanticValues &vs) -> std::vector<std::string> {
    return vs.transform<std::string>();
  };

  pg["body"] = [](const SemanticValues &vs) -> std::vector<ast::Statement> {
    return vs.transform<ast::Statement>();
  };

  // -- Statement-level actions --
  // Each action receives vs[0] as the single child from the matched rule.

  // statement    <- comp_call / init / mutation / return_stmt
  pg["statement"] = [](const SemanticValues &vs) -> ast::Statement {
    switch (vs.choice()) {
    case 0:
      return std::any_cast<ast::CompCall>(vs[0]);
    case 1:
      return std::any_cast<ast::InitAssign>(vs[0]);
    case 2:
      return std::any_cast<ast::MutAssign>(vs[0]);
    default:
      return std::any_cast<ast::ReturnStmt>(vs[0]);
    }
  };

  // init         <- var_init '=' expr ';'
  //   vs[0] = VarInit, vs[1] = Expr → return ast::InitAssign
  pg["init"] = [](const SemanticValues &vs) -> ast::InitAssign {
    return ast::InitAssign {
      std::any_cast<ast::VarInit>(vs[0]),
      std::any_cast<ast::Expr>(vs[1])
    };
  };

  // mutation     <- IDENT '=' expr ';'
  //   vs[0] = string, vs[1] = Expr → return ast::MutAssign
  pg["mutation"] = [](const SemanticValues &vs) -> ast::MutAssign {
    return ast::MutAssign {
      std::any_cast<std::string>(vs[0]),
      std::any_cast<ast::Expr>(vs[1])
    };
  };

  //
  // comp_call    <- comp_outputs '=' IDENT '(' arg_list ')' ';'
  //   vs[0] = vector<VarInit>, vs[1] = string, vs[2] = vector<string>
  //   → return ast::CompCall
  pg["comp_call"] = [](const SemanticValues &vs) -> ast::CompCall {
    return ast::CompCall {
      std::any_cast<std::vector<ast::VarInit>>(vs[0]),
      std::any_cast<std::string>(vs[1]),
      std::any_cast<std::vector<std::string>>(vs[2])
    };
  };

  // return_stmt  <- 'return' IDENT (',' IDENT)* ';'
  //   All children are IDENT strings → vs.transform<string>()
  //   → return ast::ReturnStmt
  pg["return_stmt"] = [](const SemanticValues &vs) -> ast::ReturnStmt {
    return ast::ReturnStmt {
      vs.transform<std::string>()
    };
  };

  // -- Top-level actions --

  // comp <- 'comp' IDENT '(' param_list ')' '{' body '}'
  //   vs[0] = string (name), vs[1] = vector<VarInit>, vs[2] = vector<Statement>
  pg["comp"] = [](const SemanticValues &vs) -> ast::Comp {
    return ast::Comp {
      std::any_cast<std::string>(vs[0]),
      std::any_cast<std::vector<ast::VarInit>>(vs[1]),
      std::any_cast<std::vector<ast::Statement>>(vs[2])
    };
  };

  // program <- import_stmt* comp*
  //   Heterogeneous children — use the any_cast pointer probe pattern.
  pg["program"] = [](const SemanticValues &vs) -> ast::Program {
    ast::Program prog;
    for (const auto &child : vs) {
      if (auto *imp = std::any_cast<ast::Import>(&child))
        prog.imports.push_back(*imp);
      else if (auto *comp = std::any_cast<ast::Comp>(&child))
        prog.components.push_back(*comp);
    }
    return prog;
  };
}

static constexpr const char *kGrammar = R"(
  program       <- import_stmt* comp*

  import_stmt   <- 'import' '"' < [^"]+ > '"'

  comp          <- 'comp' IDENT '(' param_list ')' '{' body '}'
  body          <- statement*
  param_list    <- (var_init (',' var_init)*)?

  statement     <- comp_call / init / mutation / return_stmt
  comp_call     <- comp_outputs '=' IDENT '(' arg_list ')' ';'
  init          <- var_init '=' expr ';'
  mutation      <- IDENT '=' expr ';'
  return_stmt   <- 'return' IDENT (',' IDENT)* ';'

  expr          <- unary (bin_operator unary)*
  unary         <- 'NOT' unary / atom
  atom          <- '(' expr ')' / IDENT
  bin_operator  <- 'AND' / 'OR' / 'XOR'

  comp_outputs  <- var_init (',' var_init)*
  arg_list      <- (IDENT (',' IDENT)*)?
  var_init      <- IDENT ':' INT

  IDENT         <- < [a-zA-Z_] [a-zA-Z0-9_-]* >
  INT           <- < [0-9]+ >

  %whitespace   <- [ \t\r\n]* ('//' (![\n] .)* [ \t\r\n]*)*
)";

} // namespace

std::optional<ast::Program> parse_program(std::string_view source,
                                          const char *path,
                                          std::string *error_out) {
  peg::parser peg_parser(kGrammar);
  if (!peg_parser) {
    if (error_out) {
      *error_out = "failed to compile prototype grammar";
    }
    return std::nullopt;
  }

  attach_actions(peg_parser);

  peg_parser.set_logger(
      [error_out](std::size_t line, std::size_t col, const std::string &msg,
                  const std::string & /*rule*/) {
        if (error_out) {
          *error_out =
              std::to_string(line) + ":" + std::to_string(col) + ": " + msg;
        }
      });

  ast::Program program;
  if (!peg_parser.parse_n(source.data(), source.size(), program, path)) {
    return std::nullopt;
  }
  return program;
}

} // namespace gate
