#ifndef FALA_EVAL_HPP
#define FALA_EVAL_HPP

#include <stdbool.h>

#include <memory>
#include <optional>
#include <ostream>
#include <variant>

#include "ast.hpp"
#include "env.hpp"
#include "evaluator.hpp"
#include "str_pool.h"

namespace walk {

struct Interpreter;

struct BuiltinFunction;

struct CustomFunction {
	std::vector<NodeIndex> param_idxs;
	NodeIndex body_idx;
};

struct Array;

struct Nil {};

using Value = std::variant<
	BuiltinFunction, CustomFunction, Array, int, bool, std::string, Nil>;

using ValueCell = std::shared_ptr<Value>;

struct Array {
	size_t size;
	std::vector<ValueCell> items;
};

struct BuiltinFunction {
	size_t param_count;
	ValueCell (Interpreter::*builtin)(std::vector<ValueCell>);
};

struct Nothing {};

struct Context {
	Env<ValueCell> env;
	Env<ValueCell>::ScopeID scope_id;

	bool in_loop;
	bool should_break;
	bool should_continue;
};

struct Interpreter {
	Interpreter(
		StringPool& pool, const AST& ast, std::istream& input, std::ostream& output
	);

	StringPool& pool;
	const AST& ast;
	std::istream& input;
	std::ostream& output;

	Context ctx;

	ValueCell eval();

	// clang-format off
	auto eval_application(NodeIndex func_idx, std::span<NodeIndex> arg_idxs) -> ValueCell;
	auto eval_if_expression(NodeIndex cond_idx, NodeIndex then_idx, std::optional<NodeIndex> else_idx) -> ValueCell;
	auto eval_for_loop(NodeIndex decl_idx, NodeIndex upto_idx, std::optional<NodeIndex> step_idx, NodeIndex then_idx) -> ValueCell;
	auto eval_while_loop(NodeIndex cond_idx, NodeIndex then_idx) -> ValueCell;
	auto eval_let_binding(std::span<NodeIndex> decl_idxs, NodeIndex exp_idx) -> ValueCell;
	auto eval_variable_declaration(NodeIndex id_idx, std::optional<NodeIndex> type_idx, NodeIndex exp_idx) -> ValueCell;
	auto eval_function_declaration(NodeIndex id_idx, std::span<NodeIndex> param_idxs, std::optional<NodeIndex> type_idx, NodeIndex exp_idx) -> ValueCell;
	auto eval_assignment(NodeIndex var_idx, NodeIndex exp_idx) -> ValueCell;
	auto eval_string(StrID string_id) -> ValueCell;
	auto eval_indexing(NodeIndex var_idx, NodeIndex index_idx) -> ValueCell;
	auto eval_integer(int integer) -> ValueCell;
	auto eval_break(NodeIndex exp_idx) -> ValueCell;
	auto eval_continue(NodeIndex exp_idx) -> ValueCell;
	auto eval_arithmetic(char operation, NodeIndex left_idx, NodeIndex right_idx) -> ValueCell;
	auto eval_logical(char operation, NodeIndex left_idx, NodeIndex right_idx) -> ValueCell;
	auto eval_negation(NodeIndex exp_idx) -> ValueCell;
	auto eval_comparison(char operation, NodeIndex left_idx, NodeIndex right_idx) -> ValueCell;
	auto eval_variable(StrID string_id) -> std::optional<std::reference_wrapper<ValueCell>>;
	auto eval_boolean(bool boolean) -> ValueCell;
	auto eval_nil() -> ValueCell;
	auto eval_character(char character) -> ValueCell;
	auto eval_type_instance(NodeIndex generic_idx, std::span<NodeIndex> arg_idxs) -> Nothing;
	auto eval_type_variable(StrID string_id) -> Nothing;
	auto eval_block(std::span<NodeIndex> exp_idxs) -> ValueCell;
	// clang-format on

	auto eval_node(NodeIndex node_idx) -> ValueCell;

	auto find_variable(StrID string_id)
		-> std::optional<std::reference_wrapper<ValueCell>>;
	auto find_type_variable(StrID string_id)
		-> std::optional<std::reference_wrapper<Nothing>>;
	auto yielding_effect() -> std::optional<StrID>;
	auto can_handle(StrID string_id) -> bool;
	void enter_new_scope();
	void close_current_scope();

	ValueCell builtin_read(std::vector<ValueCell> args);
	ValueCell builtin_write(std::vector<ValueCell> args);
	ValueCell builtin_array(std::vector<ValueCell> args);
	ValueCell builtin_exit(std::vector<ValueCell> args);

	static_assert(evaluator<Interpreter, Context, ValueCell, Nothing>);
};

std::ostream& operator<<(std::ostream& st, const ValueCell& val);

} // namespace walk

#endif
