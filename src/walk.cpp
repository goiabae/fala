#include "walk.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.hpp"
#include "str_pool.h"

namespace walk {

Value inter_eval_node(
	Interpreter* inter, AST& ast, NodeIndex node_idx, Env<Value>::ScopeID scope_id
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

Value Interpreter::eval(AST& ast) {
	return inter_eval_node(this, ast, ast.root_index, env.root_scope_id);
}

// term term+
Value eval_app(
	Interpreter* inter, AST& ast, const Node& node, Env<Value>::ScopeID scope_id
) {
	auto func_idx = node[0];
	auto args_idx = node[1];

	const auto& func_node = ast.at(func_idx);
	const auto& args_node = ast.at(args_idx);

	if (func_node.type != NodeType::ID)
		err("Unnamed functions are not implemented");

	Value* func_ptr = inter->env.find(scope_id, func_node.str_id);
	if (!func_ptr) err("Function with name <name> not found");

	Function func = func_ptr->func;
	if (func.param_count != args_node.branch.children_count
	    && func.param_count != 0)
		err("Wrong number of arguments");

	size_t argc = args_node.branch.children_count;
	Value* args = new Value[argc];
	for (size_t i = 0; i < argc; i++)
		args[i] = inter_eval_node(inter, ast, args_node[i], scope_id)
		            .to_rvalue(); // call-by-value

	if (func.is_builtin) {
		Value val = func.builtin(argc, args);
		delete[] args;
		return val;
	}

	auto new_scope = inter->env.create_child_scope(scope_id);

	// set all parameters to the corresponding arguments
	for (size_t i = 0; i < argc; i++) {
		const auto& param = ast.at(func.custom.params[i]);
		*inter->env.insert(new_scope, param.str_id) = args[i];
	}

	Value val = inter_eval_node(inter, ast, func.custom.root, new_scope);

	delete[] args;
	return val;
}

Value eval_block(
	Interpreter* inter, AST& ast, const Node& node, Env<Value>::ScopeID scope_id
) {
	Value res;
	auto scope = inter->env.create_child_scope(scope_id);
	for (size_t i = 0; i < node.branch.children_count; i++)
		res = inter_eval_node(inter, ast, node[i], scope);
	return res;
}

// if exp then exp else exp
Value eval_if(
	Interpreter* inter, AST& ast, const Node& node, Env<Value>::ScopeID scope_id
) {
	Value cond = inter_eval_node(inter, ast, node[0], scope_id);
	return (cond) ? inter_eval_node(inter, ast, node[1], scope_id)
	              : inter_eval_node(inter, ast, node[2], scope_id);
}

// when exp then exp
Value eval_when(
	Interpreter* inter, AST& ast, const Node& node, Env<Value>::ScopeID scope_id
) {
	Value cond = inter_eval_node(inter, ast, node[0], scope_id);
	if (cond) inter_eval_node(inter, ast, node[1], scope_id);
	return {};
}

// "for" decl "from" exp "to" exp ("step" exp)? "then" exp
Value eval_for(
	Interpreter* inter, AST& ast, const Node& node, Env<Value>::ScopeID scope_id
) {
	auto decl_idx = node[0];
	auto to_idx = node[1];
	auto step_idx = node[2];
	auto then_idx = node[3];

	const auto& step_node = ast.at(step_idx);

	bool with_step = step_node.type != NodeType::EMPTY;

	Value decl = inter_eval_node(inter, ast, decl_idx, scope_id);
	Value to = inter_eval_node(inter, ast, to_idx, scope_id).to_rvalue();
	Value inc =
		((with_step) ? inter_eval_node(inter, ast, step_idx, scope_id) : Value(1))
			.to_rvalue();

	if (to.type != Value::Type::NUM) err("Type of `to' value is not number");
	if (inc.type != Value::Type::NUM) err("Type of `inc' value is not number");

	auto scope = inter->env.create_child_scope(scope_id);

	Value* var = decl.var;

	inter->in_loop = true;
	Value res;
	for (Number i = var->num; i != to.num; i += inc.num) {
		*var = Value(i);
		res = inter_eval_node(inter, ast, then_idx, scope);
		if (inter->should_break) break;
		if (inter->should_continue) continue;
	}
	inter->in_loop = false;

	return res;
}

// "while" exp "then" exp
Value eval_while(
	Interpreter* inter, AST& ast, const Node& node, Env<Value>::ScopeID scope_id
) {
	auto cond_idx = node[0];
	auto then_idx = node[1];

	inter->in_loop = true;
	Value res;
	while (inter_eval_node(inter, ast, cond_idx, scope_id)) {
		res = inter_eval_node(inter, ast, then_idx, scope_id);
		if (inter->should_break) break;
		if (inter->should_continue) continue;
	}

	inter->in_loop = false;
	return res;
}

// id ([idx])? = exp
Value eval_ass(
	Interpreter* inter, AST& ast, const Node& node, Env<Value>::ScopeID scope_id
) {
	auto path_idx = node[0];
	auto exp_idx = node[1];

	Value lvalue = inter_eval_node(inter, ast, path_idx, scope_id);
	if (lvalue.type != Value::Type::VAR) err("Can only assign to variable");

	Value right = inter_eval_node(inter, ast, exp_idx, scope_id).to_rvalue();
	return *(lvalue.var) = right;
}

Value eval_var_decl(
	Interpreter* inter, AST& ast, const Node& node, Env<Value>::ScopeID scope_id
) {
	auto id_idx = node[0];
	const auto& id_node = ast.at(id_idx);

	Value* cell = inter->env.insert(scope_id, id_node.str_id);
	if (!cell) err2(node.loc, "Could not initialize variable");

	// "fun" id params opt-type "=" body
	if (node.branch.children_count == 4) {
		auto body_idx = node[3];

		const auto& params_node = ast.at(node[1]);
		const auto& opt_type_node = ast.at(node[2]);

		(void)opt_type_node;

		for (size_t i = 0; i < params_node.branch.children_count; i++) {
			const auto& param = ast.at(params_node[i]);
			if (param.type != NodeType::ID)
				err("Function parameter must be a valid identifier");
			auto value = Value(Function(
				body_idx, params_node.branch.children, params_node.branch.children_count
			));
			return *cell = value;
		}
	}

	// "var" id opt-type "=" exp
	if (node.branch.children_count == 3) {
		auto opt_type_idx = node[1];
		auto exp_idx = node[2];

		(void)opt_type_idx;

		Value res(cell);
		*cell = inter_eval_node(inter, ast, exp_idx, scope_id).to_rvalue();
		return res;
	}

	assert(false);
}

Value eval_fun_decl(
	Interpreter* inter, AST& ast, const Node& node, Env<Value>::ScopeID scope_id
) {
	auto id_idx = node[0];
	const auto& id_node = ast.at(id_idx);

	Value* cell = inter->env.insert(scope_id, id_node.str_id);
	if (!cell) err2(node.loc, "Could not initialize variable");

	// "fun" id params opt-type "=" body
	if (node.branch.children_count == 4) {
		auto body_idx = node[3];

		const auto& params_node = ast.at(node[1]);
		const auto& opt_type_node = ast.at(node[2]);

		(void)opt_type_node;

		for (size_t i = 0; i < params_node.branch.children_count; i++) {
			const auto& param = ast.at(params_node[i]);
			if (param.type != NodeType::ID)
				err("Function parameter must be a valid identifier");
			auto value = Value(Function(
				body_idx, params_node.branch.children, params_node.branch.children_count
			));
			return *cell = value;
		}
	}

	// "var" id opt-type "=" exp
	if (node.branch.children_count == 3) {
		auto opt_type_idx = node[1];
		auto exp_idx = node[2];

		(void)opt_type_idx;

		Value res(cell);
		*cell = inter_eval_node(inter, ast, exp_idx, scope_id).to_rvalue();
		return res;
	}

	assert(false);
}

Value inter_eval_node(
	Interpreter* inter, AST& ast, NodeIndex node_idx, Env<Value>::ScopeID scope_id
) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::NUM: return Value(node.num);
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
			Value left = inter_eval_node(inter, ast, node[0], scope_id);
			if (left) return Value(true);

			Value right = inter_eval_node(inter, ast, node[1], scope_id);
			return (right) ? Value(true) : Value();
		}
		case NodeType::AND: {
			Value left = inter_eval_node(inter, ast, node[0], scope_id);
			if (!left) return Value();

			Value right = inter_eval_node(inter, ast, node[1], scope_id);
			return (right) ? Value(true) : Value();
		}
#define ARITH_OP(OP)                                                          \
	{                                                                           \
		Value left = inter_eval_node(inter, ast, node[0], scope_id).to_rvalue();  \
		Value right = inter_eval_node(inter, ast, node[1], scope_id).to_rvalue(); \
		if (left.type != Value::Type::NUM)                                        \
			err("Left-hand side of arithmentic operator is not a number");          \
		if (right.type != Value::Type::NUM)                                       \
			err("Right-hand side of arithmentic operator is not a number");         \
		return Value(left.num OP right.num);                                      \
	}
		case NodeType::ADD: ARITH_OP(+);
		case NodeType::SUB: ARITH_OP(-);
		case NodeType::MUL: ARITH_OP(*);
		case NodeType::DIV: ARITH_OP(/);
		case NodeType::MOD: ARITH_OP(%);
#undef ARITH_OP
#define CMP_OP(OP)                                                             \
	{                                                                            \
		Value left = inter_eval_node(inter, ast, node[0], scope_id).to_rvalue();   \
		Value right = inter_eval_node(inter, ast, node[1], scope_id).to_rvalue();  \
		if (left.type != Value::Type::NUM || right.type != Value::Type::NUM) {     \
			err2(node.loc, "Arithmetic comparison is allowed only between numbers"); \
		}                                                                          \
                                                                               \
		return (left.num OP right.num) ? Value(true) : Value();                    \
	}
		case NodeType::GTN: CMP_OP(>);
		case NodeType::LTN: CMP_OP(<);
		case NodeType::GTE: CMP_OP(>=);
		case NodeType::LTE: CMP_OP(<=);
#undef CMP_OP
		case NodeType::EQ: { // exp == exp
			Value left = inter_eval_node(inter, ast, node[0], scope_id).to_rvalue();
			Value right = inter_eval_node(inter, ast, node[1], scope_id).to_rvalue();

			if (left.type == Value::Type::NIL && right.type == Value::Type::NIL)
				return Value(true);

			if (left.type == Value::Type::TRUE && right.type == Value::Type::TRUE)
				return Value(true);

			if (left.type == Value::Type::NUM && right.type == Value::Type::NUM)
				return (left.num == right.num) ? Value(true) : Value();

			return {};
		}
		case NodeType::NOT: { // not exp
			Value val = inter_eval_node(inter, ast, node[0], scope_id);
			return (val) ? Value() : Value(true);
		}
		case NodeType::AT: {
			Value base = inter_eval_node(inter, ast, node[0], scope_id).to_rvalue();
			if (base.type != Value::Type::ARR) err("Can only index arrays");
			Value off = inter_eval_node(inter, ast, node[1], scope_id).to_rvalue();
			if (off.type != Value::Type::NUM) err("Index must be a number");
			Value res(&base.arr.data[off.num]);
			return res;
		}
		case NodeType::ID: {
			Value* addr = inter->env.find(scope_id, node.str_id);
			if (!addr) err2(node.loc, "Variable not previously declared.");

			Value res(addr);
			return res;
		}
		case NodeType::STR: {
			const char* str = str_pool_find(inter->pool, node.str_id);
			return Value(strdup(str));
		}
		case NodeType::VAR_DECL: return eval_var_decl(inter, ast, node, scope_id);
		case NodeType::FUN_DECL: return eval_fun_decl(inter, ast, node, scope_id);
		case NodeType::NIL: return {};
		case NodeType::TRUE: return Value(true);
		case NodeType::FALSE: return Value(false);
		case NodeType::LET: {
			auto decls_idx = node[0];
			auto exp_idx = node[1];

			const auto& decls = ast.at(decls_idx);

			auto new_scope = inter->env.create_child_scope(scope_id);
			for (size_t i = 0; i < decls.branch.children_count; i++)
				inter_eval_node(inter, ast, decls[i], new_scope);

			Value res = inter_eval_node(inter, ast, exp_idx, new_scope);
			return res;
		}
		case NodeType::EMPTY: assert(false && "unreachable");
		case NodeType::CHAR: return Value(node.character);
		case NodeType::PATH: return inter_eval_node(inter, ast, node[0], scope_id);
		case NodeType::INT_TYPE: assert(false && "TODO");
		case NodeType::UINT_TYPE: assert(false && "TODO");
		case NodeType::BOOL_TYPE: assert(false && "TODO");
		case NodeType::NIL_TYPE: assert(false && "TODO");
		case NodeType::AS: return inter_eval_node(inter, ast, node[0], scope_id);
	}
	assert(false);
}

