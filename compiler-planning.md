# Compiler & Simulation Design

## Overview

Pipeline: **Source → AST** (parser) **→ Circuit** (compiler) **→ PreparedCircuit** (preparation) **→ Results** (simulation)

The compiler transforms a parsed AST into an immutable **Circuit** — a flat graph of
primitive gate nodes. A **preparation** step topologically sorts the Circuit into a
**PreparedCircuit** ready for evaluation. The **simulator** walks the prepared circuit
to compute output values from concrete inputs.

Two consumption paths are planned:

- **REPL `run` command:** Cache `PreparedCircuit`s by component name. On
  `run Foo(1,0)`, look up the cache, call `simulate`, display results.
- **Compile-to-binary:** Take a `PreparedCircuit`, emit C code (one operation per
  step in `eval_order`), compile with a host C compiler into a standalone executable.

---

## 1. Circuit Representation

This is the central design question. The previous `DAG` struct conflated three
concerns — compile-time graph structure, evaluation ordering, and mutable runtime
state — into a single type. This section explores alternatives.

### 1.1 Problems with the Current Design

- **`Node::val`** embeds mutable simulation state in a compile-time structure.
  The compiled graph cannot be treated as immutable; concurrent simulation is
  impossible; caching is unreliable.
- **`topo_order`** on the DAG couples evaluation strategy to the graph definition.
  Topological ordering is a concern of the *consumer* (simulator, codegen), not an
  intrinsic property of the circuit graph.
- **`operator()`** on the DAG makes the graph responsible for evaluating itself.
  There is no seam between "the thing you compile" and "the thing you run," leaving
  nowhere clean to attach features like signal tracing, stepping, or display.
- **Registers (v2.0)** would require persistent state across evaluations. In the
  current design, that state would be even more mutable fields on the DAG, deepening
  the conflation.

### 1.2 Design Options

#### Option A: Circuit + Simulator (reference-based)

Circuit is an immutable value. Simulator holds a reference to it and owns the
evaluation state.

```cpp
struct Circuit {
    std::vector<Node> nodes;
    size_t num_inputs;
    std::vector<NamedSignal> outputs;
};

class Simulator {
    const Circuit& circuit_;
    std::vector<size_t> topo_order_;
    std::vector<uint64_t> values_;
public:
    explicit Simulator(const Circuit& c);
    std::vector<uint64_t> run(const std::vector<uint64_t>& inputs);
};
```

| Pros | Cons |
|------|------|
| Clean separation; no duplication | Reference lifetime coupling — Simulator must not outlive Circuit |
| Circuit is truly immutable and thread-safe | REPL cache must co-manage Circuit and Simulator lifetimes |
| Multiple Simulators can share one Circuit | Cannot move or serialize a Simulator independently |
| Minimal type count | |

#### Option B: Self-Contained Simulator (owns Circuit)

Simulator takes ownership of the Circuit. One object to cache.

```cpp
class Simulator {
    Circuit circuit_;
    std::vector<size_t> topo_order_;
    std::vector<uint64_t> values_;
public:
    explicit Simulator(Circuit c);
    const Circuit& circuit() const;
    std::vector<uint64_t> run(const std::vector<uint64_t>& inputs);
};
```

| Pros | Cons |
|------|------|
| Self-contained — no lifetime issues | Cannot share a Circuit between Simulator and Codegen without copying |
| REPL cache is just `map<string, Simulator>` | "Simulator" becomes the universal handle, even for non-simulation use |
| Simple, minimal API | Conceptually muddy: is Simulator the circuit or the evaluator? |

#### Option C: Staged Pipeline (Circuit → PreparedCircuit)

Each pipeline stage is a value-type transformation. PreparedCircuit is the "sorted
DAG" — a self-contained, evaluation-ready artifact.

```cpp
struct Circuit {
    std::vector<Node> nodes;
    size_t num_inputs = 0;
    std::vector<NamedSignal> outputs;

    Signal add_node(GateType type, std::vector<size_t> inputs, int width);
};

struct PreparedCircuit {
    Circuit circuit;
    std::vector<size_t> eval_order;
};

PreparedCircuit prepare(Circuit circuit);

std::vector<uint64_t> simulate(const PreparedCircuit& pc,
                                const std::vector<uint64_t>& inputs);
```

