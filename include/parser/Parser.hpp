/**
 * File: Parser.hpp
 * Purpose: File / buffer -> AST entry point.
 */
#pragma once

#include "parser/Ast.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace gate {

std::optional<ast::Program> parse_program(const std::filesystem::path &path,
                                          std::string *error_out = nullptr);

} // namespace gate
