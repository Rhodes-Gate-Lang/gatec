#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace gate {

class SymbolTable {
public:
  void bind(const std::string& name, uint32_t node_index);   // throws DuplicateSymbolError
  void rebind(const std::string& name, uint32_t node_index);   // throws UndefinedSymbolError
  uint32_t resolve(const std::string& name) const;           // throws UndefinedSymbolError

private:
  std::unordered_map<std::string, uint32_t> symbols_;
};

} // end namespace gate
