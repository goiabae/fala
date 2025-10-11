#ifndef EVALUATOR_HPP
#define EVALUATOR_HPP

#include <concepts>
#include <optional>
#include <span>
#include <utility>

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
	bool boolean, char character, NodeIndex generic_idx
) {
	// clang-format off
	{ ev.eval_application(ctx, func_idx, arg_idxs) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_if_expression(ctx, cond_idx, then_idx, else_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_for_loop(ctx, decl_idx, upto_idx, step_idx, then_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_while_loop(ctx, cond_idx, then_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_let_binding(ctx, decl_idxs, exp_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_variable_declaration(ctx, id_idx, type_idx, exp_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_function_declaration(ctx, id_idx, param_idxs, type_idx, exp_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_assignment(ctx, var_idx, exp_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_string(ctx, string_id) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_indexing(ctx, var_idx, index_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_integer(ctx, integer) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_break(ctx, exp_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_continue(ctx, exp_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_arithmetic(ctx, operation, left_idx, right_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_logical(ctx, operation, left_idx, right_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_negation(ctx, exp_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_comparison(ctx, operation, left_idx, right_idx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_variable(ctx, string_id) } -> std::same_as<std::optional<Value&>>;
	{ ev.eval_boolean(ctx, boolean) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_nil(ctx) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_character(ctx, character) } -> std::same_as<std::pair<Context, Value>>;
	{ ev.eval_type_instance(ctx, generic_idx, arg_idxs) } -> std::same_as<std::pair<Context, Type>>;
	{ ev.eval_type_variable(ctx, string_id) } -> std::same_as<std::pair<Context, Type>>;

	// TODO: right now you cannot resume to the point of throw after handling
	{ ctx.find_variable(string_id) } -> std::same_as<std::optional<Value&>>;
	{ ctx.find_type_variable(string_id) } -> std::same_as<std::optional<Value&>>;
	{ ctx.yielding_effect() } -> std::same_as<std::optional<StrID>>;
	{ ctx.can_handle(string_id) } -> std::same_as<bool>;
	// clang-format on
};

#endif
