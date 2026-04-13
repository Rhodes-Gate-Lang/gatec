#pragma once

#include "compiler/GateoAliases.hpp"

namespace gate {


class NodeEmitter {
public:
  explicit NodeEmitter(std::string root_name) {
    obj_ = GateObject{{0, 0}, {ComponentInstance{root_name, 0}}, {}};
  }

  uint32_t emit_node(Node node);
  uint32_t emit_component(std::string name, uint32_t parent);

  Node &node_at(uint32_t index);

  GateObject take();  // move the finished object out (call once)

private:
  GateObject obj_;
};


} // end namespace gate
