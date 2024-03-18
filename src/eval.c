#include "eval.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

void print_value(Value val);
static void value_deinit(Value val);

static void env_push(Environment* env);
static void env_pop(Environment* env);
static Value* env_find(Environment* env, size_t sym_idx);
static Value* env_get_new(Environment* env, size_t sym_idx);

static Value builtin_read(size_t len, Value* args);
static Value builtin_write(size_t len, Value args[len]);

static Value eval_ast_node(Node node, Environment* env, SymbolTable* tab);

static Value apply_function(
	SymbolTable* tab, Environment* env, Node func_node, Node args_node
);

Value ast_eval(Interpreter* inter, SymbolTable* syms, AST ast) {
	return eval_ast_node(ast.root, &inter->env, syms);
}

static Environment env_init() {
	Environment env;
	env.len = 0;
	env.cap = 32;
	env.scope_count = 0;
	env.vars = malloc(sizeof(Variable) * env.cap);
	env.values = malloc(sizeof(Value) * env.cap);
	return env;
}

static void env_deinit(Environment* env) {
	for (size_t i = 0; i < env->len; i++) value_deinit(env->values[i]);
	free(env->vars);
	free(env->values);
}

static void env_push(Environment* env) { env->scope_count++; }

// go from top to bottom of the stack and remove vars belonging to the current
// scope
static void env_pop(Environment* env) {
	assert(env->scope_count > 0 && "Environment was empty");
	for (size_t i = env->len; i-- > 0; env->len--) {
		if (env->vars[i].scope != (env->scope_count - 1)) break;
		value_deinit(env->values[i]);
	}
	env->scope_count--;
}

// search for sym in env. if not found return NULL
static Value* env_find(Environment* env, size_t sym_idx) {
	for (size_t i = env->len; i > 0;) {
		--i;
		if (env->vars[i].sym == sym_idx) return &env->values[i];
	}
	return NULL;
}

// add sym to environment and return a new value cell
static Value* env_get_new(Environment* env, size_t sym_idx) {
	env->vars[env->len] =
		(Variable) {.sym = sym_idx, .scope = env->scope_count - 1};
	return &env->values[env->len++];
}

