#include "eval.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "str_pool.h"

static void value_deinit(Value val);
static void value_stack_pop(ValueStack* stack, size_t index);

static void inter_env_push(Interpreter* inter);
static void inter_env_pop(Interpreter* inter);
static Value* inter_env_get_new(Interpreter* inter, StrID str_id);
static Value* inter_env_find(Interpreter* inter, StrID str_id);

static Value builtin_read(size_t len, Value* args);
static Value builtin_write(size_t len, Value* args);

static Value inter_eval_node(Interpreter* inter, Node node);

static void inter_env_push(Interpreter* inter) { inter->env.scope_count++; }

static void inter_env_pop(Interpreter* inter) {
	assert(inter->env.scope_count > 0 && "Environment was empty");
	for (size_t i = inter->env.len; i-- > 0; inter->env.len--) {
		if (inter->env.scopes[i] != (inter->env.scope_count - 1)) break;
		value_stack_pop(&inter->values, i);
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

Value inter_eval(Interpreter* inter, AST ast) {
	return inter_eval_node(inter, ast.root);
}

Value eval_app(Interpreter* inter, Node node) {
	assert(node.children_count == 2);
	Node func_node = node.children[0];
	Node args_node = node.children[1];

	assert(func_node.type == AST_ID && "Unnamed functions are not implemented");

	Value* func_ptr = inter_env_find(inter, func_node.str_id);
	assert(func_ptr && "For now, only read and write builtins are implemented");
	Function func = func_ptr->func;

	size_t argc = args_node.children_count;
	Value* args = malloc(sizeof(Value) * argc);
	for (size_t i = 0; i < argc; i++)
		args[i] = inter_eval_node(inter, args_node.children[i]);

	if (func.is_builtin) {
		Value val = func.builtin(argc, args);
		free(args);
		return val;
	}

	inter_env_push(inter);

	for (size_t i = 0; i < argc; i++) {
		Node arg = func.args[i];
		assert(arg.type == AST_ID);
		Value* val = inter_env_get_new(inter, arg.str_id);
		*val = args[i];
	}

	Value val = inter_eval_node(inter, func.root);

	inter_env_pop(inter);

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

Value eval_if(Interpreter* inter, Node node) {
	assert(node.children_count == 3);
	Value cond = inter_eval_node(inter, node.children[0]);
	if (cond.tag != VALUE_NIL)
		return inter_eval_node(inter, node.children[1]);
	else
		return inter_eval_node(inter, node.children[2]);
}

Value eval_when(Interpreter* inter, Node node) {
	assert(node.children_count == 2);
	Value cond = inter_eval_node(inter, node.children[0]);
	return (cond.tag != VALUE_NIL) ? inter_eval_node(inter, node.children[1])
	                               : (Value) {VALUE_NIL, .nil = NULL};
}

Value eval_for(Interpreter* inter, Node node) {
	bool with_step = node.children_count == 5;

	Node var_node = node.children[0].children[0];
	Node from_node = node.children[1];
	Node to_node = node.children[2];
	Node step_node = node.children[3];
	Node exp_node = node.children[3 + with_step];

	inter_env_push(inter);

	Value from = inter_eval_node(inter, from_node);
	Value to = inter_eval_node(inter, to_node);
	Value inc = (with_step) ? inter_eval_node(inter, step_node)
	                        : (Value) {VALUE_NUM, .num = 1};

	assert(from.tag == VALUE_NUM && to.tag == VALUE_NUM && inc.tag == VALUE_NUM);

	Value* var = inter_env_get_new(inter, var_node.str_id);

	inter->in_loop = true;
	Value res;
	for (Number i = from.num; i != to.num; i += inc.num) {
		*var = (Value) {VALUE_NUM, .num = i};
		res = inter_eval_node(inter, exp_node);
		if (inter->should_break) break;
		if (inter->should_continue) continue;
	}
	inter->in_loop = false;

	inter_env_pop(inter);
	return res;
}

Value eval_while(Interpreter* inter, Node node) {
	assert(node.children_count == 2);
	Node cond = node.children[0];
	Node exp = node.children[1];
	inter->in_loop = true;
	Value res;
	while (inter_eval_node(inter, cond).tag != VALUE_NIL) {
		res = inter_eval_node(inter, exp);
		if (inter->should_break) break;
		if (inter->should_continue) continue;
	}
	inter->in_loop = false;
	return res;
}

Value eval_ass(Interpreter* inter, Node node) {
	assert(node.children_count == 2);
	Node var = node.children[0];
	assert(var.type == AST_VAR);
	Node id = var.children[0];
	Value value = inter_eval_node(inter, node.children[1]);
	Value* addr = inter_env_find(inter, id.str_id);
	assert(addr && "Variable not found. Address is 0x0");

	if (var.children_count == 2) { // array indexing
		Value idx = inter_eval_node(inter, var.children[1]);
		assert(idx.tag == VALUE_NUM);
		assert((size_t)idx.num < addr->arr.len);
		return (addr->arr.data[idx.num] = value);
	}

	return (*addr = value); // simple access
}

Value eval_decl(Interpreter* inter, Node node) {
	// fun id params = exp
	if (node.children_count == 3) {
		Node id = node.children[0];
		Node params = node.children[1];
		Node body = node.children[2];
		Value* cell = inter_env_get_new(inter, id.str_id);
		return *cell = (Value) {
						 .tag = VALUE_FUN,
						 .func =
							 (Function) {
								 .is_builtin = false,
								 .argc = params.children_count,
								 .args = params.children,
								 .root = body
							 }
					 };
	}

	Node id = node.children[0];
	Value* cell = inter_env_get_new(inter, id.str_id);

	// var id = exp
	if (node.children_count == 2)
		return *cell = inter_eval_node(inter, node.children[1]);

	// var id
	return (Value) {VALUE_NIL, .nil = NULL};
}

Value eval_var(Interpreter* inter, Node node) {
	Node id = node.children[0];
	Value* addr = inter_env_find(inter, id.str_id);
	assert(addr && "Variable not previously declared.");

	if (node.children_count == 2) { // id [idx]
		Value idx = inter_eval_node(inter, node.children[1]);
		assert(
			addr->tag == VALUE_ARR
			&& "Variable does not correspond to a previously declared array"
		);
		assert(idx.tag == VALUE_NUM && "Index is not a number");
		return (Value) {VALUE_NUM, .num = addr->arr.data[idx.num].num};
	}

	return *addr; // id
}

Value inter_eval_node(Interpreter* inter, Node node) {
	Value val;
	switch (node.type) {
		case AST_NUM: return (Value) {VALUE_NUM, .num = node.num};
		case AST_APP: return eval_app(inter, node);
		case AST_BLK: return eval_block(inter, node);
		case AST_IF: return eval_if(inter, node);
		case AST_WHEN: return eval_when(inter, node);
		case AST_FOR: return eval_for(inter, node);
		case AST_WHILE: return eval_while(inter, node);
		case AST_BREAK: {
			assert(inter->in_loop && "INTERPRET_ERR: can't break outside of loops");
			inter->should_break = true;
			assert(
				node.children_count == 1
				&& "INTERPRET_ERR: break requires a expression to evaluate the loop to"
			);
			val = inter_eval_node(inter, node.children[0]);
			break;
		}
		case AST_CONTINUE: {
			assert(
				inter->in_loop && "INTERPRET_ERR: can't continue outside of loops"
			);
			inter->should_continue = true;
			assert(
				node.children_count == 1
				&& "INTERPRET_ERR: break requires a expression to evaluate the loop to"
			);
			val = inter_eval_node(inter, node.children[0]);
			break;
		}
		case AST_ASS: return eval_ass(inter, node);
		case AST_OR: {
			assert(node.children_count == 2);
			Value left = inter_eval_node(inter, node.children[0]);
			if (left.tag == VALUE_TRUE) {
				val = (Value) {VALUE_TRUE, .num = 1};
				break;
			}
			Value right = inter_eval_node(inter, node.children[1]);
			val = (right.tag == VALUE_NIL) ? (Value) {VALUE_NIL, .nil = NULL}
			                               : (Value) {VALUE_TRUE, .num = 1};
			break;
		}
		case AST_AND: {
			assert(node.children_count == 2);
			Value left = inter_eval_node(inter, node.children[0]);
			if (left.tag == VALUE_NIL) {
				val = (Value) {VALUE_NIL, .nil = NULL};
				break;
			}
			Value right = inter_eval_node(inter, node.children[1]);
			val = (right.tag == VALUE_NIL) ? (Value) {VALUE_NIL, .nil = NULL}
			                               : (Value) {VALUE_TRUE, .num = 1};
			break;
		}
#define ARITH_OP(OP)                                         \
	{                                                          \
		assert(node.children_count == 2);                        \
		Value left = inter_eval_node(inter, node.children[0]);   \
		Value right = inter_eval_node(inter, node.children[1]);  \
		assert(left.tag == VALUE_NUM && right.tag == VALUE_NUM); \
		val = (Value) {VALUE_NUM, .num = left.num OP right.num}; \
		break;                                                   \
	}
		case AST_ADD: ARITH_OP(+);
		case AST_SUB: ARITH_OP(-);
		case AST_MUL: ARITH_OP(*);
		case AST_DIV: ARITH_OP(/);
		case AST_MOD: ARITH_OP(%);
#undef ARITH_OP
#define CMP_OP(OP)                                                    \
	{                                                                   \
		assert(node.children_count == 2);                                 \
		Value left = inter_eval_node(inter, node.children[0]);            \
		Value right = inter_eval_node(inter, node.children[1]);           \
		assert(                                                           \
			left.tag == VALUE_NUM && right.tag == VALUE_NUM                 \
			&& "Arithmetic comparison is allowed only between numbers"      \
		);                                                                \
		val = (left.num OP right.num) ? (Value) {VALUE_TRUE, .num = 1}    \
		                              : (Value) {VALUE_NIL, .nil = NULL}; \
		break;                                                            \
	}
		case AST_GTN: CMP_OP(>);
		case AST_LTN: CMP_OP(<);
		case AST_GTE: CMP_OP(>=);
		case AST_LTE: CMP_OP(<=);
#undef CMP_OP
		case AST_EQ: {
			assert(node.children_count == 2);
			Value left = inter_eval_node(inter, node.children[0]);
			Value right = inter_eval_node(inter, node.children[1]);
			if (left.tag == VALUE_NIL && right.tag == VALUE_NIL) {
				val = (Value) {VALUE_TRUE, .num = 1};
				break;
			}

			if (left.tag == VALUE_TRUE && right.tag == VALUE_TRUE) {
				val = (Value) {VALUE_TRUE, .num = 1};
				break;
			}

			if (left.tag == VALUE_NUM && right.tag == VALUE_NUM) {
				val = (left.num == right.num) ? (Value) {VALUE_TRUE, .num = 1}
				                              : (Value) {VALUE_NIL, .nil = NULL};
				break;
			}

			val = (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case AST_NOT: {
			assert(node.children_count == 1);
			Node op = node.children[0];
			Value v = inter_eval_node(inter, op);
			val = (v.tag == VALUE_NIL) ? (Value) {VALUE_TRUE, .num = 1}
			                           : (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case AST_ID: assert(false);
		case AST_STR: {
			val = (Value
			) {VALUE_STR, .str = strdup(str_pool_find(inter->pool, node.str_id))};
			break;
		}
		case AST_DECL: return eval_decl(inter, node);
		case AST_VAR: return eval_var(inter, node);
		case AST_NIL: {
			val = (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case AST_TRUE: {
			val = (Value) {VALUE_TRUE, .num = 1};
			break;
		}
		case AST_LET: {
			Node decls = node.children[0];
			Node exp = node.children[1];
			inter_env_push(inter);

			for (size_t i = 0; i < decls.children_count; i++) {
				Node decl = decls.children[i];
				inter_eval_node(inter, decl);
			}

			val = inter_eval_node(inter, exp);
			inter_env_pop(inter);
			break;
		}
	}
	return val;
}

void print_value(Value val) {
	if (val.tag == VALUE_NUM)
		printf("%d", val.num);
	else if (val.tag == VALUE_STR)
		printf("%s", val.str);
	else if (val.tag == VALUE_TRUE)
		printf("1");
	else if (val.tag == VALUE_NIL)
		printf("0");
}

static void value_deinit(Value val) {
	if (val.tag == VALUE_STR) {
		free(val.str);
	} else if (val.tag == VALUE_ARR)
		free(val.arr.data);
}

static Value builtin_read(size_t _1, Value* _2) {
	(void)_1;
	(void)_2;
	char* buf = malloc(sizeof(char) * 100);
	assert(
		buf && "INTEPRET_ERR: couldn't allocate input buffer for `read' builtin"
	);
	if (fgets(buf, 100, stdin) == NULL) {
		free(buf);
		return (Value) {VALUE_NIL, .nil = NULL};
	}
	const size_t len = strlen(buf);
	buf[len - 1] = '\0';
	Number num = 0;
	Value val;
	if (sscanf(buf, "%d", &num) == 0)
		val = (Value) {VALUE_STR, .str = buf};
	else {
		free(buf);
		val = (Value) {VALUE_NUM, .num = num};
	}
	return val;
}

static Value builtin_write(size_t len, Value* args) {
	for (size_t i = 0; i < len; i++) print_value(args[i]);
	return (Value) {VALUE_NIL, .nil = NULL};
}

static Value builtin_array(size_t argc, Value* args) {
	assert(argc == 1 && "INTEPRET_ERR: array takes a single numeric argument");
	Number arr_len = args[0].num;
	assert(arr_len >= 0 && "INTERPRET_ERR: array length must be positive");
	Value val;
	val.tag = VALUE_ARR;
	val.arr.data = malloc(sizeof(Value) * (size_t)arr_len);
	assert(val.arr.data && "INTERPRET_ERR: dynamic memory allocation error");
	val.arr.len = (size_t)arr_len;
	return val;
}

static Value builtin_exit(size_t argc, Value* args) {
	assert(argc == 1 && "INTEPRET_ERR: exit takes exit code as a argument");
	Number exit_code = args[0].num;
	exit(exit_code);
	return (Value) {VALUE_NIL, .nil = NULL};
}

static void value_stack_pop(ValueStack* stack, size_t index) {
	value_deinit(stack->values[index]);
	stack->len--;
}

#define PUSH_BUILTIN(INTER, STR, FUNC)                                     \
	*inter_env_get_new(&INTER, str_pool_intern((INTER).pool, strdup(STR))) = \
		(Value) {                                                              \
		VALUE_FUN, .func = {.is_builtin = true, .builtin = FUNC }              \
	}

Interpreter interpreter_init(STR_POOL pool) {
	Interpreter inter;
	inter.values = (ValueStack) {0, 32, malloc(sizeof(Value) * 32)};
	inter.pool = pool;

	inter.env.len = 0;
	inter.env.cap = 32; // arbitrary choice
	inter.env.scope_count = 0;
	inter.env.indexes = malloc(sizeof(size_t) * inter.env.cap);
	inter.env.syms = malloc(sizeof(size_t) * inter.env.cap);
	inter.env.scopes = malloc(sizeof(size_t) * inter.env.cap);

	inter.in_loop = false;
	inter.should_break = false;
	inter.should_continue = false;

	inter_env_push(&inter);

	PUSH_BUILTIN(inter, "read", builtin_read);
	PUSH_BUILTIN(inter, "write", builtin_write);
	PUSH_BUILTIN(inter, "array", builtin_array);
	PUSH_BUILTIN(inter, "exit", builtin_exit);

	return inter;
}

void interpreter_deinit(Interpreter* inter) {
	for (size_t i = 0; i < inter->env.len; i++) {
		value_deinit(inter->values.values[i]);
		inter->values.len--;
	}
	free(inter->env.indexes);
	free(inter->env.scopes);
	free(inter->env.syms);
	free(inter->values.values);
}
