#ifndef HIR_COMPILER_HPP
#define HIR_COMPILER_HPP

#include "ast.hpp"
#include "env.hpp"
#include "hir.hpp"
#include "logger.hpp"
#include "str_pool.h"
#include "typecheck.hpp"

namespace hir_compiler {
struct Handler {
	hir::Label label;
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

struct Context {
	SignalHandlers handlers;
	Env<hir::Operand>::ScopeID scope_id;
	hir::Module& current_module;
};

class Compiler {
 public:
	Compiler(const AST& ast, const StringPool& pool, Typechecker& checker)
	: ast(ast),
		pool(pool),
		checker(checker),
		logger("HIR_COMPILER", ast.file_name, ast.lines) {}

	hir::Module compile();

	Result compile(NodeIndex node_idx, Context ctx);
	Result compile_lvalue(NodeIndex node_idx, Context ctx);

#define DECLARE_NODE_HANDLER(NODE_TYPE) \
	Result compile_##NODE_TYPE(NodeIndex node_idx, Context ctx)

	DECLARE_NODE_HANDLER(app);
	DECLARE_NODE_HANDLER(if);

#undef DECLARE_NODE_HANDLER

	hir::Register make_register();
	hir::Label make_label();
	hir::Register make_variable(std::string name);

 private:
	const AST& ast;
	const StringPool& pool;
	Typechecker& checker;
	Logger logger;
	Env<hir::Operand> env;
	size_t register_count;
	size_t label_count;
};
} // namespace hir_compiler

#endif
