/**
 * File: Error.hpp
 * Purpose: Structured compilation errors with accumulation.
 */
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace gate {

struct SourceLocation {
  std::string file;
  size_t line = 0;
  size_t col = 0;
};

struct CompileError {
  enum class Kind {
    UndefinedSymbol,
    UndefinedComponent,
    DuplicateSymbol,
    WidthMismatch,
    ArityMismatch,
    InternalError,
  };

  Kind kind;
  std::string message;
  SourceLocation location;
};

class ErrorReporter {
public:
  void report(CompileError::Kind kind, const std::string &message) {
    errors_.push_back(CompileError{kind, message, {}});
  }

  void report(CompileError::Kind kind, const std::string &message, SourceLocation loc) {
    errors_.push_back(CompileError{kind, message, std::move(loc)});
  }

  bool has_errors() const { return !errors_.empty(); }

  const std::vector<CompileError> &errors() const { return errors_; }

  std::string format_all() const {
    std::string result;
    for (const auto &e : errors_) {
      if (!result.empty()) result += '\n';
      if (e.location.line > 0) {
        result += e.location.file + ":" + std::to_string(e.location.line) + ":"
                  + std::to_string(e.location.col) + ": ";
      }
      result += e.message;
    }
    return result;
  }

private:
  std::vector<CompileError> errors_;
};

} // namespace gate
