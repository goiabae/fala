#ifndef HIR_COMPILER_HPP
#define HIR_COMPILER_HPP

#include "ast.hpp"
#include "env.hpp"
#include "hir.hpp"
#include "str_pool.h"
#include "typecheck.hpp"

namespace hir_compiler {
struct Handler {
	hir::Operand result_register;
};

struct SignalHandlers {
	Handler continue_handler {};
	Handler break_handler {};
	Handler return_handler {};
	bool has_continue_handler {false};
	bool has_break_handler {false};
	bool has_return_handler {false};
};

struct Result {
	hir::Code code;
	hir::Register result_register;
};

class Compiler {
 public:
	Compiler(const AST& ast, const StringPool& pool, Typechecker& checker)
	: ast(ast), pool(pool), checker(checker) {}

	hir::Code compile();

	Result compile(
		NodeIndex node_idx, SignalHandlers handlers,
		Env<hir::Operand>::ScopeID scope_id
	);

	bool is_simple_path(NodeIndex node_idx);

	hir::Code find_aggregate_indexes(
		NodeIndex node_idx, hir::Register&, std::vector<hir::Operand>&,
		SignalHandlers handlers, Env<hir::Operand>::ScopeID scope_id
	);

#define DECLARE_NODE_HANDLER(NODE_TYPE) \
	Result compile_##NODE_TYPE(           \
		NodeIndex node_idx,                 \
		SignalHandlers handlers,            \
		Env<hir::Operand>::ScopeID scope_id \
	)

	DECLARE_NODE_HANDLER(app);
	DECLARE_NODE_HANDLER(if);

#undef DECLARE_NODE_HANDLER

	hir::Register make_register();

 private:
	const AST& ast;
	const StringPool& pool;
	Typechecker& checker;
	Env<hir::Operand> env;
	size_t register_count;
	size_t label_count;
};
} // namespace hir_compiler

#endif
