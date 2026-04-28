/**
 * File: ProgramLoader.hpp
 * Purpose: Provide methods to load a program and its imports into a single AST for gatec.cpp
 */
#pragma once

#include "parser/Ast.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace gate {

std::optional<ast::Program>
load_program(const std::filesystem::path &entry, std::string *error_out = nullptr);

bool resolve_imports(ast::Program &program, std::string *error_out = nullptr);

} // namespace gate
