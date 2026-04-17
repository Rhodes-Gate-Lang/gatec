#include "compiler/CompileComponent.hpp"
#include "compiler/CompCache.hpp"
#include "compiler/CompileStatement.hpp"
#include "compiler/NodeEmitter.hpp"
#include "compiler/SymbolTable.hpp"

#include "gateo/io.hpp"

namespace gate {

GateObject compile_component(const ast::Comp &comp, CompCache &cache) {
  // Setup
  NodeEmitter emitter = NodeEmitter(comp.name);
  SymbolTable symbols;

  // Process the inputs
  for (const ast::VarInit& param : comp.params) {
    Node input_node;
    input_node.type = GateType::Input;
    input_node.inputs = {}; // Nothing goes into a root input
    input_node.width = param.width;
    input_node.parent = 0; // root comp
    input_node.name = param.ident;
    input_node.literal_value = std::nullopt; // Not applicable for input nodes

    uint32_t ind = emitter.emit_node(input_node);
    symbols.bind(param.ident, ind);
  }

  // Process the body
  for (const ast::Statement& stmt : comp.body)
    compile_statement(stmt, symbols, emitter, 0, cache);

  // The parser grammar guarantees that there will be a return statement, and that its the last in the comp

  GateObject obj = emitter.take();
  obj.version.major = gateo::supported_schema_major;
  obj.version.minor = 0;
  return obj;
}

} // namespace gate
