#ifndef EVALUATOR_HPP
#define EVALUATOR_HPP

#include <concepts>
#include <optional>
#include <span>

#include "ast.hpp"
#include "str_pool.h"

template<class Evaluator, class Context, class Value, class Type>
concept evaluator = requires(
	Evaluator ev, Context ctx, NodeIndex func_idx, std::span<NodeIndex> arg_idxs,
	NodeIndex cond_idx, NodeIndex then_idx, std::optional<NodeIndex> else_idx,
	NodeIndex decl_idx, NodeIndex upto_idx, std::optional<NodeIndex> step_idx,
	std::span<NodeIndex> decl_idxs, NodeIndex exp_idx, NodeIndex id_idx,
	std::optional<NodeIndex> type_idx, std::span<NodeIndex> param_idxs,
	NodeIndex var_idx, StrID string_id, NodeIndex index_idx, int integer,
	StrID effect_id, NodeIndex left_idx, NodeIndex right_idx, char operation,
	bool boolean, char character, NodeIndex generic_idx,
	std::span<NodeIndex> exp_idxs
) {
	// clang-format off
	{ ev.eval_application(func_idx, arg_idxs) } -> std::same_as<Value>;
	{ ev.eval_if_expression(cond_idx, then_idx, else_idx) } -> std::same_as<Value>;
	{ ev.eval_for_loop(decl_idx, upto_idx, step_idx, then_idx) } -> std::same_as<Value>;
	{ ev.eval_while_loop(cond_idx, then_idx) } -> std::same_as<Value>;
	{ ev.eval_let_binding(decl_idxs, exp_idx) } -> std::same_as<Value>;
	{ ev.eval_variable_declaration(id_idx, type_idx, exp_idx) } -> std::same_as<Value>;
	{ ev.eval_function_declaration(id_idx, param_idxs, type_idx, exp_idx) } -> std::same_as<Value>;
	{ ev.eval_assignment(var_idx, exp_idx) } -> std::same_as<Value>;
	{ ev.eval_string(string_id) } -> std::same_as<Value>;
	{ ev.eval_indexing(var_idx, index_idx) } -> std::same_as<Value>;
	{ ev.eval_integer(integer) } -> std::same_as<Value>;
	{ ev.eval_break(exp_idx) } -> std::same_as<Value>;
	{ ev.eval_continue(exp_idx) } -> std::same_as<Value>;
	{ ev.eval_arithmetic(operation, left_idx, right_idx) } -> std::same_as<Value>;
	{ ev.eval_logical(operation, left_idx, right_idx) } -> std::same_as<Value>;
	{ ev.eval_negation(exp_idx) } -> std::same_as<Value>;
	{ ev.eval_comparison(operation, left_idx, right_idx) } -> std::same_as<Value>;
	{ ev.eval_variable(string_id) } -> std::same_as<std::optional<std::reference_wrapper<Value>>>;
	{ ev.eval_boolean(boolean) } -> std::same_as<Value>;
	{ ev.eval_nil() } -> std::same_as<Value>;
	{ ev.eval_character(character) } -> std::same_as<Value>;
	{ ev.eval_type_instance(generic_idx, arg_idxs) } -> std::same_as<Type>;
	{ ev.eval_type_variable(string_id) } -> std::same_as<Type>;
	{ ev.eval_block(exp_idxs) } -> std::same_as<Value>;

	{ ev.find_variable(string_id) } -> std::same_as<std::optional<std::reference_wrapper<Value>>>;
	{ ev.find_type_variable(string_id) } -> std::same_as<std::optional<std::reference_wrapper<Type>>>;
	{ ev.yielding_effect() } -> std::same_as<std::optional<StrID>>;
	{ ev.can_handle(string_id) } -> std::same_as<bool>;
	{ ev.enter_new_scope() } -> std::same_as<void>;
	{ ev.close_current_scope() } -> std::same_as<void>;
	// clang-format on
};

#endif