static Value eval_ast_node(Node node, Environment* env, SymbolTable* tab) {
	Value val;
	switch (node.type) {
		case AST_NUM: return (Value) {VALUE_NUM, .num = node.num}; break;
		case AST_APP: {
			assert(node.children_count == 2);
			Node func = node.children[0];
			Node args = node.children[1];
			val = apply_function(tab, env, func, args);
			break;
		}
		case AST_BLK: {
			env_push(env);
			for (size_t i = 0; i < node.children_count; i++)
				val = eval_ast_node(node.children[i], env, tab);
			env_pop(env);
			break;
		}
		case AST_IF: {
			assert(node.children_count == 3);
			Value cond = eval_ast_node(node.children[0], env, tab);
			if (cond.tag != VALUE_NIL)
				val = eval_ast_node(node.children[1], env, tab);
			else
				val = eval_ast_node(node.children[2], env, tab);
			break;
		}
		case AST_WHEN: {
			assert(node.children_count == 2);
			Value cond = eval_ast_node(node.children[0], env, tab);
			val = (cond.tag != VALUE_NIL) ? eval_ast_node(node.children[1], env, tab)
			                              : (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case AST_FOR: {
			env_push(env);

			Node var = node.children[0];
			Node id = var.children[0];
			Value from = eval_ast_node(node.children[1], env, tab);
			Value to = eval_ast_node(node.children[2], env, tab);
			assert(from.tag == VALUE_NUM && to.tag == VALUE_NUM);
			Node exp = node.children[3];

			int inc = (from.num <= to.num) ? 1 : -1;

			Value* addr = env_get_new(env, id.index);
			for (Number i = from.num; (i - inc) != to.num; i += inc) {
				*addr = (Value) {VALUE_NUM, .num = i};
				val = eval_ast_node(exp, env, tab);
			}

			env_pop(env);
			break;
		}
		case AST_WHILE: {
			assert(node.children_count == 2);
			Node cond = node.children[0];
			Node exp = node.children[1];
			while (eval_ast_node(cond, env, tab).tag != VALUE_NIL)
				val = eval_ast_node(exp, env, tab);
			break;
		}
		case AST_ASS: {
			assert(node.children_count == 2);
			Node var = node.children[0];
			assert(var.type == AST_VAR);
			Node id = var.children[0];
			Value value = eval_ast_node(node.children[1], env, tab);
			Value* addr = env_find(env, id.index);
			assert(addr && "Variable not found. Address is 0x0");

			if (var.children_count == 2) { // array indexing
				Value idx = eval_ast_node(var.children[1], env, tab);
				assert(idx.tag == VALUE_NUM);
				val = (addr->arr.data[idx.num] = value);
			} else { // plain
				val = (*addr = value);
			}
			break;
		}
		case AST_OR: {
			assert(node.children_count == 2);
			Value left = eval_ast_node(node.children[0], env, tab);
			if (left.tag == VALUE_TRUE) {
				val = (Value) {VALUE_TRUE, .num = 1};
				break;
			}
			Value right = eval_ast_node(node.children[1], env, tab);
			val = (right.tag == VALUE_NIL) ? (Value) {VALUE_NIL, .nil = NULL}
			                               : (Value) {VALUE_TRUE, .num = 1};
			break;
		}
		case AST_AND: {
			assert(node.children_count == 2);
			Value left = eval_ast_node(node.children[0], env, tab);
			if (left.tag == VALUE_NIL) {
				val = (Value) {VALUE_NIL, .nil = NULL};
				break;
			}
			Value right = eval_ast_node(node.children[1], env, tab);
			val = (right.tag == VALUE_NIL) ? (Value) {VALUE_NIL, .nil = NULL}
			                               : (Value) {VALUE_TRUE, .num = 1};
			break;
		}
#define ARITH_OP(OP)                                         \
	{                                                          \
		assert(node.children_count == 2);                        \
		Value left = eval_ast_node(node.children[0], env, tab);  \
		Value right = eval_ast_node(node.children[1], env, tab); \
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
		Value left = eval_ast_node(node.children[0], env, tab);           \
		Value right = eval_ast_node(node.children[1], env, tab);          \
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
			Value left = eval_ast_node(node.children[0], env, tab);
			Value right = eval_ast_node(node.children[1], env, tab);
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
			Value v = eval_ast_node(op, env, tab);
			val = (v.tag == VALUE_NIL) ? (Value) {VALUE_TRUE, .num = 1}
			                           : (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case AST_ID: assert(false);
		case AST_STR: {
			val = (Value) {VALUE_STR, .str = sym_table_get(tab, node.index)};
			break;
		}
		case AST_DECL: {
			Node var = node.children[0];
			Node id = var.children[0];
			Value* cell = env_get_new(env, id.index);

			// var id = exp
			if (node.children_count == 2) {
				val = *cell = eval_ast_node(node.children[1], env, tab);
				break;
			}

			// var arr [size]
			if (var.children_count == 2) {
				Value size = eval_ast_node(var.children[1], env, tab);
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
			Value* addr = env_find(env, id.index);
			assert(addr && "Variable not previously declared.");

			if (node.children_count == 1) { // id
				val = *addr;
			} else { // id[idx]
				Value idx = eval_ast_node(node.children[1], env, tab);
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
			env_push(env);

			for (size_t i = 0; i < decls.children_count; i++) {
				Node decl = decls.children[i];
				eval_ast_node(decl, env, tab);
			}

			val = eval_ast_node(exp, env, tab);
			env_pop(env);
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
		printf("%p\n", val.str);
		free(val.str);

	} else if (val.tag == VALUE_ARR)
		free(val.arr.data);
}

static Value apply_function(
	SymbolTable* tab, Environment* env, Node func_node, Node args_node
) {
	assert(func_node.type == AST_ID && "Unnamed functions are not implemented");

	Value* func_ptr = env_find(env, func_node.index);
	assert(func_ptr && "For now, only read and write builtins are implemented");
	Funktion func = (Funktion)func_ptr->func;

	Value* args = malloc(sizeof(Value) * args_node.children_count);
	for (size_t i = 0; i < args_node.children_count; i++)
		args[i] = eval_ast_node(args_node.children[i], env, tab);

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

Interpreter interpreter_init(SymbolTable* syms) {
	Interpreter inter;
	inter.env = env_init();
	env_push(&inter.env);
	*env_get_new(&inter.env, sym_table_insert(syms, "read")) =
		(Value) {VALUE_FUN, .func = builtin_read};
	*env_get_new(&inter.env, sym_table_insert(syms, "write")) =
		(Value) {VALUE_FUN, .func = builtin_write};
	return inter;
}

void interpreter_deinit(Interpreter* inter) {
	env_pop(&inter->env);
	env_deinit(&inter->env);
}
