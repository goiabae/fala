#ifndef FALA_COMPILER_HPP
#define FALA_COMPILER_HPP

#include <map>

#include "ast.hpp"
#include "env.hpp"
#include "lir/lir.hpp"
#include "str_pool.h"
#include "symbol_table.hpp"
#include "type.hpp"
#include "typecheck.hpp"

namespace compiler {

struct Result {
	lir::Block block;
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

struct ClosureInfo {
	std::optional<Guid> function_name;
	lir::Register object;
	std::map<Guid, std::size_t> offsets;
};

struct Context {
	SignalHandlers handlers;
	std::optional<ClosureInfo> current_closure;
	std::optional<lir::Operand> current_resumption;
	std::vector<lir::Register>& used_registers;
};

struct Compiler {
	Compiler(
		const AST& ast, const StringPool& pool, const Typechecker& tpc,
		const SymbolTable& symtab
	)
	: ast(ast),
		pool(pool),
		tpc(tpc),
		symtab(symtab),
		logger("AST->LIR", ast.file_name, ast.lines) {}

	lir::Chunk compile();
	Result compile(NodeIndex node_idx, Context ctx);
	lir::Block compile_handler_decl(
		NodeIndex node_idx, Context ctx, lir::Label body
	);
	Result compile_lvalue(NodeIndex node_idx, Context ctx);
	Result compile_or_allocate_lvalue(NodeIndex node_idx, Context ctx);
	lir::Block compile_case(
		NodeIndex node_idx, Context ctx, lir::Label exit, lir::Register res,
		std::shared_ptr<Datatype> expr_type, lir::Operand expr
	);
	lir::Block compile_pattern_match(
		NodeIndex node_idx, Context ctx, lir::Label exit,
		std::shared_ptr<Datatype> expr_type, lir::Operand expr
	);
	lir::Register make_register(lir::Type, Context);
	lir::Register alloc_register(lir::Type);
	lir::Label make_label();
	void allocate_variables();
	Result compile_constant(lir::Operand);

	lir::ChunkRef make_chunk();

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
	DECLARE_NODE_HANDLER(perform);
	DECLARE_NODE_HANDLER(match);
	DECLARE_NODE_HANDLER(array_literal);
	DECLARE_NODE_HANDLER(record_literal);

#undef DECLARE_NODE_HANDLER

 public:
	const AST& ast;
	const StringPool& pool;
	const Typechecker& tpc;
	const SymbolTable& symtab;
	Logger logger;
	std::size_t label_count {0};
	std::size_t reg_count {0};
	std::map<Guid, lir::Operand> variables;
	std::map<Guid, lir::Immediate> effect_tags {};
	vector<lir::Chunk> functions {};
	std::vector<lir::Chunk> chunks {};
	lir::Immediate write_str_tag {-1};
	lir::Immediate write_int_tag {-1};
	lir::Immediate write_char_tag {-1};
};

} // namespace compiler

#endif
