#include "eval.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "env.h"

static void value_deinit(Value val);
static void value_stack_pop(ValueStack* stack, size_t index);

static void inter_env_push(Interpreter* inter);
static void inter_env_pop(Interpreter* inter);
static Value* inter_env_get_new(Interpreter* inter, size_t sym_index);
static Value* inter_env_find(Interpreter* inter, size_t sym_index);

static Value builtin_read(size_t len, Value* args);
static Value builtin_write(size_t len, Value args[len]);

static Value ast_node_eval(Interpreter* inter, Node node);
static Value apply_function(Interpreter* inter, Node func_node, Node args_node);

static void inter_env_push(Interpreter* inter) { env_push(&inter->env); }

static void inter_env_pop(Interpreter* inter) {
	env_pop(
		&inter->env, (void (*)(void*, size_t))value_stack_pop, &inter->values
	);
}

static Value* inter_env_get_new(Interpreter* inter, size_t sym_index) {
	size_t idx = env_get_new(&inter->env, sym_index);
	inter->values.len++;
	return &inter->values.values[idx];
}

static Value* inter_env_find(Interpreter* inter, size_t sym_index) {
	bool found = true;
	size_t idx = env_find(&inter->env, sym_index, &found);
	if (!found) return NULL;
	return &inter->values.values[idx];
}

Value ast_eval(Interpreter* inter, AST ast) {
	return ast_node_eval(inter, ast.root);
}

