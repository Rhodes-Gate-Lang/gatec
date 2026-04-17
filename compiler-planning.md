# Compiler Design Plan

**Goal:** Turn an `ast::Comp` into a `GateObject`.

## Guiding Principles

- **Highly coherent and readable** — each piece has one job, compile functions are straight-line happy-path code.
- **Loosely coupled** — dependencies are explicit in function signatures; no piece knows more than it needs.
- **Testable** — every component can be tested in isolation with hand-constructed inputs.
- **Fail-fast with exceptions** — the existing `CompileError` hierarchy provides structured, typed errors. Throw at the site, catch at the pipeline boundary (CLI/tests).

## Architecture Overview

```
┌───────────────────────────────────────────────────────┐
│  compile_component(ast::Comp) -> GateObject           │
│                                                       │
│  1. Create local NodeEmitter + SymbolTable            │
│  2. Emit Input nodes for params                       │
│  3. For each statement:                               │
│     - InitAssign / MutAssign: compile_expr, bind      │
│     - CompCall: cache.resolve(name)                   │
│               → inline_gate_object(child, emitter)    │
│               → bind output names                     │
│     - ReturnStmt: emit Output nodes, record indices   │
│  4. Return emitter.take()                             │
│                                                       │
│  Throws CompileError on any failure.                  │
└───────────────────────────────────────────────────────┘

┌──────────────────────┐    ┌───────────────────────────┐
│  CompCache           │    │  inline_gate_object()     │
│                      │    │                           │
│  resolve(name):      │    │  Pure function:           │
│    if cached → ret   │    │  (GateObject, emitter)    │
│    if on disk → load │    │  → InlineResult           │
│    if in_progress →  │    │                           │
│      throw Cyclic    │    │  Remaps all indices,      │
│    else → compile    │    │  splices into parent.     │
│      from AST        │    │                           │
└──────────────────────┘    └───────────────────────────┘

┌──────────────────────┐    ┌───────────────────────────┐
│  NodeEmitter         │    │  SymbolTable              │
│  (append-only)       │    │  (name → node index)      │
│                      │    │                           │
│  emit_node → u32     │    │  bind() — throws Dup      │
│  emit_component      │    │  resolve() — throws Undef │
│  take() → GateObj    │    │                           │
└──────────────────────┘    └───────────────────────────┘
```

## Stateful Components

These four types encapsulate all mutable state. Everything else is pure functions over them.

### `NodeEmitter` — append-only GateObject builder

Owns the growing `GateObject` and exposes a minimal API for appending nodes and component instances. Has no knowledge of AST types or symbol names.

```cpp
class NodeEmitter {
public:
  explicit NodeEmitter(std::string root_name);

  uint32_t emit_node(Node node);
  uint32_t emit_component(std::string name, uint32_t parent);

  GateObject take();  // move the finished object out (call once)

private:
  GateObject obj_;
};
```

**Testability:** Emit some nodes, call `take()`, assert on the resulting `GateObject`. No AST types needed.

### `SymbolTable` — scope-aware name-to-index map

Maps variable names to node indices within a single component's compilation. A value type that knows nothing about AST nodes or the emitter.

```cpp
class SymbolTable {
public:
  void bind(const std::string& name, uint32_t node_index);   // throws DuplicateSymbolError
  uint32_t resolve(const std::string& name) const;           // throws UndefinedSymbolError

private:
  std::unordered_map<std::string, uint32_t> symbols_;
};
```

**Testability:** Bind a few names, resolve them, check that duplicates and unknowns throw the right error types.

### `CompCache` — cycle detection + memoisation + filesystem lookup

Replaces the simpler `CompRegistry` concept. Every compiled component is stored as a self-contained `GateObject`. When a subcomponent is needed, the cache checks three sources in order:

1. **In-memory cache** — already compiled this run.
2. **Filesystem** — a pre-built `.gateo` file (future standard library path).
3. **AST compilation** — dispatch `compile_component` for the named `ast::Comp`.

Cycle detection is built in: if a component is currently `InProgress`, `resolve` throws `CyclicDependencyError`.

```cpp
class CompCache {
public:
  explicit CompCache(const std::vector<ast::Comp>& program_comps);

  const GateObject& resolve(const std::string& name);  // throws CyclicDependencyError,
                                                        //        UndefinedComponentError

private:
  std::unordered_map<std::string, const ast::Comp*> ast_comps_;
  std::unordered_map<std::string, GateObject> cache_;
  std::unordered_set<std::string> in_progress_;
};
```

