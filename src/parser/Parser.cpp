/**
 * File: Parser.cpp
 * Purpose: GateLang PEG grammar (kGrammar) and peglib semantic actions.
 *
 * Parser design, precedence chain, and extension notes: src/parser/PARSER.md
 */
#include "parser/Parser.hpp"
#include "parser/Ast.hpp"

#include <peglib.h>

#include <any>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace gate {

namespace {

using namespace peg;
using ast::Program;


void attach_actions(peg::parser &pg, const std::filesystem::path &source_path) {
  /*
   * Notes for understanding how this stuff works:
   *  - Everything is a lambda that tells the parser how to deal with certain rules in the grammar
   *  - Things with multiple parts are array-based, so with IDENT ':' INT, you'd reference IDENT with vs[0] and INT with vs[1]
   */

  // ----- Helpers to translate basic types -----
  pg["IDENT"] = [](const SemanticValues &vs) -> std::string {
    return vs.token_to_string();
  };

  pg["INT"] = [](const SemanticValues &vs) -> int {
    return vs.token_to_number<int>();
  };

  // LITERAL <- < '0b' [0-1]+ >
  // token_to_string gives us the full "0b..." text; parse it as an integer.
  pg["LITERAL"] = [](const SemanticValues &vs) -> std::uint32_t {
    // skip the "0b" prefix and parse as binary
    auto s = vs.token_to_string();
    return static_cast<std::uint32_t>(std::stoul(s.substr(2), nullptr, 2));
  };


  // ----- Placing raw syntax as formatted data into the structs in include/Ast.hpp -----

  // -- Leafs --

  pg["var_init"] = [](const SemanticValues &vs) -> ast::VarInit {
    return ast::VarInit{
        std::any_cast<std::string>(vs[0]),
        static_cast<std::uint32_t>(std::any_cast<int>(vs[1])),
    };
  };

  // transform turns them all into the same type
  pg["param_list"] = [](const SemanticValues &vs) -> std::vector<ast::VarInit> {
    return vs.transform<ast::VarInit>();
  };

  const std::filesystem::path import_base_dir = source_path.parent_path();

  // import_stmt: resolve import paths relative to the current source file.
  pg["import_stmt"] = [import_base_dir](const SemanticValues &vs) -> ast::Import {
    const std::filesystem::path raw_path = vs.token_to_string();
    const std::filesystem::path resolved_path =
        raw_path.is_absolute() ? raw_path.lexically_normal()
                               : std::filesystem::absolute(import_base_dir / raw_path).lexically_normal();
    return ast::Import{resolved_path.string()};
  };

  // -- Expressions --

  // Maps the choice index of the bin_operator rule (AND/OR/XOR) to an integer
  pg["bin_operator"] = [](const SemanticValues &vs) -> int {
    return vs.choice(); // 0=AND, 1=OR, 2=XOR
  };

  // Maps the choice index of the shift_op rule (<< or >>) to a boolean flag
  pg["shift_op"] = [](const SemanticValues &vs) -> bool {
    return vs.choice() == 0; // true = left, false = right
  };

 // atom <- '(' expr ')' / merge_expr / LITERAL / IDENT
 // choice 0: parenthesized — unwrap the inner Expr
 // choice 1: merge_expr — already an Expr, pass through
 // choice 2: literal — wrap the uint32 value in an Expr
 // choice 3: identifier — wrap the string in an Expr
 pg["atom"] = [](const SemanticValues &vs) -> ast::Expr {
    switch (vs.choice()) {
    case 0:
        return std::any_cast<ast::Expr>(vs[0]);
    case 1:
        return std::any_cast<ast::Expr>(vs[0]);  // merge_expr already returns Expr
    case 2:
        return ast::Expr{ast::Literal{static_cast<std::uint64_t>(std::any_cast<std::uint32_t>(vs[0]))}};
    default:
        return ast::Expr{ast::VarRef{std::any_cast<std::string>(vs[0])}};
    }
 };

 // postfix <- atom ('[' INT ':' INT ']')?
 // vs[0]         = Expr from atom (always present)
 // vs[1], vs[2]  = INT lo, INT hi (only when the slice suffix matched)
 pg["postfix"] = [](const SemanticValues &vs) -> ast::Expr {
    auto base = std::any_cast<ast::Expr>(vs[0]);
    if (vs.size() == 1)
        return base;
    int lo = std::any_cast<int>(vs[1]);
    int hi = std::any_cast<int>(vs[2]);
    return ast::Expr{ast::SplitExpr{std::make_shared<ast::Expr>(std::move(base)), lo, hi}};
 };

  // unary <- 'NOT' unary / postfix
  // choice 0: the 'NOT' literal is invisible, vs[0] is the inner Expr.
  // choice 1: pass through postfix.
  pg["unary"] = [](const SemanticValues &vs) -> ast::Expr {
    switch (vs.choice()) {
    case 0: {
      auto inner = std::any_cast<ast::Expr>(vs[0]);
      return ast::Expr{
          ast::NotExpr{std::make_shared<ast::Expr>(std::move(inner))}};
    }
    default:
      return std::any_cast<ast::Expr>(vs[0]);
    }
  };

  // Implements left-associative parsing for shift operations.
  // vs[0] is the base expression, followed by pairs of [op, integer amount].
  pg["shift_expr"] = [](const SemanticValues &vs) -> ast::Expr {
    auto acc = std::any_cast<ast::Expr>(vs[0]);
    for (size_t i = 1; i < vs.size(); i += 2) {
        bool is_left = std::any_cast<bool>(vs[i]);
        auto amt = std::any_cast<int>(vs[i + 1]);
        auto operand = std::make_shared<ast::Expr>(std::move(acc));
        auto amount_expr = std::make_shared<ast::Expr>(ast::Expr{ast::Literal{static_cast<uint64_t>(amt)}});
        if (is_left)
            acc = ast::Expr{ast::LeftShift{std::move(operand), std::move(amount_expr)}};
        else
            acc = ast::Expr{ast::RightShift{std::move(operand), std::move(amount_expr)}};
    }
    return acc;
  };

  // merge_expr <- '{' expr (',' expr)+ '}'
  // All children are Expr values — collect them into a MergeExpr.
  pg["merge_expr"] = [](const SemanticValues &vs) -> ast::Expr {
      std::vector<std::shared_ptr<ast::Expr>> parts;
      for (const auto &v : vs)
          parts.push_back(std::make_shared<ast::Expr>(std::any_cast<ast::Expr>(v)));
      return ast::Expr{ast::MergeExpr{std::move(parts)}};
  };

  // Implements left-associative parsing for binary logic operations.
  // vs[0] is the base expression, followed by pairs of [op_index, rhs_expression].
  pg["expr"] = [](const SemanticValues &vs) -> ast::Expr {
    auto acc = std::any_cast<ast::Expr>(vs[0]);
    for (size_t i = 1; i < vs.size(); i += 2) {
        int op = std::any_cast<int>(vs[i]);
        auto rhs = std::any_cast<ast::Expr>(vs[i + 1]);
        auto lhs_ptr = std::make_shared<ast::Expr>(std::move(acc));
        auto rhs_ptr = std::make_shared<ast::Expr>(std::move(rhs));
        switch (op) {
        case 0: acc = ast::Expr{ast::AndExpr{lhs_ptr, rhs_ptr}}; break;
        case 1: acc = ast::Expr{ast::OrExpr {lhs_ptr, rhs_ptr}}; break;
        default: acc = ast::Expr{ast::XorExpr{lhs_ptr, rhs_ptr}}; break;
        }
    }
    return acc;
  };

  // -- Helpers (same pattern as param_list) --

  // Aggregates comma-separated VarInit nodes into a vector for return types.
  pg["comp_outputs"] = [](const SemanticValues &vs) -> std::vector<ast::VarInit> {
    return vs.transform<ast::VarInit>();
  };

  // Aggregates comma-separated expr values into a vector for function arguments.
  pg["arg_list"] = [](const SemanticValues &vs) -> std::vector<ast::Expr> {
    return vs.transform<ast::Expr>();
  };

  // Flattens individual statement nodes into a single vector for the block body.
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
      std::any_cast<std::vector<ast::Expr>>(vs[2])
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

  expr          <- shift_expr (bin_operator shift_expr)*
  shift_expr    <- unary (shift_op INT)*
  unary         <- 'NOT' unary / postfix
  postfix       <- atom ('[' INT ':' INT ']')?
  atom          <- '(' expr ')' / merge_expr / LITERAL / IDENT
  merge_expr    <- '{' expr (',' expr)+ '}'

  bin_operator  <- 'AND' / 'OR' / 'XOR'
  shift_op      <- '<<' / '>>'

  comp_outputs  <- var_init (',' var_init)*
  arg_list      <- (expr (',' expr)*)?
  var_init      <- IDENT ':' INT

  LITERAL       <- < '0b' [0-1]+ >
  IDENT         <- < [a-zA-Z_] [a-zA-Z0-9_-]* >
  INT           <- < [0-9]+ >

  %whitespace   <- [ \t\r\n]* ('//' (![\n] .)* [ \t\r\n]*)*
)";

std::string read_stream(std::istream &in) {
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

} // namespace

std::optional<ast::Program> parse_program(const std::filesystem::path &path,
                                          std::string *error_out) {
  std::ifstream source_file(path);
  if (!source_file) {
    if (error_out) {
      *error_out = "cannot open file: " + path.string();
    }
    return std::nullopt;
  }
  const std::string source = read_stream(source_file);

  peg::parser peg_parser(kGrammar);
  if (!peg_parser) {
    if (error_out) {
      *error_out = "failed to compile prototype grammar";
    }
    return std::nullopt;
  }
  attach_actions(peg_parser, path);

  peg_parser.set_logger(
      [error_out](std::size_t line, std::size_t col, const std::string &msg,
                  const std::string & /*rule*/) {
        if (error_out) {
          *error_out =
              std::to_string(line) + ":" + std::to_string(col) + ": " + msg;
        }
      });

  ast::Program program;
  const std::string path_string = path.string();
  if (!peg_parser.parse_n(source.data(), source.size(), program,
                          path_string.c_str())) {
    return std::nullopt;
  }
  return program;
}

} // namespace gate
