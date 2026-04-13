/**
 * File: Repl.cpp
 * Purpose: Interactive REPL — command loop, input parsing, and run execution.
 *
 * Input value parsing is extensible: to add a new format (hex, signed, float),
 * add a try_parse_* function and a call to it in parse_value().
 */
#include "ui/Repl.hpp"
#include "compiler/CompileError.hpp"
#include "compiler/Compiler.hpp"
#include "simulation/Simulation.hpp"
#include "ui/OutputFormatter.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace gate {

namespace {

// ── String Utilities ────────────────────────────────────────────────────────

std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t");
  return s.substr(start, end - start + 1);
}

// ── Input Value Parsing ─────────────────────────────────────────────────────
//
// Each format is a try_parse_* function returning optional<ParsedValue>.
// parse_value() tries them in priority order. To add a new format:
//   1. Write a try_parse_* function.
//   2. Add it to the chain in parse_value().
//   3. If the format implies a width, set parsed.width (used for validation).

struct ParsedValue {
  uint64_t bits;
  int width; // Width inferred from the format, or -1 if unspecified.
};

/// Binary literal: "0b1010" → bits=10, width=4.
/// Width is the number of digits (must match parameter width exactly).
std::optional<ParsedValue> try_parse_binary(const std::string &token) {
  if (token.size() < 3 || token[0] != '0' || token[1] != 'b') return std::nullopt;

  uint64_t value = 0;
  int digit_count = 0;
  for (size_t i = 2; i < token.size(); ++i) {
    char c = token[i];
    if (c != '0' && c != '1') return std::nullopt;
    value = (value << 1) | static_cast<uint64_t>(c - '0');
    ++digit_count;
  }
  if (digit_count == 0) return std::nullopt;

  return ParsedValue{value, digit_count};
}

/// Unsigned decimal integer: "42" → bits=42, width=-1.
/// No width is inferred; validation checks against the parameter's width.
std::optional<ParsedValue> try_parse_integer(const std::string &token) {
  if (token.empty()) return std::nullopt;
  for (char c : token) {
    if (c < '0' || c > '9') return std::nullopt;
  }
  try {
    return ParsedValue{std::stoull(token), -1};
  } catch (...) {
    return std::nullopt;
  }
}

// ── Add new try_parse_* functions above this line ───────────────────────────
// Future formats: try_parse_hex ("0xFF"), try_parse_signed ("s:-3"),
//                 try_parse_float ("f:3.14"), etc.

/// Tries all input formats in priority order. First match wins.
std::optional<ParsedValue> parse_value(const std::string &token) {
  if (auto v = try_parse_binary(token)) return v;
  if (auto v = try_parse_integer(token)) return v;
  return std::nullopt;
}

/// Validates a parsed value against a parameter's declared width.
/// Returns an error message on failure, or an empty string on success.
std::string validate_input(const ParsedValue &val, const std::string &param_name, int param_width) {
  if (val.width >= 0 && val.width != param_width) {
    std::ostringstream ss;
    ss << "binary literal for '" << param_name << "' has " << val.width
       << " bits but parameter expects width " << param_width;
    return ss.str();
  }

  uint64_t max_val = (param_width >= 64) ? UINT64_MAX : (1ULL << param_width) - 1;
  if (val.bits > max_val) {
    std::ostringstream ss;
    ss << "value " << val.bits << " for '" << param_name
       << "' does not fit in " << param_width << " bits";
    return ss.str();
  }

  return {};
}

// ── Run Command Parsing ─────────────────────────────────────────────────────

struct RunCommand {
  std::string component;
  std::vector<std::string> args;
};

/// Parses "ComponentName(arg1, arg2, ...)" from the text after "run ".
std::optional<RunCommand> parse_run_args(const std::string &text) {
  std::string trimmed = trim(text);

  size_t paren_open = trimmed.find('(');
  if (paren_open == std::string::npos) return std::nullopt;

  size_t paren_close = trimmed.rfind(')');
  if (paren_close == std::string::npos || paren_close <= paren_open) return std::nullopt;

  RunCommand cmd;
  cmd.component = trim(trimmed.substr(0, paren_open));
  if (cmd.component.empty()) return std::nullopt;

  std::string arg_str = trimmed.substr(paren_open + 1, paren_close - paren_open - 1);
  std::istringstream arg_stream(arg_str);
  std::string arg_token;
  while (std::getline(arg_stream, arg_token, ',')) {
    std::string trimmed_arg = trim(arg_token);
    if (!trimmed_arg.empty()) cmd.args.push_back(trimmed_arg);
  }

  return cmd;
}

} // namespace