**Testability:**
- Cycle detection: call `resolve` on a component that depends on itself, assert `CyclicDependencyError`.
- Caching: compile a component, call `resolve` again, verify no re-compilation occurs.
- Can be tested with trivial `ast::Comp` values that have no body (just params + return).

### `CompileError` hierarchy (existing)

Already well-designed. Each error type carries structured data and formats its own message. The hierarchy maps directly to error sites:

| Error type                | Throw site                          |
|---------------------------|-------------------------------------|
| `DuplicateSymbolError`    | `SymbolTable::bind`                 |
| `UndefinedSymbolError`    | `SymbolTable::resolve`              |
| `CyclicDependencyError`   | `CompCache::resolve`                |
| `UndefinedComponentError` | `CompCache::resolve`                |
| `MissingReturnError`      | `compile_component` (end of body)   |
| `WidthMismatchError`      | `compile_expr`, `compile_statement` |
| `ArityMismatchError`      | `compile_statement` (CompCall)      |

## Pure Functions

### `compile_component`

The top-level entry point. Creates a local `NodeEmitter` and `SymbolTable`, compiles the body, and returns a self-contained `GateObject`.

```cpp
GateObject compile_component(const ast::Comp& comp, CompCache& cache);
```

Pseudocode:

```
compile_component(comp, cache):
    emitter = NodeEmitter(comp.name)
    symbols = SymbolTable()

    for each param in comp.params:
        idx = emitter.emit_node(Input { width=param.width, name=param.ident })
        symbols.bind(param.ident, idx)

    for each stmt in comp.body:
        compile_statement(stmt, symbols, emitter, root_component=0, cache)

    // ReturnStmt should have emitted Output nodes.
    return emitter.take()
```

The root component (the one the user actually wants as the final output) is compiled by calling this function directly. Its result *is* the final `GateObject`. Subcomponent compilations also go through this function but are triggered via `CompCache::resolve`, and their results are inlined into the parent.

### `compile_statement`

Dispatches on the `Statement` variant. Receives everything it needs as explicit parameters.

```cpp
void compile_statement(const ast::Statement& stmt,
                       SymbolTable& symbols,
                       NodeEmitter& emitter,
                       uint32_t parent_component,
                       CompCache& cache);
```

- **`InitAssign`:** Compile the expression, emit the result, bind the target name.
- **`MutAssign`:** Compile the expression, rebind the target name (or overwrite).
- **`CompCall`:** Resolve the subcomponent via `cache`, inline it, bind output names.
- **`ReturnStmt`:** Resolve each returned name, emit `Output` nodes.

Note that `CompCache` is only needed because of `CompCall`. The other three cases only need `SymbolTable` and `NodeEmitter`.

### `compile_expr`

Compiles an expression tree into nodes. Returns the node index of the result.

```cpp
uint32_t compile_expr(const ast::Expr& expr,
                      const SymbolTable& symbols,
                      NodeEmitter& emitter,
                      uint32_t parent_component);
```

- **`std::string` (variable reference):** Resolve via `symbols`, return the index directly.
- **`UnaryExpr` (NOT):** Recursively compile the operand, emit a `Not` node referencing it.
- **`BinExpr`:** Recursively compile LHS and RHS, emit an `And`/`Or`/`Xor` node.

This function has no knowledge of `CompCache` — it only resolves names and emits nodes.

### `inline_gate_object` — the inliner

A pure function that splices a child `GateObject` into a parent `NodeEmitter`. This is the key enabler for the cache-based design.

```cpp
struct InlineResult {
  std::vector<uint32_t> input_node_indices;   // in parent's index space
  std::vector<uint32_t> output_node_indices;  // in parent's index space
};

InlineResult inline_gate_object(const GateObject& child,
                                NodeEmitter& parent_emitter,
                                uint32_t parent_component);
```

The inliner must:

1. Emit new `ComponentInstance` entries for all of the child's components, remapping their `parent` indices relative to the parent's component space.
2. Walk the child's `nodes` in order (they're already topologically sorted). For each node:
   - Offset all `inputs` indices by the node base offset.
   - Offset the `parent` component index by the component base offset.
   - Emit the remapped node into the parent emitter.
