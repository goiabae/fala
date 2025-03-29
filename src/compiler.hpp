#ifndef FALA_COMPILER_HPP
#define FALA_COMPILER_HPP

#include <stddef.h>
#include <stdio.h>

#include <stack>
#include <string>
#include <vector>

#include "ast.hpp"
#include "env.hpp"
#include "lir.hpp"
#include "str_pool.h"

namespace compiler {

using std::stack;
using std::string;
using std::vector;

using lir::Chunk;
using lir::Operand;

struct Result {
	enum class Signal {
		NOSIGNAL, // FIXME: remove this
		BREAK,
		CONTINUE,
		RETURN,
		EXCEPTION,
	};
	Chunk code;
	Operand opnd;
	Signal signal {Signal::NOSIGNAL};
	bool has_signal {false};
};

struct Handler {
	lir::Label destination_label;
	Operand result_register;
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

struct Compiler {
	Chunk compile(AST& ast, const StringPool& pool);

	Result compile(
		AST& ast, NodeIndex node_idx, const StringPool& pool,
		SignalHandlers handlers, Env<Operand>::ScopeID scope_id
	);

	// these are monotonically increasing as the compiler goes on
	size_t label_count {0};
	size_t tmp_count {0};
	size_t reg_count {0};

	Operand make_temporary();
	Operand make_register();
	Operand make_label();

	Operand to_rvalue(Chunk* chunk, Operand opnd);

	// used to backpatch the location of the dynamic allocation region start
	Number dyn_alloc_start {2047};

	Env<Operand> env;

	vector<Chunk> functions;

#define DECLARE_NODE_HANDLER(NAME) \
	Result compile_##NAME(           \
		AST& ast,                      \
		NodeIndex node_idx,            \
		const StringPool& pool,        \
		SignalHandlers handlers,       \
		Env<Operand>::ScopeID scope_id \
	)

	DECLARE_NODE_HANDLER(app);
	DECLARE_NODE_HANDLER(if);
	DECLARE_NODE_HANDLER(for);
	DECLARE_NODE_HANDLER(when);
	DECLARE_NODE_HANDLER(while);
	DECLARE_NODE_HANDLER(let);
	DECLARE_NODE_HANDLER(decl);
	DECLARE_NODE_HANDLER(ass);
	DECLARE_NODE_HANDLER(str);
	DECLARE_NODE_HANDLER(at);
};

} // namespace compiler

#endif