| Pros | Cons |
|------|------|
| All value types — ownership is trivial (move semantics) | One more type to understand |
| Pipeline stages are explicit: compile → prepare → simulate | Circuit is moved into PreparedCircuit (access via `pc.circuit`) |
| `PreparedCircuit` is a named, cacheable "sorted DAG" concept | |
| `simulate` is a stateless free function — trivial to test | |
| Both consumption paths take `PreparedCircuit` naturally | |
| REPL cache is `map<string, PreparedCircuit>` | |

### 1.3 Recommendation: Option C (Staged Pipeline)

Option C best serves both consumption paths and keeps each type focused on one job:

- **Circuit** is the compiler's output. It knows about nodes, inputs, and outputs.
  It doesn't know how to evaluate itself. It's immutable after compilation (the
  `add_node` method is a builder used *during* compilation, not after).

- **PreparedCircuit** is the evaluation-ready artifact. It answers the question
  "what is the sorted DAG?" with a concrete, named type. The REPL caches it. The
  codegen module reads it. It owns its Circuit via move — no lifetime issues.

- **`simulate`** is a pure function: `(PreparedCircuit, inputs) → outputs`. No
  mutable state, no side effects, trivially testable.

For the REPL: `map<string, PreparedCircuit>` is the cache. On `run Foo(1, 0)`:

1. Lookup `"Foo"` in cache. On miss: `compile_component` → `prepare` → store.
2. `auto results = simulate(pc, {1, 0});`
3. `auto display = format_outputs(pc, results);`

For compile-to-binary: a future `codegen(const PreparedCircuit&)` walks `eval_order`
and emits C source — one line per gate operation.

**Why not A or B?** Option A's reference coupling is fragile for a project where
the cache and REPL are the primary consumers. Option B wraps Circuit inside Simulator,
which is awkward when you want the Circuit for non-simulation purposes (codegen,
display, analysis). Option C keeps them composable.

### 1.4 Register Extensibility (v2.0)

When sequential logic arrives, the staged pipeline extends cleanly:

1. Add `GateType::Register` to the enum.
2. Register nodes appear in the Circuit like any other node.
3. `prepare()` identifies registers and plans the evaluation order:
   - Registers output their *old* value first.
   - Combinational logic evaluates.
   - New register input values are captured.
4. A **`Simulator` class** layers on top for stateful evaluation:

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

The key principle: **combinational simulation remains a stateless free function**.
Sequential simulation layers on top with its own state management, without modifying
`Circuit`, `PreparedCircuit`, or `simulate`.

---

## 2. Signal & Type Safety

### 2.1 Current Issues

- `outputs` uses `pair<string, size_t>` — width information is lost.
- `compile_body`'s `arg_nodes` is `vector<size_t>` — width must be looked up
  separately at each use site.
- `return_nodes` is `vector<size_t>` — same problem.
- Easy to accidentally pass a loop counter or array length as a "node index."

### 2.2 Proposed Improvements

**Signal as the universal compiler currency.** Every function that refers to a node
during compilation should use `Signal` (node index + width). This eliminates
scattered width lookups and catches width mismatches at the point of use rather than
later during validation.

Concretely:
- `compile_expr` returns `optional<Signal>` (already does).
- `compile_inline` takes `vector<Signal>` args and returns `vector<Signal>` outputs.
- `handle_return` returns `vector<Signal>`.
- Statement handlers read and write Signals in the SymbolTable.

**Named outputs use a dedicated struct:**

```cpp
struct NamedSignal {
    std::string name;
    Signal signal;
};
```

Replacing `pair<string, size_t>` throughout.

**`add_node` returns Signal:**

```cpp
Signal Circuit::add_node(GateType type, std::vector<size_t> inputs, int width) {
    size_t idx = nodes.size();
    nodes.push_back(Node{type, std::move(inputs), width});
    return Signal{idx, width};
}
```

The compiler gets a Signal back immediately — no manual `Signal{idx, width}`
construction at every call site.

### 2.3 On Templating

