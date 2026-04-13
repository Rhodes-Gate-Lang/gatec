#include "compiler/CompileStatement.hpp"
#include "compiler/CompCache.hpp"
#include "compiler/CompileError.hpp"
#include "compiler/statement/stmt_comp_call.hpp"
#include "compiler/statement/stmt_init.hpp"
#include "compiler/statement/stmt_mutation.hpp"
#include "compiler/statement/stmt_return.hpp"

namespace gate {

void compile_statement(const ast::Statement &stmt,
                       SymbolTable &symbols,
                       NodeEmitter &emitter,
                       std::uint32_t parent_component,
                       CompCache &cache) {
  // TODO: Switch to std::visit at some point
  switch (stmt.index()) {
    case 0:
      return stmt_init(std::get<ast::InitAssign>(stmt), symbols, emitter, parent_component);
    case 1:
      return stmt_mutation(std::get<ast::MutAssign>(stmt), symbols, emitter, parent_component);
    case 2:
      return stmt_comp_call(std::get<ast::CompCall>(stmt), symbols, emitter, parent_component, cache);
    case 3:
      return stmt_return(std::get<ast::ReturnStmt>(stmt), symbols, emitter, parent_component);
    default:
      throw CompileError("Invalid statement type"); // TODO: add error for uncaught statement types
  }
}

} // namespace gate