void print_value(Value val) {
	switch (val.type) {
		case Value::Type::NUM: printf("%d", val.num); break;
		case Value::Type::STR: printf("%s", val.str); break;
		case Value::Type::TRUE: printf("1"); break;
		case Value::Type::NIL: printf("0"); break;
		case Value::Type::FUN: err("Can't print function"); break;
		case Value::Type::ARR: err("Can't print array"); break;
		case Value::Type::VAR: print_value(*(val.var)); break;
	}
}

std::ostream& operator<<(std::ostream& st, Value& val) {
	switch (val.type) {
		case Value::Type::NUM: return st << val.num;
		case Value::Type::STR: return st << val.str;
		case Value::Type::TRUE: return st << 1;
		case Value::Type::NIL: return st << 0;
		case Value::Type::ARR: assert(false);
		case Value::Type::FUN: assert(false);
		case Value::Type::VAR: return st << *(val.var);
	}
	assert(false);
}

void Value::deinit() {
	if (type == Value::Type::STR)
		free(str);
	else if (type == Value::Type::ARR)
		free(arr.data);
}

static Value builtin_read(size_t _1, Value* _2) {
	(void)_1, (void)_2;
	char* buf = (char*)malloc(sizeof(char) * 100);
	if (!buf) err("Could not allocate input buffer for `read' built-in");
	if (fgets(buf, 100, stdin) == NULL) {
		free(buf);
		return {};
	}
	const size_t len = strlen(buf);
	buf[len - 1] = '\0';

	// try to parse as number
	Number num = 0;
	if (sscanf(buf, "%d", &num) != 0) {
		free(buf);
		return Value(num);
	}

	// otherwise return as string
	return Value(buf);
}

