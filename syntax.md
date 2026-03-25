# GateLang Language Specification
**Version 0.1 — Draft**

---

## Overview

GateLang is a hardware description language for defining digital logic circuits using primitive boolean operations and composable components. Files use the `.gate` extension.

---

## 1. Comments

Any text following `//` on a line is treated as a comment and ignored by the parser.

```gate
// This is a full-line comment
sum:1 = a XOR b;  // This is an inline comment
```

---

## 2. Data Types & Bus Widths

All signals in GateLang carry an explicit **bit width**, declared using a colon suffix: `name:width`.

- A width of `1` represents a single wire.
- A width greater than `1` represents a **bus** (a multi-bit signal).

```gate
flag:1      // single-bit signal
data:8      // 8-bit bus (byte)
result:32   // 32-bit bus
```

---

## 3. Primitive Operations

GateLang provides four base boolean operations. All other logic is derived from these.

| Operation | Syntax | Description |
|-----------|--------|-------------|
| AND | `a AND b` | Logical conjunction |
| OR | `a OR b` | Logical disjunction |
| XOR | `a XOR b` | Exclusive or |
| NOT | `NOT a` | Logical complement (unary) |

Compound operations are expressed by combining primitives:

```gate
xor-result:1  = a XOR b;
not-result:1  = NOT a;
and-result:1  = a AND b;
or-result:1   = a OR b;

// Derived operations
nand-result:1 = NOT (a AND b);
nor-result:1  = NOT (a OR b);
xnor-result:1 = NOT (a XOR b);
```

---

## 4. Signal Assignment

Signals are assigned using `=`. The left-hand side declares the output name and bit width; the right-hand side is an expression.

Statements are terminated with a semicolon `;`.

```gate
carry:1 = a AND b;
```

---

## 5. Bus Management

### 5.1 Bus Slicing

A sub-range of bits can be extracted from a bus using bracket notation `[low: high]` (inclusive on both ends, zero-indexed).

```gate
high:4 = value[4:7];   // extract bits 4 through 7
low:4  = value[0:3];   // extract bits 0 through 3
```

### 5.2 Bus Concatenation

Multiple signals can be merged into a wider bus using curly-brace notation `{a, b}`. Signals are concatenated in order from most-significant to least-significant.

```gate
merged:8 = {high, low};
```

---

## 6. Components

Components are reusable, named logic blocks. They accept input signals and produce output signals.

### 6.1 Component Declaration

```
comp ComponentName(input1:width, input2:width, ...) {
    // body
    return output1, output2, ...;
}
```

- Component names should be **PascalCase**.
- All input and output signals must have explicit bit widths.
- The `return` statement declares which signals are outputs of the component.

```gate
comp HalfAdder(a:1, b:1) {
    sum:1   = a XOR b;
    carry:1 = a AND b;

    return sum, carry;
}
```

### 6.2 Component Instantiation

A component is instantiated by calling it by name. Outputs are captured on the left-hand side using a comma-separated list. Output signal names must include their bit widths.

```
output1:width, output2:width = ComponentName(input1, input2);
```

To avoid naming conflicts when instantiating the same component multiple times, output signals should be prefixed with an instance identifier.

```gate
// First instance of HalfAdder
ha1-sum:1, ha1-carry:1 = HalfAdder(a, b);

// Second instance, using output from the first
sum:1, ha2-carry:1 = HalfAdder(ha1-sum, cin);
```

### 6.3 Full Example — Full Adder

```gate
comp FullAdder(a:1, b:1, cin:1) {
    // Instantiate first HalfAdder
    ha1-sum:1, ha1-carry:1 = HalfAdder(a, b);

    // Instantiate second HalfAdder using output from the first
    sum:1, ha2-carry:1 = HalfAdder(ha1-sum, cin);

    // Combine carries
    cout:1 = ha1-carry OR ha2-carry;

    return sum, cout;
}
```

---

## 7. File Imports

External `.gate` files can be imported using the `import` keyword. Imported files are resolved relative to the current file's location. All components defined in the imported file become available in the importing file.

```gate
import "halfadder.gate"
import "utils/mux.gate"
```

---

## 8. Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Component names | PascalCase | `FullAdder`, `Mux4To1` |
| Signal names | lowercase with hyphens | `ha1-carry`, `cin`, `result` |
| Instance-scoped signals | `instanceId-signalName` | `ha1-sum`, `ha2-carry` |

---

## 9. Grammar Summary (Informal)

```
program       ::= (import_stmt | component)*

import_stmt   ::= "import" STRING_LITERAL

component     ::= "comp" IDENTIFIER "(" param_list ")" "{" body "}"

param_list    ::= typed_name ("," typed_name)*
typed_name    ::= IDENTIFIER ":" INT

body          ::= statement* return_stmt

statement     ::= assignment ";"
assignment    ::= typed_name "=" expression

return_stmt   ::= "return" IDENTIFIER ("," IDENTIFIER)* ";"

expression    ::= unary_expr | binary_expr | instantiation | bus_expr

unary_expr    ::= "NOT" expression
binary_expr   ::= expression ("AND" | "OR" | "XOR") expression
instantiation ::= IDENTIFIER "(" arg_list ")"
bus_expr      ::= "{" IDENTIFIER ("," IDENTIFIER)* "}"
              |   IDENTIFIER "[" INT "," INT "]"
```
