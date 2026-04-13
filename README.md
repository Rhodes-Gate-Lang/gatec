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

Requires CMake 3.20+ and a C++17 compiler. Dependencies fetched by CMake:

- [cpp-peglib](https://github.com/yhirose/cpp-peglib) (parser)
- [gateo-cpp](https://github.com/Rhodes-Gate-Lang/gateo-cpp) at tag [`v2.0.1`](https://github.com/Rhodes-Gate-Lang/gateo-cpp/releases/tag/v2.0.1) (`.gateo` I/O; the compiler uses the alias `gate::GateObject` → `gateo::v2::view::GateObject` from `compiler/GateoAliases.hpp`), which downloads the matching [gateo-schema](https://github.com/Rhodes-Gate-Lang/gateo-schema) **v2.0.1** release at configure time

You also need **Protocol Buffers** installed: the `protoc` compiler and the C++ protobuf library (`protobuf` / `libprotobuf` packages on most systems). If those are not available, configure with `-DGATEO_FETCH_PROTOBUF=ON` so gateo-cpp builds protobuf from source (slower first configure).

```sh
cmake -B build
cmake --build build
```

The compiler driver is **`build/gatec`** (CMake target `gate-lang-cli`).

## Usage

Compile a `.gate` file: every top-level component is resolved through `CompCache`, and each cached `GateObject` is written as **`<name>.gateo`**. By default files go to **`build/gateo-cache/`** (set at configure time; override with a second argument).

```
$ ./build/gatec examples/adders.gate
wrote 2 .gateo file(s) to /path/to/repo/build/gateo-cache
```

```
$ ./build/gatec examples/adders.gate ./my-gateo
```

### REPL *(not in tree yet)*

Interactive simulation via a REPL is described in older docs; the current CLI is compile-and-emit only until `ui/Repl` is wired back in.

### Input formats *(for future REPL / simulator)*

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

The pipeline is moving toward a `.gateo`-centric model:

```
Source (.gate)
  │  parse_program()
  ▼
ast::Program          — syntax tree: components, statements, expressions
  │  Compiler::get_comp()
  ▼
gate::GateObject      — flat gateo.v2 view (same shape as a .gateo file)
  │  (simulation TBD from GateObject)
  ▼
vector<uint64_t>      — output bit values (once sim is ported)
```

**Compiler** resolves a component name from an in-memory cache, future
precompiled `.gateo` search paths, or the AST. Version `{0,0}` marks an
in-progress compile for cycle detection; a stamped version marks completion.

**Statement lowering** is split under `src/compiler/statement/` (`init`,
`mutation`, `comp_call`, `return_stmt`), driven by `compile_statement` and
`CompileContext`.

**Circuit / PreparedCircuit** remain in the tree for legacy simulation until
that path reads `GateObject` instead.

### Error Handling

An `ErrorReporter` accumulates structured `CompileError` objects with a `Kind`
enum (UndefinedSymbol, WidthMismatch, ArityMismatch, etc.) and a human-readable
message. Source location support is planned but not yet wired through the AST.

## Project Structure

```
include/
  compiler/         — Compiler, CompileContext, statements, InlineGateo, …
  core/Ast.hpp      — Program, Comp, Statement, Expr
  core/Circuit.hpp  — legacy graph types (simulation)
  Parser.hpp        — parse_program
  Repl.hpp          — interactive REPL
  simulation/       — PreparedCircuit, prepare, simulate
src/
  compiler/         — get_comp, compile_component, statement/*.cpp
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
