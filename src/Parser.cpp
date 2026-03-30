/**
 * File: Parser.cpp
 * Purpose: Minimal GateLang prototype — only `comp` definitions, plain identifiers (no
 * `:width`), booleans + calls + parens. See prototype.md.
 */
#include "Parser.hpp"

#include <peglib.h>

#include <any>
#include <stdexcept>
#include <utility>

namespace gate {

namespace {

using namespace peg;
using ast::Program;


void attach_actions(peg::parser &parser) {

  parser["program"] = [](const SemanticValues &vs) -> Program {
    Program program;
    for (const auto &cell : vs) {
      program.components.push_back(std::any_cast<std::string>(cell)); // will be a component type in real one
    }
    return program;
  };
}

static constexpr const char *kGrammar = R"(
# This is a comment
program      <- component*

component    <- IDENT*

IDENT        <- !KEY < [a-zA-Z_] [a-zA-Z0-9-]* >

%whitespace  <- [ \t\r\n]* ('//' (![\n] .)* [ \t\r\n]*)*
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