static Value builtin_write(size_t len, Value* args) {
	for (size_t i = 0; i < len; i++) print_value(args[i]);
	return {};
}

static Value builtin_array(size_t argc, Value* args) {
	if (argc != 1) err("Expected a single numeric argument");
	Number arr_len = args[0].num;
	if (arr_len < 0) err("Array length must be positive");
	Value val;
	val.type = Value::Type::ARR;
	val.arr.data = (Value*)malloc(sizeof(Value) * (size_t)arr_len);
	if (!val.arr.data) err("Dynamic memory allocation error");
	val.arr.len = (size_t)arr_len;
	return val;
}

static Value builtin_exit(size_t argc, Value* args) {
	if (argc != 1) err("exit takes exit code as a argument");
	Number exit_code = args[0].num;
	exit(exit_code);
	return {};
}

// if COUNT is 0 the function takes a variable amount of arguments
#define PUSH_BUILTIN(STR, FUNC, COUNT)               \
	*env.insert(scope_id, pool->intern(strdup(STR))) = \
		Value(Function(FUNC, COUNT))

Interpreter::Interpreter(STR_POOL _pool)
: pool(_pool), in_loop(false), should_break(false), should_continue(false) {
	// arbitrary choice
	auto scope_id = env.root_scope_id;
	PUSH_BUILTIN("read", builtin_read, 1);
	PUSH_BUILTIN("write", builtin_write, 0);
	PUSH_BUILTIN("array", builtin_array, 1);
	PUSH_BUILTIN("exit", builtin_exit, 1);
}

} // namespace walk
