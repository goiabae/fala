#include "eval.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.h"
#include "str_pool.h"

static void value_deinit(Value val);

static Value inter_eval_node(Interpreter* inter, Node node);

static void err(const char* msg) {
	fprintf(stderr, "INTERPRETER ERROR: %s\n", msg);
	exit(1);
}

static void inter_env_push(Interpreter* inter) { inter->env.scope_count++; }

static void inter_env_pop(Interpreter* inter) {
	if (inter->env.scope_count == 0) err("Environment was empty");
	for (size_t i = inter->env.len; i-- > 0; inter->env.len--) {
		if (inter->env.scopes[i] != (inter->env.scope_count - 1)) break;
		value_deinit(inter->values.values[i]);
		inter->values.len--;
	}
	inter->env.scope_count--;
}

static Value* inter_env_get_new(Interpreter* inter, StrID str_id) {
	inter->env.scopes[inter->env.len] = inter->env.scope_count - 1;
	inter->env.syms[inter->env.len] = str_id.idx;
	inter->env.indexes[inter->env.len] = inter->env.len;
	Value* res = &inter->values.values[inter->env.len++];
	inter->values.len++;
	return res;
}

static Value* inter_env_find(Interpreter* inter, StrID str_id) {
	for (size_t i = inter->env.len; i > 0;)
		if (inter->env.syms[--i] == str_id.idx) return &inter->values.values[i];
	return NULL;
}

Value Interpreter::eval(AST ast) { return inter_eval_node(this, ast.root); }

// term term+
Value eval_app(Interpreter* inter, Node node) {
	assert(node.children_count == 2);
	Node func_node = node.children[0];
	Node args_node = node.children[1];

	if (func_node.type != AST_ID) err("Unnamed functions are not implemented");

	Value* func_ptr = inter_env_find(inter, func_node.str_id);
	if (!func_ptr) err("Function with name <name> not found");

	Function func = func_ptr->func;
	if (func.param_count != args_node.children_count && func.param_count != 0)
		err("Wrong number of arguments");

	size_t argc = args_node.children_count;
	Value* args = new Value[argc];
	for (size_t i = 0; i < argc; i++)
		args[i] = inter_eval_node(inter, args_node.children[i]);

	if (func.is_builtin) {
		Value val = func.builtin(argc, args);
		delete[] args;
		return val;
	}

	inter_env_push(inter);

	// set all parameters to the corresponding arguments
	for (size_t i = 0; i < argc; i++)
		*inter_env_get_new(inter, func.params[i].str_id) = args[i];

	Value val = inter_eval_node(inter, func.root);

	inter_env_pop(inter);

	delete[] args;
	return val;
}

Value eval_block(Interpreter* inter, Node node) {
	Value res;
	inter_env_push(inter);
	for (size_t i = 0; i < node.children_count; i++)
		res = inter_eval_node(inter, node.children[i]);
	inter_env_pop(inter);
	return res;
}

// if exp then exp else exp
Value eval_if(Interpreter* inter, Node node) {
	Value cond = inter_eval_node(inter, node.children[0]);
	return (cond) ? inter_eval_node(inter, node.children[1])
	              : inter_eval_node(inter, node.children[2]);
}

// when exp then exp
Value eval_when(Interpreter* inter, Node node) {
	Value cond = inter_eval_node(inter, node.children[0]);
	if (cond) inter_eval_node(inter, node.children[1]);
	return {};
}

// for var id from exp to exp (step exp)? then exp
Value eval_for(Interpreter* inter, Node node) {
	bool with_step = node.children_count == 5;

	Node id_node = node.children[0].children[0];
	Node from_node = node.children[1];
	Node to_node = node.children[2];
	Node step_node = node.children[3];
	Node exp_node = node.children[3 + with_step];

	Value from = inter_eval_node(inter, from_node);
	Value to = inter_eval_node(inter, to_node);
	Value inc = (with_step) ? inter_eval_node(inter, step_node) : Value(1);

	if (from.type != Value::Type::NUM) err("Type of `from' value is not number");
	if (to.type != Value::Type::NUM) err("Type of `to' value is not number");
	if (inc.type != Value::Type::NUM) err("Type of `inc' value is not number");

	inter_env_push(inter);

	Value* var = inter_env_get_new(inter, id_node.str_id);

	inter->in_loop = true;
	Value res;
	for (Number i = from.num; i != to.num; i += inc.num) {
		*var = Value(i);
		res = inter_eval_node(inter, exp_node);
		if (inter->should_break) break;
		if (inter->should_continue) continue;
	}
	inter->in_loop = false;

	inter_env_pop(inter);
	return res;
}

