# V3 TODO List

We are implementing:
**Registers**:
Add a `reg` keyword in front of a variable initialization. Simulation behavior is that it maintains the same output for the whole clock cycle, and then switches to the new input at the end of the clock run

**Splitters**:
Being able to do `a[0:4]` creates a node that outputs the first 4 bits. IMPORTANT: We are doing MOST to LEAST significant bits, so that it reads like an array where `a=0b010011` and `a[0:4] = 0b0100`.

**Mergers**:
Being able to do `{a, b}` which outputs a new width of a.width + b.width. `a=0b0011` and `b=0101` should output `0b00110101`, where a is placed more significant to b

**Literals**:
For right now, I think we should do literals as only a binary format to avoid confusion with ints and idents. They need to be `0b**`, so `0b110100101` would be a valid literal. Somehow, I'd like to build in a feature of auto-expanding to the correct width, but I don't know how that would work.

**Logical Shifts**:
Need to be able to do `var >> INT` and `var << INT` to logically shift, and backfill with 0s. Truncates to the width `var` already has.

## Parser

- Add AST types to expressions
- Add more left recursion in the grammar to support the new types
- Add the actions to support making them into the AST structs

## Compiler

- Add additional cases to `compile_expr()`
- Add additional functions for compiling each new type of node

## Schema

- Once we have the semantics of these fully set in stone, we need to add the additional types to the schema, plus any changes that may need to be made to the node structs.
