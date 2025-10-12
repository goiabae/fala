#include "walk.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <memory>
#include <variant>

#include "ast.hpp"
#include "str_pool.h"

namespace walk {

ValueCell inter_eval_node(
	Interpreter* inter, AST& ast, NodeIndex node_idx,
	Env<ValueCell>::ScopeID scope_id
);

static void err(const char* msg) {
	fprintf(stderr, "INTERPRETER ERROR: %s\n", msg);
	exit(1);
}

static void err2(Location loc, const char* msg) {
	fprintf(
		stderr,
		"INTERPRETER ERROR(%d, %d, %d): %s\n",
		loc.begin.line + 1,
		loc.begin.column + 1,
		loc.end.column + 1,
		msg
	);
	exit(1);
}

ValueCell Interpreter::eval(AST& ast) {
	return inter_eval_node(this, ast, ast.root_index, env.root_scope_id);
}

// term term+
ValueCell eval_app(
	Interpreter* inter, AST& ast, const Node& node,
	Env<ValueCell>::ScopeID scope_id
) {
	auto func_idx = node[0];
	auto args_idx = node[1];

	const auto& func_node = ast.at(func_idx);
	const auto& args_node = ast.at(args_idx);

	if (func_node.type != NodeType::ID)
		err("Unnamed functions are not implemented");

	ValueCell* maybe_func_ptr = inter->env.find(scope_id, func_node.str_id);
	if (!maybe_func_ptr) {
		auto msg = "Function with name "
		         + std::string(inter->pool.find(func_node.str_id)) + " not found";
		err(msg.c_str());
	}

	ValueCell func_ptr = *maybe_func_ptr;

	std::vector<ValueCell> args {};
	for (auto arg_idx : args_node) {
		auto arg = inter_eval_node(inter, ast, arg_idx, scope_id);
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
		auto new_scope = inter->env.create_child_scope(scope_id);

		for (size_t i = 0; i < args.size(); i++) {
			const auto& param_node = ast.at(custom.param_idxs[i]);
			assert(param_node.type == NodeType::ID);
			auto cell = inter->env.insert(new_scope, param_node.str_id, args[i]);
			assert(cell != nullptr);
		}

		auto val = inter_eval_node(inter, ast, custom.body_idx, new_scope);
		return val;
	} else {
		assert(false);
	}
}

ValueCell eval_block(
	Interpreter* inter, AST& ast, const Node& node,
	Env<ValueCell>::ScopeID scope_id
) {
	ValueCell res {nullptr};
	auto scope = inter->env.create_child_scope(scope_id);
	for (size_t i = 0; i < node.branch.children_count; i++)
		res = inter_eval_node(inter, ast, node[i], scope);
	return res;
}

// if exp then exp else exp
ValueCell eval_if(
	Interpreter* inter, AST& ast, const Node& node,
	Env<ValueCell>::ScopeID scope_id
) {
	ValueCell cond = inter_eval_node(inter, ast, node[0], scope_id);
	auto boolean = std::get<bool>(*cond);
	return (boolean) ? inter_eval_node(inter, ast, node[1], scope_id)
	                 : inter_eval_node(inter, ast, node[2], scope_id);
}

// when exp then exp
ValueCell eval_when(
	Interpreter* inter, AST& ast, const Node& node,
	Env<ValueCell>::ScopeID scope_id
) {
	ValueCell cond = inter_eval_node(inter, ast, node[0], scope_id);
	auto boolean = std::get<bool>(*cond);
	if (boolean) inter_eval_node(inter, ast, node[1], scope_id);
	return {};
}

// "for" decl "from" exp "to" exp ("step" exp)? "then" exp
ValueCell eval_for(
	Interpreter* inter, AST& ast, const Node& node,
	Env<ValueCell>::ScopeID scope_id
) {
	auto decl_idx = node[0];
	auto to_idx = node[1];
	auto step_idx = node[2];
	auto then_idx = node[3];

	const auto& step_node = ast.at(step_idx);

	bool with_step = step_node.type != NodeType::EMPTY;

	auto new_scope_id = inter->env.create_child_scope(scope_id);

	auto decl = inter_eval_node(inter, ast, decl_idx, new_scope_id);
	auto to = inter_eval_node(inter, ast, to_idx, new_scope_id);
	auto inc = with_step ? inter_eval_node(inter, ast, step_idx, new_scope_id)
	                     : std::make_shared<Value>(Value {1});

	if (not std::holds_alternative<int>(*to))
		err("Type of `to' value is not number");

	if (not std::holds_alternative<int>(*inc))
		err("Type of `inc' value is not number");

	auto inner_scope_id = inter->env.create_child_scope(new_scope_id);

	inter->in_loop = true;
	for (int i = std::get<int>(*decl); i != std::get<int>(*to);
	     i += std::get<int>(*inc)) {
		*decl = Value {i};
		inter_eval_node(inter, ast, then_idx, inner_scope_id);
		if (inter->should_break) break;
		if (inter->should_continue) continue;
	}
	inter->in_loop = false;

	return std::make_shared<Value>(Nil {});
}

// "while" exp "then" exp
ValueCell eval_while(
	Interpreter* inter, AST& ast, const Node& node,
	Env<ValueCell>::ScopeID scope_id
) {
	auto cond_idx = node[0];
	auto then_idx = node[1];

	inter->in_loop = true;
	ValueCell res {};
	ValueCell cond {};
	while ((cond = inter_eval_node(inter, ast, cond_idx, scope_id))
	       and std::get<bool>(*cond)) {
		res = inter_eval_node(inter, ast, then_idx, scope_id);
		if (inter->should_break) break;
		if (inter->should_continue) continue;
	}

	inter->in_loop = false;
	return res;
}

// id ([idx])? = exp
ValueCell eval_ass(
	Interpreter* inter, AST& ast, const Node& node,
	Env<ValueCell>::ScopeID scope_id
) {
	auto path_idx = node[0];
	auto exp_idx = node[1];

	auto lvalue = inter_eval_node(inter, ast, path_idx, scope_id);
	auto right = inter_eval_node(inter, ast, exp_idx, scope_id);

	*lvalue = *right;
	return right;
}

ValueCell eval_var_decl(
	Interpreter* inter, AST& ast, const Node& node,
	Env<ValueCell>::ScopeID scope_id
) {
	assert(node.branch.children_count == 3);

	auto id_idx = node[0];
	[[maybe_unused]] auto opt_type_idx = node[1];
	auto exp_idx = node[2];

	const auto& id_node = ast.at(id_idx);

	auto value = inter_eval_node(inter, ast, exp_idx, scope_id);
	auto cell = inter->env.insert(scope_id, id_node.str_id, value);
	if (!cell) err2(node.loc, "Could not initialize variable");
	return value;
}

ValueCell eval_fun_decl(
	Interpreter* inter, AST& ast, const Node& node,
	Env<ValueCell>::ScopeID scope_id
) {
	assert(node.branch.children_count == 4);

	auto id_idx = node[0];
	auto params_idx = node[1];
	auto opt_type_idx = node[2];
	auto body_idx = node[3];

	const auto& id_node = ast.at(id_idx);
	const auto& params_node = ast.at(params_idx);

	auto cell = inter->env.insert(scope_id, id_node.str_id);
	if (!cell) err2(node.loc, "Could not initialize variable");

	std::vector<NodeIndex> param_idxs {};
	for (auto param_idx : params_node) {
		const auto& param_node = ast.at(param_idx);
		if (param_node.type != NodeType::ID)
			err("Function parameter must be a valid identifier");
		param_idxs.push_back(param_idx);
	}

	auto value = std::make_shared<Value>(CustomFunction {param_idxs, body_idx});
	*cell = value;
	return *cell;
}

ValueCell inter_eval_node(
	Interpreter* inter, AST& ast, NodeIndex node_idx,
	Env<ValueCell>::ScopeID scope_id
) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::NUM: return std::make_shared<Value>(Value(node.num));
		case NodeType::APP: return eval_app(inter, ast, node, scope_id);
		case NodeType::BLK: return eval_block(inter, ast, node, scope_id);
		case NodeType::IF: return eval_if(inter, ast, node, scope_id);
		case NodeType::WHEN: return eval_when(inter, ast, node, scope_id);
		case NodeType::FOR: return eval_for(inter, ast, node, scope_id);
		case NodeType::WHILE: return eval_while(inter, ast, node, scope_id);
		case NodeType::BREAK: {
			if (!inter->in_loop) err("Can't break outside of a loop");
			inter->should_break = true;
			return inter_eval_node(inter, ast, node[0], scope_id);
		}
		case NodeType::CONTINUE: {
			if (!inter->in_loop) err("Can't continue outside of a loop");
			inter->should_continue = true;
			return inter_eval_node(inter, ast, node[0], scope_id);
		}
		case NodeType::ASS: return eval_ass(inter, ast, node, scope_id);
		case NodeType::OR: {
			auto left = inter_eval_node(inter, ast, node[0], scope_id);
			if (std::get<bool>(*left)) return left;

			auto right = inter_eval_node(inter, ast, node[1], scope_id);
			return right;
		}
		case NodeType::AND: {
			auto left = inter_eval_node(inter, ast, node[0], scope_id);
			if (not std::get<bool>(*left)) return left;

			auto right = inter_eval_node(inter, ast, node[1], scope_id);
			return right;
		}
#define ARITH_OP(OP)                                                   \
	{                                                                    \
		auto left = inter_eval_node(inter, ast, node[0], scope_id);        \
		auto right = inter_eval_node(inter, ast, node[1], scope_id);       \
		if (left == nullptr) {                                             \
			err("Left side was null while performing operation " #OP "\n");  \
		}                                                                  \
		if (right == nullptr) {                                            \
			err("Right side was null while performing operation " #OP "\n"); \
		}                                                                  \
		if (not std::holds_alternative<int>(*left))                        \
			err("Left-hand side of arithmentic operator is not a number");   \
		if (not std::holds_alternative<int>(*right))                       \
			err("Right-hand side of arithmentic operator is not a number");  \
		return std::make_shared<Value>(Value(std::get<int>(*left)          \
		                                       OP std::get<int>(*right))); \
	}
		case NodeType::ADD: ARITH_OP(+);
		case NodeType::SUB: ARITH_OP(-);
		case NodeType::MUL: ARITH_OP(*);
		case NodeType::DIV: ARITH_OP(/);
		case NodeType::MOD: ARITH_OP(%);
#undef ARITH_OP
#define CMP_OP(OP)                                                             \
	{                                                                            \
		auto left = inter_eval_node(inter, ast, node[0], scope_id);                \
		auto right = inter_eval_node(inter, ast, node[1], scope_id);               \
		if (not std::holds_alternative<int>(*left)                                 \
		    || not std::holds_alternative<int>(*right)) {                          \
			err2(node.loc, "Arithmetic comparison is allowed only between numbers"); \
		}                                                                          \
                                                                               \
		return (std::get<int>(*left) OP std::get<int>(*right))                     \
		       ? std::make_shared<Value>(true)                                     \
		       : std::make_shared<Value>(false);                                   \
	}
		case NodeType::GTN: CMP_OP(>);
		case NodeType::LTN: CMP_OP(<);
		case NodeType::GTE: CMP_OP(>=);
		case NodeType::LTE: CMP_OP(<=);
#undef CMP_OP
		case NodeType::EQ: { // exp == exp
			auto left = inter_eval_node(inter, ast, node[0], scope_id);
			auto right = inter_eval_node(inter, ast, node[1], scope_id);

			if (std::holds_alternative<Nil>(*left)
			    and std::holds_alternative<Nil>(*right))
				return std::make_shared<Value>(true);

			if (std::holds_alternative<bool>(*left)
			    and std::holds_alternative<bool>(*right))
				return std::make_shared<Value>(
					std::get<bool>(*left) == std::get<bool>(*right)
				);

			if (std::holds_alternative<int>(*left)
			    and std::holds_alternative<int>(*right))
				return std::make_shared<Value>(
					std::get<int>(*left) == std::get<int>(*right)
				);

			err("Can't compare values");
			return {};
		}
		case NodeType::NOT: { // not exp
			auto val = inter_eval_node(inter, ast, node[0], scope_id);
			return std::make_shared<Value>(not std::get<bool>(*val));
		}
		case NodeType::AT: {
			auto base = inter_eval_node(inter, ast, node[0], scope_id);
			if (not std::holds_alternative<Array>(*base))
				err("Can only index arrays");

			auto off = inter_eval_node(inter, ast, node[1], scope_id);
			if (not std::holds_alternative<int>(*off)) err("Index must be a number");

			assert((size_t)std::get<int>(*off) < std::get<Array>(*base).size);

			return std::get<Array>(*base).items[(size_t)std::get<int>(*off)];
		}
		case NodeType::ID: {
			auto var = inter->env.find(scope_id, node.str_id);
			if (!var) err2(node.loc, "Variable not previously declared.");
			if ((*var) == nullptr) {
				std::cerr << "\"" << std::string(inter->pool.find(node.str_id)) << "\""
									<< '\n';
				err2(node.loc, "Value cell was null");
			}
			return *var;
		}
		case NodeType::STR: {
			const char* str = inter->pool.find(node.str_id);
			if (str == nullptr) err("Unknown string");
			return std::make_shared<Value>(std::string(str));
		}
		case NodeType::VAR_DECL: return eval_var_decl(inter, ast, node, scope_id);
		case NodeType::FUN_DECL: return eval_fun_decl(inter, ast, node, scope_id);
		case NodeType::NIL: return std::make_shared<Value>(Nil {});
		case NodeType::TRUE: return std::make_shared<Value>(true);
		case NodeType::FALSE: return std::make_shared<Value>(false);
		case NodeType::LET: {
			auto decls_idx = node[0];
			auto exp_idx = node[1];

			const auto& decls = ast.at(decls_idx);

			auto new_scope = inter->env.create_child_scope(scope_id);
			for (size_t i = 0; i < decls.branch.children_count; i++)
				inter_eval_node(inter, ast, decls[i], new_scope);

			auto res = inter_eval_node(inter, ast, exp_idx, new_scope);
			return res;
		}
		case NodeType::EMPTY: assert(false && "unreachable");
		case NodeType::CHAR: return std::make_shared<Value>((int)node.character);
		case NodeType::PATH: return inter_eval_node(inter, ast, node[0], scope_id);
		case NodeType::INSTANCE:
			assert(false && "used only in typechecking. should not be evaluated");
		case NodeType::AS: return inter_eval_node(inter, ast, node[0], scope_id);
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
	Number arr_len = std::get<int>(*args[0]);
	if (arr_len < 0) err("Array length must be positive");
	ValueCell val = std::make_shared<Value>(Array {});
	std::get<Array>(*val).items.reserve((size_t)arr_len);
	for (auto i = 0ul; i < (size_t)arr_len; i++) {
		std::get<Array>(*val).items[i] = std::make_shared<Value>((int)0);
	}
	std::get<Array>(*val).size = (size_t)arr_len;
	return val;
}

ValueCell builtin_exit(std::vector<ValueCell> args) {
	if (args.size() != 1) err("exit takes exit code as a argument");
	Number exit_code = std::get<int>(*args[0]);
	exit(exit_code);
	return {};
}

#define PUSH_BUILTIN(STR, FUNC, COUNT)              \
	*env.insert(scope_id, pool.intern(strdup(STR))) = \
		std::make_shared<Value>(BuiltinFunction(COUNT, FUNC))

Interpreter::Interpreter(StringPool& _pool)
: pool(_pool), in_loop(false), should_break(false), should_continue(false) {
	// arbitrary choice
	auto scope_id = env.root_scope_id;
	PUSH_BUILTIN("read_int", builtin_read, 1);
	PUSH_BUILTIN("write_int", builtin_write, 1);
	PUSH_BUILTIN("write_str", builtin_write, 1);
	PUSH_BUILTIN("make_array", builtin_array, 1);
	PUSH_BUILTIN("exit", builtin_exit, 1);
}

} // namespace walk
