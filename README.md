# Gate-Lang
A simple hardware description language to virtually pipeline base logic gates together to make higher level components

## Design
1. Parsing
Parsed into an Abstract Syntax Tree using the [cpp-peglib](https://github.com/yhirose/cpp-peglib) library
2. Compiling
The brunt of the project is in taking this AST and generating a Directed Acyclic Graph of gates mapping the inputs to the outputs
3. Simulating
The simulator uses Kahn's algorithm to navigate the DAG to evaluate outputs

## Syntax Overview
TODO

## Project Milestones
### v0.1.0 alpha
Project setup, design, and parsing
### v0.2.0 beta
Full Compiling stage turning the AST->DAG
### v1.0.0 MVP release
Simulator and main REPL loop to interact
### v2.0.0 Sequential Logic and Polish
Introducing some form of register primitives to allow memory instead of purely combinational logic, in addition to polishing QOL features we discover during initial development.