3. Collect the indices of `Input` and `Output` typed nodes (in the parent's index space) and return them as `InlineResult`.

The caller (`compile_statement` handling `CompCall`) then:
- Wires the caller's argument node indices to the child's input nodes (these become the same node, or a pass-through — design decision).
- Binds the output variable names to the child's output node indices.

**Testability:** Construct small `GateObject` values by hand (e.g., a NOT gate with one input and one output), inline them into an empty `NodeEmitter`, verify all indices are correctly remapped.

## How `CompCall` works end-to-end

Given `(out1, out2) = SubComp(a, b)`:

1. `compile_statement` resolves `a` and `b` via `symbols` → gets node indices for the arguments.
2. Calls `cache.resolve("SubComp")` → gets a `const GateObject&` (compiled, cached, or loaded from disk).
3. Calls `inline_gate_object(child, emitter, parent_component)` → gets `InlineResult` with input/output node indices in the parent's space.
4. Validates arity: `call.args.size() == result.input_node_indices.size()`, `call.outputs.size() == result.output_node_indices.size()`.
5. Wires inputs: for each `(arg_node_idx, child_input_idx)` pair, the child's input node needs to be connected to the caller's argument. (Implementation detail: either rewrite the child's input node to point at the arg, or emit a pass-through.)
6. Binds outputs: `symbols.bind("out1", result.output_node_indices[0])`, etc.

## Key Design Decisions

### Why `GateObject` as the cache format?

- **Uniform representation:** A standard library component loaded from disk and a component defined in the same file are both `GateObject` values. The inliner handles both identically.
- **Clean separation:** `compile_component` always produces a self-contained `GateObject`. It never thinks about a parent context. Inlining is a separate, well-defined operation.
- **Future-proof:** Enables separate compilation, caching across runs (serialize to disk), and a standard library with zero additional compilation logic.

### Why exceptions over `std::expected`?

- **Straight-line code:** Compile functions are happy-path only. No `TRY` macros, no `if (!result)` checks.
- **The hierarchy is already built:** `CompileError` subtypes carry structured data (`symbol_name`, `expected_width`, etc.) and format their own messages.
- **Fail-fast is correct here:** A broken wire invalidates everything downstream. There's no value in continuing past the first error.
- **Catch at the boundary:** CLI, REPL, and tests all catch `CompileError` uniformly.

### Partial emission on error

If a throw occurs mid-compilation, the `NodeEmitter`'s internal `GateObject` is left in an inconsistent state. This is fine — the emitter is stack-local, so it's destroyed when the exception unwinds. No cleanup needed.

## File Layout

```
include/compiler/
├── CompCache.hpp              # Cache + cycle detection + dispatch
├── CompileComponent.hpp       # compile_component() — top-level entry
├── CompileError.hpp           # Exception hierarchy (existing, unchanged)
├── CompileExpr.hpp            # compile_expr() — expression tree → nodes
├── CompileStatement.hpp       # compile_statement() — variant dispatcher
├── GateoAliases.hpp           # Type aliases for gateo-cpp (existing, unchanged)
├── InlineGateo.hpp            # inline_gate_object() — pure inliner
├── NodeEmitter.hpp            # Append-only GateObject builder (existing, unchanged)
├── SymbolTable.hpp            # Name → node index map (existing, unchanged)
└── statement/
    ├── stmt_comp_call.hpp     # stmt_comp_call() — subcomponent inlining
    ├── stmt_init.hpp          # stmt_init() — init assignment
    ├── stmt_mutation.hpp      # stmt_mutation() — reassignment
    └── stmt_return.hpp        # stmt_return() — output emission

src/compiler/
├── CompCache.cpp
├── CompileComponent.cpp
├── CompileExpr.cpp
├── CompileStatement.cpp
├── InlineGateo.cpp
├── NodeEmitter.cpp            # (existing, unchanged)
├── SymbolTable.cpp            # (existing, unchanged)
└── statement/
    ├── comp_call.cpp
    ├── init.cpp
    ├── mutation.cpp
    └── return_stmt.cpp
```

### Placement rationale

**Top-level `include/compiler/` for orchestration and infrastructure.** These files
define the public interfaces that coordinate compilation: the cache, the three
`compile_*` free functions, the emitter, and the symbol table. They live together
because they form the compiler's API surface — any external caller (CLI, tests)
only needs to include from this level.

**`statement/` subdirectory for per-statement-type logic.** Each statement handler
is a single free function with a narrowly scoped job. Grouping them in a subdirectory
keeps the top-level `compiler/` directory focused on cross-cutting concerns and makes
it immediately clear where to find "how does init assignment compile?" vs "how does
the whole compilation pipeline fit together?"

**One header + one `.cpp` per function/class.** Each file contains exactly one public
function or class. This makes dependencies between pieces grep-able via `#include`
lines and keeps compilation units small for incremental builds.

### Dependency graph (headers only)

```
CompileComponent.hpp
  └── CompCache.hpp (forward-declared)
      └── GateoAliases.hpp
      └── Ast.hpp

CompileStatement.hpp
  └── NodeEmitter.hpp
  └── SymbolTable.hpp
  └── CompCache.hpp (forward-declared)

CompileExpr.hpp
  └── NodeEmitter.hpp
  └── SymbolTable.hpp

InlineGateo.hpp
  └── NodeEmitter.hpp
  └── GateoAliases.hpp

statement/stmt_init.hpp        ─┐
statement/stmt_mutation.hpp     │── NodeEmitter, SymbolTable
statement/stmt_return.hpp      ─┘

statement/stmt_comp_call.hpp   ──── NodeEmitter, SymbolTable, CompCache (fwd)
```

Note that `CompileExpr.hpp` and the non-CompCall statement headers have **no dependency
on `CompCache`**. This is the loose coupling the design targets: expression compilation
and most statement compilation can be built, tested, and reasoned about without any
knowledge of the caching/subcomponent system.

## Implementation Steps

### Phase 1: Core infrastructure

1. **`NodeEmitter`** — Implement the class with `emit_node`, `emit_component`, and `take`. Write unit tests that emit a handful of nodes and assert on the resulting `GateObject`.

2. **`SymbolTable`** — Implement `bind` and `resolve` with the appropriate `CompileError` throws. Write unit tests for successful lookups, duplicate binds, and undefined resolves.

### Phase 2: Expression compilation

3. **`compile_expr`** — Implement the three `Expr` variant cases (variable ref, unary, binary). Test with a hand-built `SymbolTable` and `NodeEmitter`, verifying emitted node types, input wiring, and widths.

### Phase 3: Statement compilation (excluding CompCall)

4. **`compile_statement` for `InitAssign`** — Compile the expression, bind the result. Test that the symbol table is updated and the correct nodes are emitted.

5. **`compile_statement` for `MutAssign`** — Compile the expression, rebind (or overwrite) the target. Test the overwrite semantics.

6. **`compile_statement` for `ReturnStmt`** — Resolve each name, emit `Output` nodes. Test that the right nodes get the `Output` type and names.

### Phase 4: Component compilation (single-component, no subcomps)

7. **`compile_component`** (without `CompCall` support) — Wire together `NodeEmitter`, `SymbolTable`, and the statement compilers. Test with simple `ast::Comp` values that use only `InitAssign`, `MutAssign`, and `ReturnStmt`. Verify the full `GateObject` output.

### Phase 5: The inliner

8. **`inline_gate_object`** — Implement index remapping for nodes and component instances. Test exhaustively with small hand-built `GateObject` values:
   - Single-node identity (one input, one output).
   - Multi-node (e.g., a NOT gate: input → not → output).
   - Nested components (child has its own component instances).

### Phase 6: CompCall and the cache

9. **`CompCache`** — Implement with in-memory caching and cycle detection. Filesystem lookup can be stubbed initially. Test cycle detection independently (begin → begin → assert throw).

10. **`compile_statement` for `CompCall`** — Integrate `CompCache::resolve` + `inline_gate_object` + argument wiring + output binding. Test with a program containing two components where one calls the other.

### Phase 7: Integration

11. **End-to-end tests** — Compile full `ast::Program` values (multiple components, cross-component calls) and verify the final `GateObject`. Test error cases: cycles, undefined components, arity mismatches, width mismatches.

12. **Wire into CLI/REPL** — Replace the existing compilation entry point with the new `compile_component` + `CompCache` setup. Catch `CompileError` at the boundary.

### Phase 8: Standard library (future)

13. **Filesystem lookup in `CompCache`** — Implement loading `.gateo` files from a search path. The inliner already handles them — no compilation logic changes needed.
