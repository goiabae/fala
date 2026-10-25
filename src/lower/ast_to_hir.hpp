#ifndef AST_TO_HIR_HPP
#define AST_TO_HIR_HPP

#include <optional>

#include "ast.hpp"
#include "hir/hir.hpp"
#include "hir/hir_type.hpp"
#include "indexes.hpp"
#include "logger.hpp"
#include "str_pool.h"
#include "symbol_table.hpp"
#include "typecheck.hpp"

namespace lower::ast_to_hir {

struct LocalVariable {
	hir::Register register_;
};

struct GlobalVariable {
	hir::Label label;
};

struct StaticFunction {
	hir::Label label;
};

struct ClosureFunction {
	hir::Label label;
	hir::Register closure_register;
};

struct EnclosedVariable {
	std::size_t offset;
};

using Variable = std::variant<LocalVariable, StaticFunction>;

struct Handler {
	hir::Label label;
};

struct SignalHandlers {
	Handler continue_handler;
	Handler break_handler;
	Handler return_handler;
	hir::Register resume_handler;
	bool has_continue_handler;
	bool has_break_handler;
	bool has_return_handler;
	bool has_resume_handler;
};

struct Result {
	hir::Code code;
	hir::Register result_register;
};

struct ClosureInfo {
	std::optional<Guid> function_name;
	hir::Register object;
	std::map<Guid, std::size_t> offsets;
};

struct Context {
	SignalHandlers handlers;
	hir::Module& current_module;
	std::optional<ClosureInfo> current_closure;
};

enum class NameKind {
	STATIC_FUNCTION,
	CLOSURE,
	VALUE,
	ENCLOSED_VALUE,
};

class Compiler {
 public:
	Compiler(
		const AST& ast, const StringPool& pool, Typechecker& checker,
		const SymbolTable& symtab
	)
	: ast(ast),
		pool(pool),
		checker(checker),
		symtab(symtab),
		logger("HIR_COMPILER", ast.file_name, ast.lines) {}

	hir::Module compile();
	hir::Module start_module();

	Result compile(NodeIndex node_idx, Context ctx);
	Result compile_lvalue(NodeIndex node_idx, Context ctx);
	auto compile_declaration(NodeIndex node_idx, Context ctx) -> std::optional<
		std::tuple<hir::Code, hir::Operand, std::optional<hir::Code*>>>;

#define DECLARE_NODE_HANDLER(NODE_TYPE) \
	Result compile_##NODE_TYPE(NodeIndex node_idx, Context ctx)

	DECLARE_NODE_HANDLER(app);
	DECLARE_NODE_HANDLER(if);
	DECLARE_NODE_HANDLER(perform);

#undef DECLARE_NODE_HANDLER

	hir::Register make_register(const hir::HTPtr&);
	hir::Label make_label(std::optional<std::string> name = std::nullopt);
	hir::Register make_variable(
		const hir::HTPtr&, std::optional<std::string> name = std::nullopt
	);

	std::optional<Variable> classify_variable(Guid);
	auto type_of(NodeIndex) -> hir::HTPtr;

 public:
	hir::HTContext hctx {};

 private:
	const AST& ast;
	const StringPool& pool;
	Typechecker& checker;
	const SymbolTable& symtab;
	Logger logger;
	size_t register_count;
	size_t label_count;
	std::map<Guid, Variable> variables;
	std::map<Guid, std::size_t> effect_tags {};
};
} // namespace lower::ast_to_hir

#endif
