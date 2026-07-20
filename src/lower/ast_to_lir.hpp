#ifndef FALA_COMPILER_HPP
#define FALA_COMPILER_HPP

#include "ast.hpp"
#include "env.hpp"
#include "lir/lir.hpp"
#include "str_pool.h"
#include "typecheck.hpp"

namespace compiler {

struct Result {
	lir::Chunk code;
	lir::Operand opnd;
};

struct Handler {
	lir::Label destination_label;
	lir::Operand result_register;
};

// They are called handlers, but are really just labels
struct SignalHandlers {
	Handler continue_handler {};
	Handler break_handler {};
	Handler return_handler {};
	bool has_continue_handler {false};
	bool has_break_handler {false};
	bool has_return_handler {false};
};

struct Context {
	SignalHandlers handlers;
	Env<lir::Operand>::ScopeID scope_id;
};

struct Compiler {
	Compiler(const AST& ast, const StringPool& pool, const Typechecker& tpc)
	: ast(ast), pool(pool), tpc(tpc) {}

	lir::Chunk compile();
	Result compile(NodeIndex node_idx, Context ctx);
	Result compile_lvalue(NodeIndex node_idx, Context ctx);
	Result compile_or_allocate_lvalue(NodeIndex node_idx, Context ctx);
	lir::Operand make_register();
	lir::Label make_label();

#define DECLARE_NODE_HANDLER(NAME) \
	Result compile_##NAME(NodeIndex node_idx, Context ctx)

	DECLARE_NODE_HANDLER(app);
	DECLARE_NODE_HANDLER(if);
	DECLARE_NODE_HANDLER(for);
	DECLARE_NODE_HANDLER(when);
	DECLARE_NODE_HANDLER(while);
	DECLARE_NODE_HANDLER(let);
	DECLARE_NODE_HANDLER(var_decl);
	DECLARE_NODE_HANDLER(fun_decl);
	DECLARE_NODE_HANDLER(ass);
	DECLARE_NODE_HANDLER(str);
	DECLARE_NODE_HANDLER(at);
	DECLARE_NODE_HANDLER(block);
	DECLARE_NODE_HANDLER(continue);
	DECLARE_NODE_HANDLER(break);

#undef DECLARE_NODE_HANDLER

 public:
	const AST& ast;
	const StringPool& pool;
	const Typechecker& tpc;
	std::size_t label_count {0};
	std::size_t reg_count {0};
	Env<lir::Operand> env {};
	vector<lir::Chunk> functions {};
};

} // namespace compiler

#endif
