# Compiler & Simulation Design

## Overview

The pipeline is: **Source → AST (parser) → DAG (compiler) → Results (simulation)**.

The compiler takes a parsed `ast::Program` and, on demand, compiles any named
component into a flat DAG of primitive gate nodes. The simulation layer evaluates
a compiled DAG given concrete input values.

---

## Data Structures

### Node

A single primitive gate in the circuit.

```cpp
enum class GateType { Input, And, Or, Xor, Not };

struct Node {
    GateType type;
    std::vector<size_t> inputs;  // indices into DAG::nodes (0 for Input, 1 for Not, 2 for binary)
    int width;                   // output bus width in bits
    uint64_t val = 0;           // simulation scratch — holds the node's current output
};
```

- **Input nodes** have an empty `inputs` vector. Their `val` is set externally before evaluation.
- **NOT** has 1 entry in `inputs`.
- **AND / OR / XOR** have 2 entries in `inputs`.
- `width` is set at compile time based on the operands. All inputs to a binary gate must
  share the same width; the output width matches.
- After each gate evaluation, mask `val` with `(1ULL << width) - 1` to clear stray upper bits.

### DAG

The compiled circuit for one component, fully flattened (no sub-component references).

```cpp
struct DAG {
    std::vector<Node> nodes;

    size_t num_inputs;
    // Input nodes are always nodes[0..num_inputs). This is a convention
    // maintained by the compiler — it creates input nodes first.

    std::vector<std::pair<std::string, size_t>> outputs;
    // Each entry is (signal_name, node_index). Populated from the component's
    // return statement. Names are kept for REPL display ("sum = 1, cout = 0").

    std::vector<size_t> topo_order;
    // Filled once after compilation by running Kahn's algorithm.
    // eval() walks this in order, skipping input nodes.

    size_t add_node(GateType type, std::vector<size_t> inputs, int width);
    // Appends a node and returns its index.

    std::vector<uint64_t> operator()(const std::vector<uint64_t>& inputs);
    // Set input node values, walk topo_order evaluating each gate, return output values.
};
```

### ComponentRegistry

Lookup table from component name to its AST definition. Built once from the
parsed program (including imports). Used during compilation to inline component calls.

```cpp
using ComponentRegistry = std::unordered_map<std::string, ast::Comp>;
```

### SymbolTable

Maps signal names to (node_index, width) within a single compilation scope.
Each component body and each inlined call gets its own symbol table.

```cpp
struct Signal {
    size_t node;
    int width;
};
using SymbolTable = std::unordered_map<std::string, Signal>;
```

---

## Compilation (AST → DAG)

### Entry Point

```cpp
DAG compile_component(const std::string& name,
                      const ComponentRegistry& registry,
                      std::string* error_out);
```

Looks up `name` in the registry, creates a fresh DAG, and calls `compile_body`.

### compile_body

```cpp
void compile_body(const ast::Comp& comp,
                  const std::vector<size_t>& arg_nodes,
                  DAG& dag,
                  SymbolTable& symtab,
                  const ComponentRegistry& registry,
                  std::string* error_out);
```

This is the core recursive function. It processes one component's body:

1. **Bind parameters:** For each formal parameter, map its name → the corresponding
   entry in `arg_nodes` in the symbol table. (For top-level calls, `arg_nodes` are the
   input nodes. For inlined calls, they're the caller's argument node indices.)
2. **Walk statements** in order, dispatching on variant type.
3. This function is called both from `compile_component` (top-level) and from
   itself when inlining a `CompCall`.

### Statement Handling

**InitAssign** (`sum:1 = a XOR b;`)
1. Call `compile_expr` on the RHS → get `(node_index, inferred_width)`.
2. Check `inferred_width == target.width`. Report error on mismatch.
3. Add `target.ident → Signal{node_index, width}` to the symbol table.

**MutAssign** (`flag = NOT flag;`)
1. Look up the existing signal to get its declared width.
2. Call `compile_expr` on the RHS → get `(node_index, inferred_width)`.
3. Check widths match.
4. Update the symbol table entry to point to the new node index.

**CompCall** (`ha1_sum:1, ha1_carry:1 = HalfAdder(a, b);`)
1. Look up the called component's AST in the `ComponentRegistry`.
2. Resolve each argument name in the caller's symbol table to get the actual node indices.
3. Validate argument count matches parameter count. Validate widths match.
4. Create a **fresh** symbol table for the callee.
5. Call `compile_body` recursively with the callee's AST, passing the argument
   node indices. This adds the callee's gates directly into the same DAG.