static Value ast_node_eval(Interpreter* inter, Node node) {
	Value val;
	switch (node.type) {
		case AST_NUM: return (Value) {VALUE_NUM, .num = node.num}; break;
		case AST_APP: {
			assert(node.children_count == 2);
			Node func = node.children[0];
			Node args = node.children[1];
			val = apply_function(inter, func, args);
			break;
		}
		case AST_BLK: {
			inter_env_push(inter);
			for (size_t i = 0; i < node.children_count; i++)
				val = ast_node_eval(inter, node.children[i]);
			inter_env_pop(inter);
			break;
		}
		case AST_IF: {
			assert(node.children_count == 3);
			Value cond = ast_node_eval(inter, node.children[0]);
			if (cond.tag != VALUE_NIL)
				val = ast_node_eval(inter, node.children[1]);
			else
				val = ast_node_eval(inter, node.children[2]);
			break;
		}
		case AST_WHEN: {
			assert(node.children_count == 2);
			Value cond = ast_node_eval(inter, node.children[0]);
			val = (cond.tag != VALUE_NIL) ? ast_node_eval(inter, node.children[1])
			                              : (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case AST_FOR: {
			inter_env_push(inter);

			Node var = node.children[0];
			Node id = var.children[0];
			Value from = ast_node_eval(inter, node.children[1]);
			Value to = ast_node_eval(inter, node.children[2]);
			assert(from.tag == VALUE_NUM && to.tag == VALUE_NUM);
			Node exp = node.children[3];

			int inc = (from.num <= to.num) ? 1 : -1;

			Value* addr = inter_env_get_new(inter, id.index);
			for (Number i = from.num; (i - inc) != to.num; i += inc) {
				*addr = (Value) {VALUE_NUM, .num = i};
				val = ast_node_eval(inter, exp);
			}

			inter_env_pop(inter);
			break;
		}
		case AST_WHILE: {
			assert(node.children_count == 2);
			Node cond = node.children[0];
			Node exp = node.children[1];
			while (ast_node_eval(inter, cond).tag != VALUE_NIL)
				val = ast_node_eval(inter, exp);
			break;
		}
		case AST_ASS: {
			assert(node.children_count == 2);
			Node var = node.children[0];
			assert(var.type == AST_VAR);
			Node id = var.children[0];
			Value value = ast_node_eval(inter, node.children[1]);
			Value* addr = inter_env_find(inter, id.index);
			assert(addr && "Variable not found. Address is 0x0");

			if (var.children_count == 2) { // array indexing
				Value idx = ast_node_eval(inter, var.children[1]);
				assert(idx.tag == VALUE_NUM);
				val = (addr->arr.data[idx.num] = value);
			} else { // plain
				val = (*addr = value);
			}
			break;
		}
		case AST_OR: {
			assert(node.children_count == 2);
			Value left = ast_node_eval(inter, node.children[0]);
			if (left.tag == VALUE_TRUE) {
				val = (Value) {VALUE_TRUE, .num = 1};
				break;
			}
			Value right = ast_node_eval(inter, node.children[1]);
			val = (right.tag == VALUE_NIL) ? (Value) {VALUE_NIL, .nil = NULL}
			                               : (Value) {VALUE_TRUE, .num = 1};
			break;
		}
		case AST_AND: {
			assert(node.children_count == 2);
			Value left = ast_node_eval(inter, node.children[0]);
			if (left.tag == VALUE_NIL) {
				val = (Value) {VALUE_NIL, .nil = NULL};
				break;
			}
			Value right = ast_node_eval(inter, node.children[1]);
			val = (right.tag == VALUE_NIL) ? (Value) {VALUE_NIL, .nil = NULL}
			                               : (Value) {VALUE_TRUE, .num = 1};
			break;
		}
#define ARITH_OP(OP)                                         \
	{                                                          \
		assert(node.children_count == 2);                        \
		Value left = ast_node_eval(inter, node.children[0]);     \
		Value right = ast_node_eval(inter, node.children[1]);    \
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
		Value left = ast_node_eval(inter, node.children[0]);              \
		Value right = ast_node_eval(inter, node.children[1]);             \
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
			Value left = ast_node_eval(inter, node.children[0]);
			Value right = ast_node_eval(inter, node.children[1]);
			if (left.tag == VALUE_NIL && right.tag == VALUE_NIL) {
				val = (Value) {VALUE_TRUE, .num = 1};
				break;
			}

			if (left.tag == VALUE_TRUE && right.tag == VALUE_TRUE) {
				val = (Value) {VALUE_TRUE, .num = 1};
				break;
			}

			assert(
				left.tag == VALUE_NUM && right.tag == VALUE_NUM
				&& "Only nil, true and number comparison are implemented"
			);
			val = (left.num == right.num) ? (Value) {VALUE_TRUE, .num = 1}
			                              : (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case AST_NOT: {
			assert(node.children_count == 1);
			Node op = node.children[0];
			Value v = ast_node_eval(inter, op);
			val = (v.tag == VALUE_NIL) ? (Value) {VALUE_TRUE, .num = 1}
			                           : (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case AST_ID: assert(false);
		case AST_STR: {
			val = (Value) {VALUE_STR, .str = sym_table_get(&inter->syms, node.index)};
			break;
		}
		case AST_DECL: {
			Node var = node.children[0];
			Node id = var.children[0];
			Value* cell = inter_env_get_new(inter, id.index);

			// var id = exp
			if (node.children_count == 2) {
				val = *cell = ast_node_eval(inter, node.children[1]);
				break;
			}

			// var arr [size]
			if (var.children_count == 2) {
				Value size = ast_node_eval(inter, var.children[1]);
				*cell = (Value) {
					VALUE_ARR,
					.arr.data = malloc(sizeof(Value) * size.num),
					.arr.len = size.num};
				memset(cell->arr.data, 0, sizeof(Value) * size.num);
			}

			// var id and var arr [size]
			// no value to return
			val = (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case AST_VAR: {
			Node id = node.children[0];
			Value* addr = inter_env_find(inter, id.index);
			assert(addr && "Variable not previously declared.");

			if (node.children_count == 1) { // id
				val = *addr;
			} else { // id[idx]
				Value idx = ast_node_eval(inter, node.children[1]);
				assert(
					addr->tag == VALUE_ARR
					&& "Variable does not correspond to a previously declared array"
				);
				assert(idx.tag == VALUE_NUM && "Index is not a number");
				val = (Value) {VALUE_NUM, .num = addr->arr.data[idx.num].num};
			}
			break;
		}
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
				ast_node_eval(inter, decl);
			}

			val = ast_node_eval(inter, exp);
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
		printf("true");
	else if (val.tag == VALUE_NIL)
		printf("nil");
}

static void value_deinit(Value val) {
	if (val.tag == VALUE_STR) {
		free(val.str);
	} else if (val.tag == VALUE_ARR)
		free(val.arr.data);
}

static Value apply_function(
	Interpreter* inter, Node func_node, Node args_node
) {
	assert(func_node.type == AST_ID && "Unnamed functions are not implemented");

	Value* func_ptr = inter_env_find(inter, func_node.index);
	assert(func_ptr && "For now, only read and write builtins are implemented");
	Funktion func = (Funktion)func_ptr->func;

	Value* args = malloc(sizeof(Value) * args_node.children_count);
	for (size_t i = 0; i < args_node.children_count; i++)
		args[i] = ast_node_eval(inter, args_node.children[i]);

	Value val = func(args_node.children_count, args);
	free(args);
	return val;
}

static Value builtin_read(size_t _1, Value* _2) {
	(void)_1;
	(void)_2;
	char* buf = malloc(sizeof(char) * 100);
	if (fgets(buf, 100, stdin) == NULL) {
		free(buf);
		return (Value) {VALUE_NIL, .nil = NULL};
	}
	const size_t len = strlen(buf);
	buf[len - 1] = '\0';
	long num = 0;
	Value val;
	if (sscanf(buf, "%ld", &num) == 0)
		val = (Value) {VALUE_STR, .str = buf};
	else {
		free(buf);
		val = (Value) {VALUE_NUM, .num = num};
	}
	return val;
}

static Value builtin_write(size_t len, Value args[len]) {
	for (size_t i = 0; i < len; i++) print_value(args[i]);
	return (Value) {VALUE_NIL, .nil = NULL};
}

static void value_stack_pop(ValueStack* stack, size_t index) {
	value_deinit(stack->values[index]);
	stack->len--;
}

Interpreter interpreter_init() {
	Interpreter inter;
	inter.values.values = malloc(sizeof(Value) * 32);
	inter.values.cap = 32;
	inter.values.len = 0;
	inter.syms = sym_table_init();
	inter.env = env_init();
	inter_env_push(&inter);
	*inter_env_get_new(&inter, sym_table_insert(&inter.syms, "read")) =
		(Value) {VALUE_FUN, .func = builtin_read};
	*inter_env_get_new(&inter, sym_table_insert(&inter.syms, "write")) =
		(Value) {VALUE_FUN, .func = builtin_write};
	return inter;
}

void interpreter_deinit(Interpreter* inter) {
	free(inter->values.values);
	sym_table_deinit(&inter->syms);
	env_deinit(
		&inter->env, (void (*)(void*, size_t))value_stack_pop, &inter->values
	);
}
