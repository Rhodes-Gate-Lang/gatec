#include "compiler/SymbolTable.hpp"
#include "compiler/CompileError.hpp"
#include <cstdint>
#include <string>

namespace gate {


void SymbolTable::bind(const std::string& name, uint32_t node_index) {
  if (symbols_.count(name))
    throw DuplicateSymbolError(name);

  symbols_[name] = node_index;
}


void SymbolTable::rebind(const std::string& name, uint32_t node_index) {
  if (!symbols_.count(name))
    throw UndefinedSymbolError(name);

  symbols_[name] = node_index;
}


uint32_t SymbolTable::resolve(const std::string& name) const {
  if (symbols_.count(name))
    return symbols_.at(name);

  throw UndefinedSymbolError(name);
}


} // end namespace gate
