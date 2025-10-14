#include "walk.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <variant>

#include "ast.hpp"
#include "str_pool.h"

namespace walk {

void Interpreter::enter_new_scope() {
	ctx.scope_id = ctx.env.create_child_scope(ctx.scope_id);
}

void Interpreter::close_current_scope() {
	ctx.scope_id = ctx.env.get_parent_scope(ctx.scope_id).value();
}

auto Interpreter::find_variable(StrID string_id)
	-> std::optional<std::reference_wrapper<ValueCell>> {
	auto cell = ctx.env.find(ctx.scope_id, string_id);
	if (cell == nullptr) return {};
	return {*cell};
}

[[noreturn]] static void err(const char* msg) {
	fprintf(stderr, "INTERPRETER ERROR: %s\n", msg);
	exit(1);
}

auto Interpreter::eval_application(
	NodeIndex func_idx, std::span<NodeIndex> arg_idxs
) -> ValueCell {
	const auto& func_node = ast.at(func_idx);

	if (func_node.type != NodeType::ID)
		err("Unnamed functions are not implemented");

	auto maybe_func_ptr = find_variable(func_node.str_id);
	if (not maybe_func_ptr.has_value()) {
		auto msg = "Function with name " + std::string(pool.find(func_node.str_id))
		         + " not found";
		err(msg.c_str());
	}

	ValueCell func_ptr = *maybe_func_ptr;
	assert(func_ptr != nullptr);

	std::vector<ValueCell> args {};
	for (auto arg_idx : arg_idxs) {
		auto arg = eval_node(arg_idx);
		assert(arg != nullptr);
		args.push_back(arg);
	}

	if (std::holds_alternative<BuiltinFunction>(*func_ptr)) {
		auto builtin = std::get<BuiltinFunction>(*func_ptr);
		if (builtin.param_count != args.size()) err("Wrong number of arguments");
		auto val = builtin.builtin(args);
		return val;
	} else if (std::holds_alternative<CustomFunction>(*func_ptr)) {
		auto custom = std::get<CustomFunction>(*func_ptr);
		enter_new_scope();
		for (size_t i = 0; i < args.size(); i++) {
			const auto& param_node = ast.at(custom.param_idxs[i]);
			assert(param_node.type == NodeType::ID);
			auto cell = ctx.env.insert(ctx.scope_id, param_node.str_id, args[i]);
			assert(cell != nullptr);
		}

		auto val = eval_node(custom.body_idx);
		close_current_scope();
		return val;
	} else {
		assert(false);
	}
}

auto Interpreter::eval_block(std::span<NodeIndex> exp_idxs) -> ValueCell {
	auto res = std::make_shared<Value>(Nil {});
	enter_new_scope();
	for (auto exp_idx : exp_idxs) {
		auto val = eval_node(exp_idx);
		res = val;
	}
	close_current_scope();
	return res;
}

auto Interpreter::eval_if_expression(
	NodeIndex cond_idx, NodeIndex then_idx, std::optional<NodeIndex> else_idx
) -> ValueCell {
	const auto cond = eval_node(cond_idx);
	if (std::get<bool>(*cond)) {
		const auto then_val = eval_node(then_idx);
		return then_val;
	} else if (else_idx.has_value()) {
		const auto else_val = eval_node(else_idx.value());
		return else_val;
	} else {
		return std::make_shared<Value>(Nil {});
	}
}

auto Interpreter::eval_for_loop(
	NodeIndex decl_idx, NodeIndex upto_idx, std::optional<NodeIndex> step_idx,
	NodeIndex then_idx
) -> ValueCell {
	enter_new_scope();
	const auto decl = eval_node(decl_idx);
	const auto to = eval_node(upto_idx);

	ValueCell inc {nullptr};
	if (step_idx.has_value()) {
		inc = eval_node(step_idx.value());
	} else {
		inc = std::make_shared<Value>(1);
	}

	if (not std::holds_alternative<int>(*to))
		err("Type of `to' value is not number");

	if (not std::holds_alternative<int>(*inc))
		err("Type of `inc' value is not number");

	int initial = std::get<int>(*decl);
	int upto = std::get<int>(*to);
	int inc_int = std::get<int>(*inc);

	ctx.in_loop = true;
	for (int i = initial; i != upto; i += inc_int) {
		*decl = Value {i};
		eval_node(then_idx);
		if (ctx.should_break) {
			ctx.should_break = false;
			break;
		}
		if (ctx.should_continue) {
			ctx.should_continue = false;
			break;
		}
	}
	ctx.in_loop = false;

	close_current_scope();

	return std::make_shared<Value>(Nil {});
}

auto Interpreter::eval_while_loop(NodeIndex cond_idx, NodeIndex then_idx)
	-> ValueCell {
	ctx.in_loop = true;
	auto res = std::make_shared<Value>(Nil {});
	ValueCell cond {};
	while (true) {
		const auto cond = eval_node(cond_idx);
		if (not std::get<bool>(*cond)) break;
		const auto val = eval_node(then_idx);
		if (ctx.should_break) {
			ctx.should_break = false;
			break;
		}
		if (ctx.should_continue) {
			ctx.should_continue = false;
			continue;
		}
	}

	ctx.in_loop = false;
	return res;
}

ValueCell copy_value(ValueCell value) {
	if (std::holds_alternative<int>(*value)) {
		return std::make_shared<Value>(std::get<int>(*value));
	} else if (std::holds_alternative<Array>(*value)) {
		auto result = std::make_shared<Value>(Array {});
		auto size = std::get<Array>(*value).size;
		std::get<Array>(*result).size = size;
		std::get<Array>(*result).items.reserve(size);
		const auto& items = std::get<Array>(*value).items;
		for (auto i = 0ul; i < size; i++) {
			std::get<Array>(*result).items[i] = copy_value(items[i]);
		}
		return result;
	} else if (std::holds_alternative<bool>(*value)) {
		return std::make_shared<Value>(std::get<bool>(*value));
	} else {
		assert(false && "not implemented yet");
	}
}

auto Interpreter::eval_assignment(NodeIndex var_idx, NodeIndex exp_idx)
	-> ValueCell {
	const auto right = eval_node(exp_idx);
	const auto lvalue = eval_node(var_idx);
	auto copied_right = copy_value(right);
	*lvalue = *copied_right;
	return copied_right;
}

auto Interpreter::eval_variable_declaration(
	NodeIndex id_idx, std::optional<NodeIndex> type_idx, NodeIndex exp_idx
) -> ValueCell {
	const auto& id_node = ast.at(id_idx);
	const auto value = eval_node(exp_idx);
	auto copied_value = copy_value(value);
	auto cell = ctx.env.insert(ctx.scope_id, id_node.str_id, copied_value);
	if (cell == nullptr) err("Could not initialize variable");
	if (*cell == nullptr) err("Value was null");
	return copied_value;
}

auto Interpreter::eval_function_declaration(
	NodeIndex id_idx, std::span<NodeIndex> param_idxs,
	std::optional<NodeIndex> type_idx, NodeIndex exp_idx
) -> ValueCell {
	const auto& id_node = ast.at(id_idx);
	std::vector<NodeIndex> param_idxs_vec {};
	for (auto param_idx : param_idxs) {
		param_idxs_vec.push_back(param_idx);
	}
	auto value =
		std::make_shared<Value>(CustomFunction {param_idxs_vec, exp_idx});
	assert(value != nullptr);
	auto cell = ctx.env.insert(ctx.scope_id, id_node.str_id, value);
	if (!cell) err("Could not initialize variable");
	return value;
}

auto Interpreter::eval_boolean(bool boolean) -> ValueCell {
	return std::make_shared<Value>(boolean);
}

auto Interpreter::eval_nil() -> ValueCell {
	return std::make_shared<Value>(Nil {});
}

auto Interpreter::eval_character(char character) -> ValueCell {
	return std::make_shared<Value>((int)character);
}

auto Interpreter::eval_integer(int integer) -> ValueCell {
	return std::make_shared<Value>(integer);
}

auto Interpreter::eval_break(NodeIndex exp_idx) -> ValueCell {
	if (not ctx.in_loop) err("Can't break outside of a loop");
	auto val = eval_node(exp_idx);
	ctx.should_break = true;
	return val;
}

auto Interpreter::eval_continue(NodeIndex exp_idx) -> ValueCell {
	if (not ctx.in_loop) err("Can't continue outside of a loop");
	auto val = eval_node(exp_idx);
	ctx.should_continue = true;
	return val;
}

auto Interpreter::eval_logical(
	char operation, NodeIndex left_idx, NodeIndex right_idx
) -> ValueCell {
	if (operation == '&') {
		const auto left = eval_node(left_idx);
		if (std::get<bool>(*left)) {
			const auto right = eval_node(right_idx);
			return std::make_shared<Value>(std::get<bool>(*right));
		} else {
			return std::make_shared<Value>(false);
		}
	} else if (operation == '|') {
		const auto left = eval_node(left_idx);
		if (std::get<bool>(*left)) {
			return std::make_shared<Value>(true);
		} else {
			const auto right = eval_node(right_idx);
			return std::make_shared<Value>(std::get<bool>(*right));
		}
	} else {
		err("Unknown logical operator");
	}
}

auto Interpreter::eval_arithmetic(
	char operation, NodeIndex left_idx, NodeIndex right_idx
) -> ValueCell {
	const auto left = eval_node(left_idx);
	const auto right = eval_node(right_idx);
	if (left == nullptr) {
		// ast_print_detailed((AST*)&ast, pool);
		std::cerr << "index: " << left_idx.index << '\n';
		exit(1);
	}
	assert(right != nullptr);
	if (not std::holds_alternative<int>(*left))
		err("Left-hand side of arithmentic operator is not a number");
	if (not std::holds_alternative<int>(*right))
		err("Right-hand side of arithmentic operator is not a number");
	int left_int = std::get<int>(*left);
	int right_int = std::get<int>(*right);
	int result_int = -1;
	if (operation == '+') {
		result_int = left_int + right_int;
	} else if (operation == '-') {
		result_int = left_int - right_int;
	} else if (operation == '*') {
		result_int = left_int * right_int;
	} else if (operation == '/') {
		result_int = left_int / right_int;
	} else if (operation == '%') {
		result_int = left_int % right_int;
	} else {
		err("Unknown arithmetic operator");
	}
	return std::make_shared<Value>(result_int);
}

auto Interpreter::eval_comparison(
	char operation, NodeIndex left_idx, NodeIndex right_idx
) -> ValueCell {
	const auto left = eval_node(left_idx);
	const auto right = eval_node(right_idx);

	if (operation == '=') {
		bool result = false;

		if (std::holds_alternative<Nil>(*left)
		    and std::holds_alternative<Nil>(*right))
			result = true;
		else if (std::holds_alternative<bool>(*left)
		         and std::holds_alternative<bool>(*right))
			result = std::get<bool>(*left) == std::get<bool>(*right);
		else if (std::holds_alternative<int>(*left)
		         and std::holds_alternative<int>(*right))
			result = std::get<int>(*left) == std::get<int>(*right);
		else
			err("Can't compare values for equality");

		return std::make_shared<Value>(result);
	}

	if (not std::holds_alternative<int>(*left)
	    or not std::holds_alternative<int>(*right)) {
		err("Arithmetic comparison is allowed only between numbers");
	}

	int left_int = std::get<int>(*left);
	int right_int = std::get<int>(*right);
	bool result = false;
	if (operation == '>') {
		result = left_int > right_int;
	} else if (operation == '<') {
		result = left_int < right_int;
	} else if (operation == '[') {
		result = left_int <= right_int;
	} else if (operation == ']') {
		result = left_int >= right_int;
	} else {
		err("Unknown comparison operator");
	}

	return std::make_shared<Value>(result);
}

auto Interpreter::eval_negation(NodeIndex exp_idx) -> ValueCell {
	const auto value = eval_node(exp_idx);
	if (not std::holds_alternative<bool>(*value))
		err("not operator expects a boolean value");
	return std::make_shared<Value>(not std::get<bool>(*value));
}

auto Interpreter::eval_indexing(NodeIndex var_idx, NodeIndex index_idx)
	-> ValueCell {
	const auto base = eval_node(var_idx);
	if (not std::holds_alternative<Array>(*base)) err("Can only index arrays");

	const auto off = eval_node(index_idx);
	if (not std::holds_alternative<int>(*off)) err("Index must be a number");

	assert((size_t)std::get<int>(*off) < std::get<Array>(*base).size);

	auto value = std::get<Array>(*base).items[(size_t)std::get<int>(*off)];
	return value;
}

auto Interpreter::eval_variable(StrID string_id)
	-> std::optional<std::reference_wrapper<ValueCell>> {
	auto cell = ctx.env.find(ctx.scope_id, string_id);
	if (cell == nullptr) {
		err("Variable not previously declared.");
		return std::optional<std::reference_wrapper<ValueCell>> {};
	} else if (*cell == nullptr) {
		err("Value of variable was null");
	} else {
		return {*cell};
	}
}

auto Interpreter::eval_string(StrID string_id) -> ValueCell {
	const char* str = pool.find(string_id);
	if (str == nullptr) err("Unknown string");
	return std::make_shared<Value>(std::string(str));
}

auto Interpreter::eval_let_binding(
	std::span<NodeIndex> decl_idxs, NodeIndex exp_idx
) -> ValueCell {
	enter_new_scope();
	for (auto decl_idx : decl_idxs) eval_node(decl_idx);

	const auto res = eval_node(exp_idx);
	close_current_scope();
	return res;
}

auto Interpreter::eval_node(NodeIndex node_idx) -> ValueCell {
	const auto& node = ast.at(node_idx);
	// std::cerr << node.loc.begin.line << ":" << node.loc.begin.column << '\n';
	switch (node.type) {
		case NodeType::NUM: return eval_integer(node.num);
		case NodeType::APP:
			return eval_application(node[0], std::span<NodeIndex>(ast.at(node[1])));
		case NodeType::BLK: return eval_block(std::span<NodeIndex>(node));
		case NodeType::IF: return eval_if_expression(node[0], node[1], {node[2]});
		case NodeType::WHEN: return eval_if_expression(node[0], node[1], {});
		case NodeType::FOR:
			return eval_for_loop(
				node[0],
				node[1],
				(ast.at(node[2]).type == NodeType::EMPTY)
					? std::optional<NodeIndex> {}
					: std::optional<NodeIndex> {node[2]},
				node[3]
			);
		case NodeType::WHILE: return eval_while_loop(node[0], node[1]);
		case NodeType::BREAK: return eval_break(node[0]);
		case NodeType::CONTINUE: return eval_continue(node[0]);
		case NodeType::ASS: return eval_assignment(node[0], node[1]);
		case NodeType::OR: return eval_logical('|', node[0], node[1]);
		case NodeType::AND: return eval_logical('&', node[0], node[1]);
		case NodeType::ADD: return eval_arithmetic('+', node[0], node[1]);
		case NodeType::SUB: return eval_arithmetic('-', node[0], node[1]);
		case NodeType::MUL: return eval_arithmetic('*', node[0], node[1]);
		case NodeType::DIV: return eval_arithmetic('/', node[0], node[1]);
		case NodeType::MOD: return eval_arithmetic('%', node[0], node[1]);
		case NodeType::GTN: return eval_comparison('>', node[0], node[1]);
		case NodeType::LTN: return eval_comparison('<', node[0], node[1]);
		case NodeType::GTE: return eval_comparison(']', node[0], node[1]);
		case NodeType::LTE: return eval_comparison('[', node[0], node[1]);
		case NodeType::EQ: return eval_comparison('=', node[0], node[1]);
		case NodeType::NOT: return eval_negation(node[0]);
		case NodeType::AT: return eval_indexing(node[0], node[1]);
		case NodeType::ID: {
			auto maybe_value = eval_variable(node.str_id);
			if (not maybe_value.has_value()) err("Variable not found");
			ValueCell val = maybe_value.value();
			if (val == nullptr) err("Variable was null");
			return val;
		}
		case NodeType::STR: return eval_string(node.str_id);
		case NodeType::VAR_DECL:
			return eval_variable_declaration(
				node[0],
				(ast.at(node[1]).type == NodeType::EMPTY) ? std::optional<NodeIndex> {}
																									: node[1],
				node[2]
			);
		case NodeType::FUN_DECL:
			return eval_function_declaration(
				node[0],
				std::span<NodeIndex>(ast.at(node[1])),
				(ast.at(node[2]).type == NodeType::EMPTY) ? std::optional<NodeIndex> {}
																									: node[2],
				node[3]
			);
		case NodeType::NIL: return eval_nil();
		case NodeType::TRUE: return eval_boolean(true);
		case NodeType::FALSE: return eval_boolean(false);
		case NodeType::LET:
			return eval_let_binding(std::span<NodeIndex>(ast.at(node[0])), node[1]);
		case NodeType::EMPTY: assert(false && "unreachable");
		case NodeType::CHAR: return eval_character(node.character);
		case NodeType::PATH: return eval_node(node[0]);
		case NodeType::INSTANCE:
			assert(false && "used only in typechecking. should not be evaluated");
		case NodeType::AS: return eval_node(node[0]);
	}
	assert(false);
}

void print_value(ValueCell val) {
	if (std::holds_alternative<int>(*val)) {
		printf("%d", std::get<int>(*val));
	} else if (std::holds_alternative<std::string>(*val)) {
		printf("%s", std::get<std::string>(*val).c_str());
	} else if (std::holds_alternative<bool>(*val)) {
		if (std::get<bool>(*val))
			printf("1");
		else
			printf("0");
	} else {
		err("Can't print value");
	}
}

std::ostream& operator<<(std::ostream& st, ValueCell& val) { print_value(val); }

ValueCell builtin_read(std::vector<ValueCell>) {
	char* buf = (char*)malloc(sizeof(char) * 100);
	if (!buf) err("Could not allocate input buffer for `read' built-in");
	if (fgets(buf, 100, stdin) == NULL) {
		free(buf);
		assert(false);
		return {};
	}
	const size_t len = strlen(buf);
	buf[len - 1] = '\0';

	// try to parse as number
	Number num = 0;
	if (sscanf(buf, "%d", &num) != 0) {
		free(buf);
		return std::make_shared<Value>(num);
	}

	// otherwise return as string
	return std::make_shared<Value>(std::string(buf));
}

ValueCell builtin_write(std::vector<ValueCell> args) {
	for (size_t i = 0; i < args.size(); i++) print_value(args[i]);
	return std::make_shared<Value>(Nil {});
}

ValueCell builtin_array(std::vector<ValueCell> args) {
	if (args.size() != 1) err("Expected a single numeric argument");
	if (args[0] == nullptr) err("Expected a single numeric argument");
	if (not std::holds_alternative<int>(*args[0]))
		err("Expected a single numeric argument");
	auto arr_len = std::get<int>(*args[0]);
	if (arr_len < 0) err("Array length must be positive");

	ValueCell val = std::make_shared<Value>(Array {
		(size_t)arr_len,
		std::vector<ValueCell>((size_t)arr_len, std::make_shared<Value>(0))
	});
	assert(val != nullptr);
	return val;
}

ValueCell builtin_exit(std::vector<ValueCell> args) {
	if (args.size() != 1) err("exit takes exit code as a argument");
	Number exit_code = std::get<int>(*args[0]);
	exit(exit_code);
	return {};
}

Interpreter::Interpreter(StringPool& pool, const AST& ast)
: pool {pool}, ast {ast} {}

#define PUSH_BUILTIN(STR, FUNC, COUNT)                      \
	*ctx.env.insert(ctx.scope_id, pool.intern(strdup(STR))) = \
		std::make_shared<Value>(BuiltinFunction(COUNT, FUNC))

ValueCell Interpreter::eval() {
	ctx.scope_id = ctx.env.root_scope_id;
	PUSH_BUILTIN("read_int", builtin_read, 1);
	PUSH_BUILTIN("write_int", builtin_write, 1);
	PUSH_BUILTIN("write_str", builtin_write, 1);
	PUSH_BUILTIN("make_array", builtin_array, 1);
	PUSH_BUILTIN("exit", builtin_exit, 1);
	const auto val = eval_node(ast.root_index);
	return val;
}

} // namespace walk