// ── Repl ────────────────────────────────────────────────────────────────────

Repl::Repl(const ast::Program &program)
    : registry_(program) {}

/// Main loop: reads lines, dispatches commands.
void Repl::run() {
  std::cout << "gate-lang repl — type 'help' for commands\n";

  std::string line;
  while (true) {
    std::cout << "> " << std::flush;
    if (!std::getline(std::cin, line)) break;

    std::string trimmed = trim(line);
    if (trimmed.empty()) continue;

    std::string cmd_word = trimmed;
    std::string cmd_args;
    size_t space_pos = trimmed.find_first_of(" \t");
    if (space_pos != std::string::npos) {
      cmd_word = trimmed.substr(0, space_pos);
      cmd_args = trim(trimmed.substr(space_pos + 1));
    }

    if (cmd_word == "exit" || cmd_word == "quit") break;
    if (cmd_word == "help") { handle_help(); continue; }
    if (cmd_word == "list") { handle_list(); continue; }
    if (cmd_word == "run")  { handle_run(cmd_args); continue; }

    std::cerr << "unknown command: '" << cmd_word << "' — type 'help' for commands\n";
  }
}

// ── handle_run ──────────────────────────────────────────────────────────────
/// Compiles (or finds cached) the named component, parses and validates the
/// input arguments, simulates, and displays formatted results.
void Repl::handle_run(const std::string &args) {
  auto cmd = parse_run_args(args);
  if (!cmd) {
    std::cerr << "usage: run ComponentName(arg1, arg2, ...)\n";
    return;
  }

  // Compile and cache on first use.
  auto cache_hit = cache_.find(cmd->component);
  if (cache_hit == cache_.end()) {
    try {
      Circuit circuit = compile_component(cmd->component, registry_);
      cache_hit = cache_.emplace(cmd->component, prepare(std::move(circuit))).first;
    } catch (const CompileError &e) {
      std::cerr << "compile error: " << e.what() << "\n";
      return;
    }
  }

  const PreparedCircuit &pc = cache_hit->second;
  const auto &params = registry_.lookup(cmd->component).params;

  // Validate argument count.
  if (cmd->args.size() != params.size()) {
    std::cerr << "error: " << cmd->component << " expects " << params.size()
              << " arguments but got " << cmd->args.size() << "\n";
    return;
  }

  // Parse and validate each input value.
  std::vector<uint64_t> input_values;
  input_values.reserve(params.size());
  for (size_t i = 0; i < cmd->args.size(); ++i) {
    auto parsed = parse_value(cmd->args[i]);
    if (!parsed) {
      std::cerr << "error: cannot parse '" << cmd->args[i] << "' as a value\n";
      return;
    }
    std::string error = validate_input(*parsed, params[i].ident, params[i].width);
    if (!error.empty()) {
      std::cerr << "error: " << error << "\n";
      return;
    }
    input_values.push_back(parsed->bits);
  }

  // Simulate and display.
  auto results = simulate(pc, input_values);
  auto output_lines = format_outputs(pc, results);
  for (const auto &line : output_lines) {
    std::cout << "  " << line << "\n";
  }
}

// ── handle_list ─────────────────────────────────────────────────────────────
/// Displays all components loaded from the source file with their signatures.
void Repl::handle_list() const {
  if (registry_.empty()) {
    std::cout << "  (no components loaded)\n";
    return;
  }
  for (const auto &[name, comp] : registry_) {
    std::cout << "  " << name << "(";
    for (size_t i = 0; i < comp.params.size(); ++i) {
      if (i > 0) std::cout << ", ";
      std::cout << comp.params[i].ident << ":" << comp.params[i].width;
    }
    std::cout << ")\n";
  }
}

// ── handle_help ─────────────────────────────────────────────────────────────
/// Prints available commands and input formats.
void Repl::handle_help() const {
  std::cout << "Commands:\n"
            << "  run ComponentName(arg1, arg2, ...)  simulate a component\n"
            << "  list                                show loaded components\n"
            << "  help                                show this message\n"
            << "  exit                                quit the REPL\n"
            << "\n"
            << "Input formats:\n"
            << "  42      decimal integer\n"
            << "  0b1010  binary literal (digit count must match parameter width)\n";
}

} // namespace gate