C++ templates enforce constraints at **C++ compile time**, but signal widths in
gate-lang are **runtime values** parsed from source code. A `Signal<4>` template
parameter would require widths to be known when the C++ compiler runs, which they
aren't — they come from the `.gate` file. You also couldn't store heterogeneous
signals in a single container (`vector<Signal<?>>` isn't expressible).

Templates are the wrong tool here. The right approach is **runtime width validation
at Signal creation/combination points**, reported through the error system. The
`Signal` struct's value is that it *carries* width alongside the node reference,
making that validation convenient rather than requiring cross-referencing the node
array.

---

## 3. Compiler Architecture

### 3.1 Current Issues

- `compile_body` is a monolithic ~90-line function handling all statement types
  plus parameter binding, with five parameters threaded through every call.
- No architectural distinction between top-level compilation (fills
  `circuit.outputs`) and inlined compilation (returns signals to caller). The
  `ReturnStmt` handler pushes directly to `dag.outputs`, which would corrupt
  outputs during inlining.
- `CompCall` currently calls `compile_component` (which creates a *separate* DAG)
  instead of inlining into the current circuit. The public API doesn't expose a
  way to compile into an existing circuit.
- Statement dispatch via cascading `get_if` is verbose and doesn't leverage the
  variant type system.

### 3.2 Options

#### Option A: ComponentCompiler Class

Encapsulate the compilation context (circuit, registry, errors, symbol table) in a
class. Each instance represents one scope (one component body). Inlining creates
a child instance that shares the Circuit but has a fresh SymbolTable.

```cpp
class ComponentCompiler {
public:
    ComponentCompiler(Circuit& circuit,
                      const ComponentRegistry& registry,
                      ErrorReporter& errors);

    bool compile_top_level(const ast::Comp& comp);

    std::optional<std::vector<Signal>> compile_inline(
        const ast::Comp& comp,
        const std::vector<Signal>& args);

private:
    Circuit& circuit_;
    const ComponentRegistry& registry_;
    ErrorReporter& errors_;
    SymbolTable symbols_;

    bool handle_init(const ast::InitAssign& stmt);
    bool handle_mutation(const ast::MutAssign& stmt);
    bool handle_comp_call(const ast::CompCall& stmt);
    std::vector<Signal> handle_return(const ast::ReturnStmt& stmt);
    std::optional<Signal> compile_expr(const ast::Expr& expr);
};
```

**How inlining works:**

```
compile_top_level("FullAdder")
├── binds params to input nodes
├── handle_comp_call("HalfAdder")
│   ├── resolves arg signals from caller's SymbolTable
│   ├── creates new ComponentCompiler (same circuit_, fresh symbols_)
│   └── calls compile_inline(HalfAdder, args)
│       ├── binds params to arg signals
│       ├── handle_init (sum, carry)
│       ├── handle_return → returns [sum_sig, carry_sig]
│       └── caller binds outputs to its own SymbolTable
├── handle_init (cout)
└── handle_return → populates circuit_.outputs
```

| Pros | Cons |
|------|------|
| `compile_top_level` vs `compile_inline` solves the dual-output problem | One more class to understand |
| Shared state lives in fields, not function parameters | |
| Each handler is small and single-purpose | |
| Natural recursion: new instance per scope | |
| Easy to unit test handlers in isolation | |

#### Option B: Visitor Pattern

Define a `StmtVisitor` interface with one virtual method per statement type.
`CompileVisitor` implements it and carries the compilation context.

```cpp
struct StmtVisitor {
    virtual void visit(const ast::InitAssign&) = 0;
    virtual void visit(const ast::MutAssign&) = 0;
    virtual void visit(const ast::CompCall&) = 0;
    virtual void visit(const ast::ReturnStmt&) = 0;
};
```

| Pros | Cons |
|------|------|
| Classic, well-documented pattern | Verbose boilerplate for a `variant`-based AST |
| Natural if multiple AST passes are needed | Requires adapter layer: variant → virtual dispatch |
| | Doesn't naturally solve top-level vs. inline distinction |
| | Overkill for a single-pass compiler |

### 3.3 Recommendation: Option A (ComponentCompiler)

The ComponentCompiler class is the right fit for gate-lang:

- It **solves the inlining problem** structurally: `compile_top_level` fills
  `circuit.outputs`; `compile_inline` returns `vector<Signal>` to the caller.
  Two entry points, one class, no mode flag.
- It **groups context naturally** — no more threading five parameters through
  every function call.
- It **leverages `std::visit`** (or `get_if`) for dispatch, which is natural for
  a variant-based AST. The visitor pattern adds virtual dispatch *on top of* variant
  dispatch, which is redundant.
- It **scales** to new statement types: add a handler method, add a case to the
  dispatch.

The class is an **implementation detail** of `Compiler.cpp`. The public API stays
simple:

```cpp
ComponentRegistry build_registry(const ast::Program& program);

std::optional<Circuit> compile_component(const std::string& name,
                                          const ComponentRegistry& registry,
                                          ErrorReporter& errors);
```

---

## 4. Error Handling

### 4.1 Current Issues

- `string* error_out` is fragile: nullable, holds only one error, no structure.
- The `report()` helper returns `bool` but callers return `optional<T>` — the
  return types don't compose.
- No source location, no error categorization, no way to accumulate multiple errors.
- Many error sites are TODO comments with no early returns — execution falls through.

### 4.2 Proposed Design

**Structured error type** with categorization and (future) source locations:

```cpp
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
```

**Error accumulator** that collects all errors from a compilation:

```cpp
class ErrorReporter {
public:
    void report(CompileError::Kind kind, const std::string& message);
    void report(CompileError error);

    bool has_errors() const;
    const std::vector<CompileError>& errors() const;
    std::string format_all() const;

private:
    std::vector<CompileError> errors_;
};
```

**Usage pattern** in statement handlers — report and bail:

```cpp
bool ComponentCompiler::handle_init(const ast::InitAssign& stmt) {
    if (symbols_.contains(stmt.target.ident)) {
        errors_.report(CompileError::Kind::DuplicateSymbol,
                       "'" + stmt.target.ident + "' is already defined");
        return false;
    }
    auto sig = compile_expr(stmt.value);
    if (!sig) return false;
    if (sig->width != stmt.target.width) {
        errors_.report(CompileError::Kind::WidthMismatch,
                       "'" + stmt.target.ident + "' declared as width " +
                       std::to_string(stmt.target.width) +
                       " but expression has width " +
                       std::to_string(sig->width));
        return false;
    }
    symbols_[stmt.target.ident] = *sig;
    return true;
}
```

**Source locations** require the parser to annotate AST nodes (cpp-peglib provides
line/col info during parsing). This is a follow-up task — the `ErrorReporter`
architecture works with or without locations. When locations are added to the AST,
errors will automatically carry them through to display.

---

## 5. Complete Type Reference

All types for the redesigned system in one place:

```cpp
namespace gate {

// ── Primitives ──────────────────────────────────────────────────────────────

enum class GateType { Input, And, Or, Xor, Not };

struct Signal {
    size_t node;
    int width;
};

struct NamedSignal {
    std::string name;
    Signal signal;
};

struct Node {
    GateType type;
    std::vector<size_t> inputs;
    int width;
};

// ── Circuit (compiler output, immutable after compilation) ──────────────────

struct Circuit {
    std::vector<Node> nodes;
    size_t num_inputs = 0;
    std::vector<NamedSignal> outputs;

    Signal add_node(GateType type, std::vector<size_t> inputs, int width);
};

// ── PreparedCircuit (topologically sorted, evaluation-ready) ────────────────

struct PreparedCircuit {
    Circuit circuit;
    std::vector<size_t> eval_order;
};

PreparedCircuit prepare(Circuit circuit);

// ── Simulation ──────────────────────────────────────────────────────────────

std::vector<uint64_t> simulate(const PreparedCircuit& pc,
                                const std::vector<uint64_t>& inputs);

std::vector<std::string> format_outputs(const PreparedCircuit& pc,
                                         const std::vector<uint64_t>& results);

// ── Error Handling ──────────────────────────────────────────────────────────

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
    void report(CompileError::Kind kind, const std::string& message);
    void report(CompileError error);
    bool has_errors() const;
    const std::vector<CompileError>& errors() const;
    std::string format_all() const;
private:
    std::vector<CompileError> errors_;
};

// ── Compiler ────────────────────────────────────────────────────────────────

using ComponentRegistry = std::unordered_map<std::string, ast::Comp>;
using SymbolTable = std::unordered_map<std::string, Signal>;

ComponentRegistry build_registry(const ast::Program& program);

std::optional<Circuit> compile_component(const std::string& name,
                                          const ComponentRegistry& registry,
                                          ErrorReporter& errors);

} // namespace gate
```

---

## 6. File Layout

| File | Responsibility |
|------|---------------|
| `include/Circuit.hpp` | `GateType`, `Node`, `Signal`, `NamedSignal`, `Circuit`, `PreparedCircuit` |
| `include/Compiler.hpp` | `ComponentRegistry`, `SymbolTable`, `compile_component`, `build_registry` |
| `include/Simulation.hpp` | `prepare`, `simulate`, `format_outputs` |
| `include/Error.hpp` | `SourceLocation`, `CompileError`, `ErrorReporter` |
| `src/Compiler.cpp` | `ComponentCompiler` (internal class), all compilation logic |
| `src/Simulation.cpp` | `prepare` (Kahn's algorithm), `simulate` (eval loop), `format_outputs` |

`DAG.hpp` is retired. Its contents are reorganized into `Circuit.hpp` and
`Simulation.hpp`.

---

## 7. Implementation Checklist

Recommended order — each phase builds on the previous.

### Phase 1: Foundation (types and file structure)

- [ ] Create `include/Error.hpp` with `SourceLocation`, `CompileError`, `ErrorReporter`
- [ ] Implement `ErrorReporter` methods in `src/Error.cpp`
- [ ] Rename `include/DAG.hpp` → `include/Circuit.hpp`
- [ ] Rename `DAG` → `Circuit`, remove `Node::val`, remove `topo_order`, remove `operator()`
- [ ] Add `NamedSignal` struct; change `outputs` to `vector<NamedSignal>`
- [ ] Have `add_node` return `Signal` instead of `size_t`
- [ ] Add `PreparedCircuit` struct to `Circuit.hpp`
- [ ] Update `include/Simulation.hpp`: declare `prepare`, `simulate`, `format_outputs`
- [ ] Update `include/Compiler.hpp`: replace `string* error_out` with `ErrorReporter&`
- [ ] Verify the project still compiles (stubs are fine)

### Phase 2: Compiler rewrite

- [ ] Implement `ComponentCompiler` class skeleton in `Compiler.cpp`
- [ ] Implement `compile_expr` as a `ComponentCompiler` method
- [ ] Implement `handle_init`
- [ ] Implement `handle_mutation`
- [ ] Implement `handle_return` (with top-level vs inline distinction)
- [ ] Implement `handle_comp_call` with recursive `compile_inline`
- [ ] Implement `compile_top_level` (creates input nodes, dispatches, fills outputs)
- [ ] Wire up public `compile_component` → `ComponentCompiler::compile_top_level`
- [ ] Test: compile `HalfAdder` from `adders.gate`, inspect Circuit nodes

### Phase 3: Simulation

- [ ] Implement `prepare()` — Kahn's algorithm → `PreparedCircuit`
- [ ] Implement `simulate()` — walk `eval_order`, compute gate outputs, collect results
- [ ] Implement `format_outputs()` — zip `NamedSignal` names with result values
- [ ] Test: `prepare` + `simulate` on compiled `HalfAdder`, verify truth table

### Phase 4: Integration

- [ ] Update `main.cpp`: parse → compile → prepare → simulate → display
- [ ] Test end-to-end with `FullAdder` (exercises inlining)
- [ ] Add REPL loop with `PreparedCircuit` cache
- [ ] Test: `run FullAdder(1, 1, 0)` in REPL produces `sum = 0, cout = 1`

### Future work

- [ ] Source locations: annotate AST nodes during parsing, thread through to `CompileError`
- [ ] Compile-to-binary: codegen module that emits C from `PreparedCircuit`
- [ ] Sequential logic (v2.0): `GateType::Register`, `Simulator` class with `step()`
- [ ] Multi-bit operations: slicing (`a[0:0]`), concatenation (`{sum3, sum2, sum1, sum0}`)
