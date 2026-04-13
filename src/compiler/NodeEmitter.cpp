#include "compiler/NodeEmitter.hpp"
#include "compiler/GateoAliases.hpp"

namespace gate {


uint32_t NodeEmitter::emit_node(Node node) {
  obj_.nodes.push_back(node);
  return obj_.nodes.size() - 1;
}


uint32_t NodeEmitter::emit_component(std::string name, uint32_t parent) {
  obj_.components.push_back(ComponentInstance{name, parent});
  return obj_.components.size() - 1;
}


Node &NodeEmitter::node_at(uint32_t index) {
  return obj_.nodes.at(index);
}


GateObject NodeEmitter::take() {
  return std::move(obj_);
}


} // end namespace gate