// while exp then exp
Value eval_while(Interpreter* inter, Node node) {
	assert(node.children_count == 2);
	Node cond = node.children[0];
	Node exp = node.children[1];

	inter->in_loop = true;
	Value res;
	while (inter_eval_node(inter, cond)) {
		res = inter_eval_node(inter, exp);
		if (inter->should_break) break;
		if (inter->should_continue) continue;
	}
	inter->in_loop = false;
	return res;
}

// id ([idx])? = exp
Value eval_ass(Interpreter* inter, Node node) {
	assert(node.children_count == 2);
	Node var = node.children[0];
	Node id = var.children[0];

	Value value = inter_eval_node(inter, node.children[1]);
	Value* cell = inter_env_find(inter, id.str_id);
	if (!cell) err("Variable not found. Address is 0x0");

	if (var.children_count == 2) { // array indexing
		Value idx = inter_eval_node(inter, var.children[1]);
		if (idx.type != Value::Type::NUM) err("Index needs to be a number");
		if ((size_t)idx.num >= cell->arr.len) err("Index out of bounds");
		return cell->arr.data[idx.num] = value;
	}

	return *cell = value; // simple access
}

Value eval_decl(Interpreter* inter, Node node) {
	Node id = node.children[0];
	Value* cell = inter_env_get_new(inter, id.str_id);

	// fun id id+ = exp
	if (node.children_count == 3) {
		Node params = node.children[1];
		for (size_t i = 0; i < params.children_count; i++)
			if (params.children[i].type != AST_ID)
				err("Function parameter must be a valid identifier");
		Node exp = node.children[2];
		return *cell = Value(Function(exp, params.children, params.children_count));
	}

	// var id = exp
	if (node.children_count == 2)
		return *cell = inter_eval_node(inter, node.children[1]);

	// var id
	return Value(false);
}

Value eval_var(Interpreter* inter, Node node) {
	Node id = node.children[0];
	Value* addr = inter_env_find(inter, id.str_id);
	if (!addr) err("Variable not previously declared.");

	if (node.children_count == 2) { // id [idx]
		if (addr->type != Value::Type::ARR)
			err("Variable does not correspond to a previously declared array");

		Value idx = inter_eval_node(inter, node.children[1]);
		if (idx.type != Value::Type::NUM) err("Index is not a number");
		return Value(addr->arr.data[idx.num].num);
	}

	return *addr; // id
}

Value inter_eval_node(Interpreter* inter, Node node) {
	switch (node.type) {
		case AST_NUM: return Value(node.num);
		case AST_APP: return eval_app(inter, node);
		case AST_BLK: return eval_block(inter, node);
		case AST_IF: return eval_if(inter, node);
		case AST_WHEN: return eval_when(inter, node);
		case AST_FOR: return eval_for(inter, node);
		case AST_WHILE: return eval_while(inter, node);
		case AST_BREAK: {
			assert(node.children_count == 1); // break exp
			if (!inter->in_loop) err("Can't break outside of a loop");
			inter->should_break = true;
			return inter_eval_node(inter, node.children[0]);
		}
		case AST_CONTINUE: {
			assert(node.children_count == 1); // continue exp
			if (!inter->in_loop) err("Can't continue outside of a loop");
			inter->should_continue = true;
			return inter_eval_node(inter, node.children[0]);
		}
		case AST_ASS: return eval_ass(inter, node);
		case AST_OR: {
			assert(node.children_count == 2); // exp or exp

			Value left = inter_eval_node(inter, node.children[0]);
			if (left) return Value(true);

			Value right = inter_eval_node(inter, node.children[1]);
			return (right) ? Value(true) : Value();
		}
		case AST_AND: {
			assert(node.children_count == 2); // exp and exp

			Value left = inter_eval_node(inter, node.children[0]);
			if (!left) return Value();

			Value right = inter_eval_node(inter, node.children[1]);
			return (right) ? Value(true) : Value();
		}
#define ARITH_OP(OP)                                                  \
	{                                                                   \
		assert(node.children_count == 2);                                 \
		Value left = inter_eval_node(inter, node.children[0]);            \
		Value right = inter_eval_node(inter, node.children[1]);           \
		if (left.type != Value::Type::NUM)                                \
			err("Left-hand side of arithmentic operator is not a number");  \
		if (right.type != Value::Type::NUM)                               \
			err("Right-hand side of arithmentic operator is not a number"); \
		return Value(left.num OP right.num);                              \
	}
		case AST_ADD: ARITH_OP(+);
		case AST_SUB: ARITH_OP(-);
		case AST_MUL: ARITH_OP(*);
		case AST_DIV: ARITH_OP(/);
		case AST_MOD: ARITH_OP(%);
#undef ARITH_OP
#define CMP_OP(OP)                                                       \
	{                                                                      \
		assert(node.children_count == 2);                                    \
		Value left = inter_eval_node(inter, node.children[0]);               \
		Value right = inter_eval_node(inter, node.children[1]);              \
		if (left.type != Value::Type::NUM || right.type != Value::Type::NUM) \
			err("Arithmetic comparison is allowed only between numbers");      \
                                                                         \
		return (left.num OP right.num) ? Value(true) : Value();              \
	}
		case AST_GTN: CMP_OP(>);
		case AST_LTN: CMP_OP(<);
		case AST_GTE: CMP_OP(>=);
		case AST_LTE: CMP_OP(<=);
#undef CMP_OP
		case AST_EQ: { // exp == exp
			Value left = inter_eval_node(inter, node.children[0]);
			Value right = inter_eval_node(inter, node.children[1]);

			if (left.type == Value::Type::NIL && right.type == Value::Type::NIL)
				return Value(true);

			if (left.type == Value::Type::TRUE && right.type == Value::Type::TRUE)
				return Value(true);

			if (left.type == Value::Type::NUM && right.type == Value::Type::NUM)
				return (left.num == right.num) ? Value(true) : Value();

			return {};
		}
		case AST_NOT: { // not exp
			Value val = inter_eval_node(inter, node.children[0]);
			return (val) ? Value() : Value(true);
		}
		case AST_ID: assert(false);
		case AST_STR: {
			const char* str = str_pool_find(inter->pool, node.str_id);
			return Value(strdup(str));
		}
		case AST_DECL: return eval_decl(inter, node);
		case AST_VAR: return eval_var(inter, node);
		case AST_NIL: return {};
		case AST_TRUE: return Value(true);
		case AST_LET: {
			Node decls = node.children[0];
			Node exp = node.children[1];
			inter_env_push(inter);

			for (size_t i = 0; i < decls.children_count; i++)
				inter_eval_node(inter, decls.children[i]);

			Value res = inter_eval_node(inter, exp);
			inter_env_pop(inter);
			return res;
		}
	}
	assert(false);
}