6. After `compile_body` returns, the callee's return statement will have been
   processed. Collect the output node indices from the callee's symbol table
   (via the callee's return names).
7. Bind each output name from the `CompCall` into the **caller's** symbol table.

**ReturnStmt** (`return sum, cout;`)
1. For each name, look up its node index in the symbol table.
2. Push `(name, node_index)` into `dag.outputs`.

### compile_expr

```cpp
Signal compile_expr(const ast::Expr& expr,
                    DAG& dag,
                    const SymbolTable& symtab,
                    std::string* error_out);
```

Recursive. Returns the node index and inferred width for the expression.

- **Identifier:** Look up in symbol table. Return its `Signal` as-is (no new node).
- **UnaryExpr (NOT):** Compile the operand recursively. Create a NOT node
  with the operand's index. Width = operand's width.
- **BinExpr (AND/OR/XOR):** Compile LHS and RHS recursively. Check their
  widths match — error if not. Create the gate node. Width = shared input width.

---

## Simulation (DAG → Results)

### Topological Sort (Kahn's Algorithm)

Run once after compilation to populate `dag.topo_order`.

1. Compute in-degree for every node.
2. Seed queue with all zero-in-degree nodes (the input nodes).
3. Process: dequeue a node, append to `topo_order`, decrement in-degree
   of all nodes that use it as an input. Enqueue any that reach zero.
4. If `topo_order.size() != nodes.size()`, there's a cycle — should never
   happen if compilation is correct.

### eval (operator())

Called for each `run` command in the REPL.

1. Set `nodes[i].val` for each input node `i` from the input arguments.
2. Walk `topo_order`. For each non-input node, read its input nodes' `val`s,
   compute the gate operation, mask to width, store in `node.val`.
3. Collect output values from `dag.outputs` indices.
4. Return as `vector<uint64_t>` (or named pairs for display).

---

## Caching

The REPL maintains a `map<string, DAG>` of already-compiled components.
On `run FullAdder(1, 1, 0)`:

1. Check cache for `"FullAdder"`. If miss, call `compile_component`.
2. Topological sort is already done (stored in the DAG).
3. Call `dag({1, 1, 0})`.
4. Display named outputs.

Invalidate cache on source file reload.

---

## Error Checking (during compilation)

All of these are checked during the AST → DAG pass:

- **Undefined signal:** Identifier not found in symbol table.
- **Undefined component:** CompCall references name not in the registry.
- **Argument count mismatch:** CompCall arg count ≠ component param count.
- **Width mismatch:** Binary gate operands differ in width, or assigned width
  doesn't match expression width, or CompCall arg/param widths differ.
- **Duplicate signal name:** InitAssign with a name already in the symbol table.

---

## File Layout

| File | Responsibility |
|------|---------------|
| `include/DAG.hpp` | `GateType`, `Node`, `Signal`, `DAG` struct definitions |
| `include/Compiler.hpp` | `ComponentRegistry`, `compile_component` declaration |
| `src/Compiler.cpp` | `compile_component`, `compile_body`, `compile_expr`, statement handlers |
| `include/Simulation.hpp` | Topological sort, `eval` / `operator()` declarations |
| `src/Simulation.cpp` | Kahn's algorithm, gate evaluation loop |

---

## Implementation Checklist

Each item matches a `// TODO(n):` comment in the source. Recommended order is
top-to-bottom — each step builds on the ones above it.

### Compiler (`src/Compiler.cpp`)

- [ ] **TODO(1) — `compile_expr`:** Implement `std::visit` over `expr.data`.
  - `std::string` → look up identifier in symtab, error if missing, return Signal.
  - `ast::UnaryExpr` → compile operand recursively, create NOT node, return Signal.
  - `ast::BinExpr` → compile LHS & RHS, check widths match, map `ast::BinOp` to
    `GateType`, create gate node, return Signal.

- [ ] **TODO(2) — `compile_body` parameter binding:** Loop over `comp.params`,
  insert each `param.ident → Signal{arg_nodes[i], param.width}` into the symbol
  table. Validate `arg_nodes.size() == comp.params.size()`.

- [ ] **TODO(3) — `compile_body` statement dispatch:** Use `std::visit` on each
  statement in `comp.body`:
  - `InitAssign` → compile RHS expr, check width vs. target, insert into symtab
    (error on duplicate).
  - `MutAssign` → look up existing signal, compile RHS, check widths, update symtab.
  - `CompCall` → look up component AST in registry, resolve arg nodes from symtab,
    validate counts/widths, call `compile_body` recursively with fresh symtab,
    bind returned node indices as the call's output names in the caller's symtab.
  - `ReturnStmt` → look up each name in symtab, push `(name, node_index)` into
    `dag.outputs` (top-level) or collect into `return_nodes` (inlined).

### Simulation (`src/Simulation.cpp`)

- [ ] **TODO(4) — `topological_sort`:** Kahn's algorithm.
  - Build consumers adjacency list (for each node, who reads from it?).
  - Set `in_degree[i] = nodes[i].inputs.size()`.
  - Seed queue with all nodes where `in_degree == 0`.
  - Process queue: dequeue → append to `topo_order` → decrement consumers'
    in-degree → enqueue any that hit zero.
  - Assert `topo_order.size() == nodes.size()`.

- [ ] **TODO(5) — `operator()` (eval):** Evaluate the circuit.
  - Set input node values: `nodes[i].val = inputs[i] & mask` for `i < num_inputs`.
  - Walk `topo_order`, skip Input nodes, compute gate output from input node vals.
  - Mask each result: `val &= (1ULL << width) - 1`.
  - Collect `nodes[idx].val` for each output index, return as `vector<uint64_t>`.

- [ ] **TODO(6) — `format_outputs`:** Zip `dag.outputs` names with `results` values.
  Return `vector<string>` like `"sum = 1"`, `"cout = 0"`.
