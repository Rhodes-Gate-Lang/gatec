/**
 * File: Parser.hpp
 * Purpose: File / buffer -> AST entry point.
 */
#pragma once

#include "core/Ast.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace gate {

std::optional<ast::Program> parse_program(std::string_view source,
                                          const char *path = nullptr,
                                          std::string *error_out = nullptr);

} // namespace gate
