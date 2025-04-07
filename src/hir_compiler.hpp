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
	hir::Code compile(AST& ast, const StringPool& pool);

	Result compile(
		AST& ast, NodeIndex node_idx, const StringPool& pool,
		SignalHandlers handlers, Env<hir::Operand>::ScopeID scope_id
	);

	Result get_pointer_for(
		AST& ast, NodeIndex node_idx, const StringPool& pool,
		SignalHandlers handlers, Env<hir::Operand>::ScopeID scope_id
	);

	Result to_rvalue(Result pointer_result);

#define DECLARE_NODE_HANDLER(NODE_TYPE) \
	Result compile_##NODE_TYPE(           \
		AST& ast,                           \
		NodeIndex node_idx,                 \
		const StringPool& pool,             \
		SignalHandlers handlers,            \
		Env<hir::Operand>::ScopeID scope_id \
	)

	DECLARE_NODE_HANDLER(app);
	DECLARE_NODE_HANDLER(if);

#undef DECLARE_NODE_HANDLER

	hir::Register make_register();

	Env<hir::Operand> env;

 private:
	size_t register_count;
	size_t label_count;
};
} // namespace hir_compiler

#endif