void print_value(Value val) {
	if (val.type == Value::Type::NUM)
		printf("%d", val.num);
	else if (val.type == Value::Type::STR)
		printf("%s", val.str);
	else if (val.type == Value::Type::TRUE)
		printf("1");
	else if (val.type == Value::Type::NIL)
		printf("0");
}

std::ostream& operator<<(std::ostream& st, Value& val) {
	switch (val.type) {
		case Value::Type::NUM: return st << val.num;
		case Value::Type::STR: return st << val.str;
		case Value::Type::TRUE: return st << 1;
		case Value::Type::NIL: return st << 0;
		case Value::Type::ARR: assert(false);
		case Value::Type::FUN: assert(false);
	}
	assert(false);
}

static void value_deinit(Value val) {
	if (val.type == Value::Type::STR)
		free(val.str);
	else if (val.type == Value::Type::ARR)
		free(val.arr.data);
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
	assert(argc == 1 && "INTEPRET_ERR: array takes a single numeric argument");
	Number arr_len = args[0].num;
	assert(arr_len >= 0 && "INTERPRET_ERR: array length must be positive");
	Value val;
	val.type = Value::Type::ARR;
	val.arr.data = (Value*)malloc(sizeof(Value) * (size_t)arr_len);
	assert(val.arr.data && "INTERPRET_ERR: dynamic memory allocation error");
	val.arr.len = (size_t)arr_len;
	return val;
}

static Value builtin_exit(size_t argc, Value* args) {
	if (argc != 1) err("INTEPRET_ERR: exit takes exit code as a argument");
	Number exit_code = args[0].num;
	exit(exit_code);
	return {};
}

// if COUNT is 0 the function takes a variable amount of arguments
#define PUSH_BUILTIN(INTER, STR, FUNC, COUNT)                              \
	*inter_env_get_new(&INTER, str_pool_intern((INTER).pool, strdup(STR))) = \
		Value(Function(FUNC, COUNT))

Interpreter::Interpreter(STR_POOL _pool)
: pool(_pool), in_loop(false), should_break(false), should_continue(false) {
	// arbitrary choice
	constexpr size_t default_cap = 32;

	values.len = 0;
	values.cap = default_cap;
	values.values = new Value[default_cap];

	env.len = 0;
	env.cap = default_cap;
	env.scope_count = 0;
	env.indexes = new size_t[default_cap];
	env.syms = new size_t[default_cap];
	env.scopes = new size_t[default_cap];

	inter_env_push(this);

	PUSH_BUILTIN(*this, "read", builtin_read, 1);
	PUSH_BUILTIN(*this, "write", builtin_write, 0);
	PUSH_BUILTIN(*this, "array", builtin_array, 1);
	PUSH_BUILTIN(*this, "exit", builtin_exit, 1);
}

Interpreter::~Interpreter() {
	for (size_t i = 0; i < env.len; i++) {
		value_deinit(values.values[i]);
		values.len--;
	}
	delete[] env.indexes;
	delete[] env.syms;
	delete[] env.scopes;
	delete[] values.values;
}
