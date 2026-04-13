/**
 * File: CompileError.hpp
 * Purpose: Exception hierarchy for compilation errors.
 *
 * Each error type carries structured data (names, widths, counts) and formats
 * its own human-readable message. Call sites throw with parameters only —
 * no string construction at the throw site.
 *
 * Catch CompileError at the pipeline boundary (REPL, CLI, tests) to handle
 * all compilation failures uniformly.
 */
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

namespace gate {

struct SourceLocation {
  std::string file;
  size_t line = 0;
  size_t col = 0;
}; // Probably should move this to include/core/ once implemented in the AST structs

// ── Base ──────────────────────────────────────────────────────────────────

class CompileError : public std::runtime_error {
public:
  explicit CompileError(const std::string &message, SourceLocation loc = {})
      : std::runtime_error(message), location(std::move(loc)) {}

  SourceLocation location;
};

// ── Symbol errors ─────────────────────────────────────────────────────────

class UndefinedSymbolError : public CompileError {
public:
  explicit UndefinedSymbolError(std::string name, SourceLocation loc = {})
      : CompileError("'" + name + "' is not defined", std::move(loc)),
        symbol_name(std::move(name)) {}

  std::string symbol_name;
};

class DuplicateSymbolError : public CompileError {
public:
  explicit DuplicateSymbolError(std::string name, SourceLocation loc = {})
      : CompileError("'" + name + "' is already defined", std::move(loc)),
        symbol_name(std::move(name)) {}

  std::string symbol_name;
};

// ── Component errors ──────────────────────────────────────────────────────

class UndefinedComponentError : public CompileError {
public:
  explicit UndefinedComponentError(std::string name, SourceLocation loc = {})
      : CompileError("unknown component '" + name + "'", std::move(loc)),
        component_name(std::move(name)) {}

  std::string component_name;
};

class CyclicDependencyError : public CompileError {
public:
  explicit CyclicDependencyError(std::string name, SourceLocation loc = {})
      : CompileError("cyclic dependency detected involving component '" +
                         name + "'",
                     std::move(loc)),
        component_name(std::move(name)) {}

  std::string component_name;
};

// ── Width / arity errors ──────────────────────────────────────────────────

/// Context string describes what was being checked, e.g. "'x' declared",
/// "argument 'y'", "binary expression". The error formats the rest.
class WidthMismatchError : public CompileError {
public:
  WidthMismatchError(std::string context, int expected, int actual,
                     SourceLocation loc = {})
      : CompileError(context + ": expected width " + std::to_string(expected) +
                         " but got " + std::to_string(actual),
                     std::move(loc)),
        context_name(std::move(context)),
        expected_width(expected),
        actual_width(actual) {}

  std::string context_name;
  int expected_width;
  int actual_width;
};

/// Context string describes what was being counted, e.g.
/// "component 'Adder' arguments", "component 'Adder' outputs".
class ArityMismatchError : public CompileError {
public:
  ArityMismatchError(std::string context, size_t expected, size_t actual,
                     SourceLocation loc = {})
      : CompileError(context + ": expected " + std::to_string(expected) +
                         " but got " + std::to_string(actual),
                     std::move(loc)),
        context_name(std::move(context)),
        expected_count(expected),
        actual_count(actual) {}

  std::string context_name;
  size_t expected_count;
  size_t actual_count;
};

} // namespace gate
