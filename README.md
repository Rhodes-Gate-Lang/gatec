# Gate-Lang

A hardware description language for building combinational logic circuits from
primitive gates (AND, OR, XOR, NOT). Write components, compose them hierarchically,
and simulate them interactively.

```
comp HalfAdder(a:1, b:1) {
    sum:1 = a XOR b;
    carry:1 = a AND b;
    return sum, carry;
}

comp FullAdder(a:1, b:1, cin:1) {
    ha1_sum:1, ha1_carry:1 = HalfAdder(a, b);
    sum:1, ha2_carry:1 = HalfAdder(ha1_sum, cin);
    cout:1 = ha1_carry OR ha2_carry;
    return sum, cout;
}
```

## Building

Requires CMake 3.20+ and a C++17 compiler. cpp-peglib is fetched automatically.

```sh
cmake -B build
cmake --build build
```

## Usage

Load a `.gate` file to enter the REPL:

```
$ ./build/gate-lang examples/adders.gate
gate-lang repl — type 'help' for commands
> run HalfAdder(1, 1)
  sum = 0 (0b0)
  carry = 1 (0b1)
> run FullAdder(1, 1, 0)
  sum = 0 (0b0)
  cout = 1 (0b1)
```

### REPL Commands

| Command | Description |
|---------|-------------|
| `run Component(args...)` | Simulate a component with the given inputs |
| `list` | Show all loaded components with their signatures |
| `help` | Print available commands and input formats |
| `exit` | Quit the REPL |

### Input Formats

| Format | Example | Notes |
|--------|---------|-------|
| Decimal integer | `42` | Must fit in the parameter's bit width |
| Binary literal | `0b1010` | Digit count must match the parameter width exactly |

## Syntax

### Components

A component declares named inputs with bit widths, a body of statements, and
named return values:

```
comp Name(param:width, ...) {
    ...
    return output1, output2;
}
```

### Statements

**Init assignment** — declares a new signal from a gate expression:

```
sum:1 = a XOR b;
```

**Mutation** — reassigns an existing signal:

```
flag = NOT flag;
```

**Component call** — inlines another component, binding its outputs:

```
s:1, c:1 = HalfAdder(a, b);
```

**Return** — declares which signals are the component's outputs:

```
return sum, carry;
```

### Expressions

Binary operations: `AND`, `OR`, `XOR` (two operands, same width).
Unary: `NOT` (one operand). Parentheses for grouping.

```
result:1 = (a AND b) XOR (NOT c);
```

### Imports

```
import "adders.gate"
```

## Architecture

The pipeline has four stages, each producing a distinct type:

```
Source (.gate)
  │  parse_program()
  ▼
ast::Program          — syntax tree: components, statements, expressions
  │  compile_component()
  ▼
Circuit               — flat graph of primitive gate nodes (immutable)
  │  prepare()
  ▼
PreparedCircuit       — circuit + topological eval order (cacheable)
  │  simulate()
  ▼
vector<uint64_t>      — output bit values
```

**Circuit** is the compiler's output — a flat, immutable graph where every
sub-component call has been inlined into primitive gates. It carries no
simulation state.

**PreparedCircuit** adds a topological evaluation order (Kahn's algorithm).
The REPL caches these by component name. It is also the input for a future
compile-to-binary codegen path.

**simulate** is a stateless free function: given a prepared circuit and input
values, it walks the evaluation order, computes each gate, and returns output
values. No mutable state, no side effects.

### Compiler Internals

The compiler uses a `ComponentCompiler` class (internal to `Compiler.cpp`) that
represents one compilation scope. Inlining a component call creates a child
`ComponentCompiler` that shares the same `Circuit` being built but has its own
symbol table. Two entry points handle the top-level vs. inlined distinction:

- `compile_top_level` — creates input nodes, compiles the body, populates
  `circuit.outputs` from the return statement.
- `compile_inline` — binds argument signals to parameters, compiles the body,
  returns output signals to the caller for binding.

### Error Handling

An `ErrorReporter` accumulates structured `CompileError` objects with a `Kind`
enum (UndefinedSymbol, WidthMismatch, ArityMismatch, etc.) and a human-readable
message. Source location support is planned but not yet wired through the AST.

## Project Structure

```
include/
  Ast.hpp           — AST types: Program, Comp, Statement, Expr
  Circuit.hpp       — GateType, Node, Signal, Circuit, PreparedCircuit
  Compiler.hpp      — compile_component, build_registry
  Error.hpp         — CompileError, ErrorReporter
  Parser.hpp        — parse_program
  Repl.hpp          — interactive REPL
  Simulation.hpp    — prepare, simulate, format_outputs
src/
  Compiler.cpp      — ComponentCompiler class, AST → Circuit compilation
  Parser.cpp        — PEG grammar + semantic actions (cpp-peglib)
  Repl.cpp          — command loop, input parsing, run execution
  Simulation.cpp    — Kahn's algorithm, gate evaluation loop
  main.cpp          — CLI entry point
examples/
  adders.gate       — HalfAdder and FullAdder
  rca4.gate         — 4-bit ripple-carry adder (requires multi-bit ops)
```

## Milestones

### v0.1.0 — Parsing
Project setup, PEG grammar, AST definition.

### v0.2.0 — Compiler & Simulation
Circuit representation, compiler with component inlining, simulation engine,
interactive REPL with input validation. *(current)*

### v1.0.0 — MVP Release
Multi-bit operations (slicing, concatenation), source locations in errors,
compile-to-binary codegen.

### v2.0.0 — Sequential Logic
Register primitives, stateful `Simulator` class, clock/reset semantics.
