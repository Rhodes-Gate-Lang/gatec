# Future Work

Planned enhancements, roughly ordered by priority.

---

## Compiler Validation

- [ ] **Reject statements after `return`** — `compile_body` currently keeps
  processing statements after a `ReturnStmt`. Should report an error if
  `return_signals_` is already populated when the next statement is encountered.

- [ ] **Detect recursive/cyclic component calls** — A "currently compiling" set
  should be threaded through `compile_inline` to catch mutual recursion before it
  stack-overflows. Gate-lang is combinational, so recursion is always invalid.

## Error Quality

- [ ] **Source locations in errors** — Annotate each AST node with a
  `SourceLocation` (file, line, col) during parsing. cpp-peglib provides this via
  `SemanticValues::line_info()`. Thread locations through to `ErrorReporter` so
  errors print as `file:line:col: message`. The `ErrorReporter` interface already
  accepts `SourceLocation`; the AST structs just need the field added.

## Multi-bit Operations

- [ ] **Bit slicing** — `a[0:0]` to extract a single bit or range from a wider
  bus. Requires a new AST expression type and a compiler handler that emits
  shift + mask nodes.

- [ ] **Concatenation** — `{sum3, sum2, sum1, sum0}` to merge signals into a
  wider bus. Requires a new AST expression type and a compiler handler that emits
  shift + OR nodes.

These are needed to support `rca4.gate` (the 4-bit ripple-carry adder example).

## Compile-to-Binary

- [ ] **Codegen module** — Take a `PreparedCircuit` and emit C source code. Each
  step in `eval_order` becomes one line of C (bitwise operations on `uint64_t`
  variables). The emitted C is compiled with a host compiler into a standalone
  binary.

  Sketch: `codegen(const PreparedCircuit&) → std::string` returning the C source.

## Sequential Logic (v2.0)

- [ ] **Register primitive** — Add `GateType::Register` to the enum. Register
  nodes output their *previous* value during combinational evaluation; new input
  values are captured at the end of each clock step.

- [ ] **`Simulator` class** — Wraps a `PreparedCircuit` with persistent register
  state. Provides `step()` (one clock cycle) and `reset()`. Combinational
  `simulate()` remains a stateless free function; sequential simulation layers on
  top.

  ```cpp
  class Simulator {
      PreparedCircuit pc_;
      std::vector<uint64_t> register_state_;
  public:
      explicit Simulator(PreparedCircuit pc);
      std::vector<uint64_t> step(const std::vector<uint64_t>& inputs);
      void reset();
  };
  ```

- [ ] **Clock/reset semantics** — Define how clock and reset signals interact
  with registers in the language syntax.

## REPL Enhancements

- [ ] **Additional input formats** — Hex (`0xFF`), signed integers (`s:-3`),
  floating-point bit patterns (`f:3.14`).

- [ ] **Output format control** — Command or flag to display results in decimal,
  binary, hex, or signed interpretation.

- [ ] **Source reload** — `reload` command that re-parses the source file and
  invalidates the `PreparedCircuit` cache.

## Code Quality

- [ ] **SymbolTable / ComponentRegistry wrappers** — Replace the raw
  `unordered_map` type aliases with thin wrapper classes exposing `contains()`,
  `lookup()`, `define()`. Deferred until the compiler grows enough to justify the
  abstraction.
