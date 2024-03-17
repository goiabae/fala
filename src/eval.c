#include "eval.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_value(Value val);
static void value_deinit(Value val);

static Value* env_stack_find(EnvironmentStack stack, const char* name);
static void env_stack_push(EnvironmentStack* stack);
static void env_stack_pop(EnvironmentStack* stack);
static Value* env_stack_get_new(EnvironmentStack* stack, const char* name);

static Value builtin_read(size_t len, Value* args);
static Value builtin_write(size_t len, Value* args);

static Value eval_ast_node(Node node, EnvironmentStack stack, SymbolTable* tab);

static Value apply_function(
	SymbolTable* tab, EnvironmentStack* stack, Node func_node, Node args_node
);

static VarTable var_table_init(void) {
	VarTable tab;
	tab.len = 0;
	tab.cap = 100;

	tab.names = malloc(sizeof(char*) * tab.cap);
	memset(tab.names, 0, sizeof(char*) * tab.cap);

	tab.values = malloc(sizeof(int) * tab.cap);
	memset(tab.values, 0, sizeof(int) * tab.cap);

	return tab;
}

static void var_table_deinit(VarTable tab) {
	free(tab.names);
	for (size_t i = 0; i < tab.len; i++) value_deinit(tab.values[i]);
	free(tab.values);
}

// retrieve pointer to location of variable. if variable doesn't exists returns
// a new location
static Value* var_table_get_where(VarTable* tab, const char* name) {
	for (size_t i = 0; i < tab->len; i++)
		if (strcmp(tab->names[i], name) == 0) return &tab->values[i];
	return NULL;
}

// return is the exit code of the ran program
Value ast_eval(Interpreter* inter, SymbolTable* syms, AST ast) {
	Value val = eval_ast_node(ast.root, inter->envs, syms);
	return val;
}

static EnvironmentStack env_stack_init() {
	EnvironmentStack stack;
	stack.len = 0;
	stack.envs = malloc(sizeof(Environment) * 16);
	stack.cap = 16;
	return stack;
}

static void env_stack_deinit(EnvironmentStack stack) {
	for (size_t i = 0; i < stack.len; i++) var_table_deinit(stack.envs[i].vars);
	free(stack.envs);
}

// search environment stack for a variable cell with name.
// if doesn't find, return a new one in the inner most environment
static Value* env_stack_find(EnvironmentStack stack, const char* name) {
	for (size_t i = stack.len; i > 0;) {
		i -= 1;
		Value* addr = var_table_get_where(&stack.envs[i].vars, name);
		if (addr) return addr;
	}
	return NULL;
}

// returns the address of a new cell in the inner most scope
static Value* env_stack_get_new(EnvironmentStack* stack, const char* name) {
	VarTable* last = &stack->envs[stack->len - 1].vars;
	last->names[last->len] = name;
	return &last->values[last->len++];
}

// pushes a new inner most scope
static void env_stack_push(EnvironmentStack* stack) {
	stack->envs[stack->len++] = (Environment) {.vars = var_table_init()};
}

// pops the inner most scope
static void env_stack_pop(EnvironmentStack* stack) {
	var_table_deinit(stack->envs[stack->len-- - 1].vars);
}

static Value eval_ast_node(
	Node node, EnvironmentStack stack, SymbolTable* tab
) {
	Value val;
	switch (node.type) {
		case FALA_NUM: return (Value) {VALUE_NUM, .num = node.num}; break;
		case FALA_APP: {
			assert(node.children_count == 2);
			Node func = node.children[0];
			Node args = node.children[1];
			val = apply_function(tab, &stack, func, args);
			break;
		}
		case FALA_BLOCK: {
			env_stack_push(&stack);
			for (size_t i = 0; i < node.children_count; i++)
				val = eval_ast_node(node.children[i], stack, tab);
			env_stack_pop(&stack);
			break;
		}
		case FALA_IF: {
			assert(node.children_count == 3);
			Value cond = eval_ast_node(node.children[0], stack, tab);
			if (cond.tag != VALUE_NIL)
				val = eval_ast_node(node.children[1], stack, tab);
			else
				val = eval_ast_node(node.children[2], stack, tab);
			break;
		}
		case FALA_WHEN: {
			assert(node.children_count == 2);
			Value cond = eval_ast_node(node.children[0], stack, tab);
			val = (cond.tag != VALUE_NIL)
			      ? eval_ast_node(node.children[1], stack, tab)
			      : (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case FALA_FOR: {
			env_stack_push(&stack);

			Node var = node.children[0];
			Node id = var.children[0];
			Value from = eval_ast_node(node.children[1], stack, tab);
			Value to = eval_ast_node(node.children[2], stack, tab);
			assert(from.tag == VALUE_NUM && to.tag == VALUE_NUM);
			Node exp = node.children[3];

			int inc = (from.num <= to.num) ? 1 : -1;

			Value* addr = env_stack_find(stack, tab->arr[id.index]);
			if (!addr) addr = env_stack_get_new(&stack, tab->arr[id.index]);

			for (Number i = from.num; (i - inc) != to.num; i += inc) {
				*addr = (Value) {VALUE_NUM, .num = i};
				val = eval_ast_node(exp, stack, tab);
			}

			env_stack_pop(&stack);
			break;
		}
		case FALA_WHILE: {
			assert(node.children_count == 2);
			Node cond = node.children[0];
			Node exp = node.children[1];
			while (eval_ast_node(cond, stack, tab).tag != VALUE_NIL)
				val = eval_ast_node(exp, stack, tab);
			break;
		}
		case FALA_ASS: {
			assert(node.children_count == 2);
			Node var = node.children[0];
			assert(var.type == FALA_VAR);
			Node id = var.children[0];
			Value value = eval_ast_node(node.children[1], stack, tab);
			Value* addr = env_stack_find(stack, sym_table_get(tab, id.index));
			assert(addr && "Variable not found. Address is 0x0");

			if (var.children_count == 2) { // array indexing
				Value idx = eval_ast_node(var.children[1], stack, tab);
				assert(idx.tag == VALUE_NUM);
				val = (addr->arr.data[idx.num] = value);
			} else { // plain
				val = (*addr = value);
			}
			break;
		}
		case FALA_OR: {
			assert(node.children_count == 2);
			Value left = eval_ast_node(node.children[0], stack, tab);
			if (left.tag == VALUE_TRUE) {
				val = (Value) {VALUE_TRUE, .num = 1};
				break;
			}
			Value right = eval_ast_node(node.children[1], stack, tab);
			val = (right.tag == VALUE_NIL) ? (Value) {VALUE_NIL, .nil = NULL}
			                               : (Value) {VALUE_TRUE, .num = 1};
			break;
		}
		case FALA_AND: {
			assert(node.children_count == 2);
			Value left = eval_ast_node(node.children[0], stack, tab);
			if (left.tag == VALUE_NIL) {
				val = (Value) {VALUE_NIL, .nil = NULL};
				break;
			}
			Value right = eval_ast_node(node.children[1], stack, tab);
			val = (right.tag == VALUE_NIL) ? (Value) {VALUE_NIL, .nil = NULL}
			                               : (Value) {VALUE_TRUE, .num = 1};
			break;
		}
#define ARITH_OP(OP)                                           \
	{                                                            \
		assert(node.children_count == 2);                          \
		Value left = eval_ast_node(node.children[0], stack, tab);  \
		Value right = eval_ast_node(node.children[1], stack, tab); \
		assert(left.tag == VALUE_NUM && right.tag == VALUE_NUM);   \
		val = (Value) {VALUE_NUM, .num = left.num OP right.num};   \
		break;                                                     \
	}
		case FALA_ADD: ARITH_OP(+);
		case FALA_SUB: ARITH_OP(-);
		case FALA_MUL: ARITH_OP(*);
		case FALA_DIV: ARITH_OP(/);
		case FALA_MOD: ARITH_OP(%);
#undef ARITH_OP
#define CMP_OP(OP)                                                    \
	{                                                                   \
		assert(node.children_count == 2);                                 \
		Value left = eval_ast_node(node.children[0], stack, tab);         \
		Value right = eval_ast_node(node.children[1], stack, tab);        \
		assert(                                                           \
			left.tag == VALUE_NUM && right.tag == VALUE_NUM                 \
			&& "Arithmetic comparison is allowed only between numbers"      \
		);                                                                \
		val = (left.num OP right.num) ? (Value) {VALUE_TRUE, .num = 1}    \
		                              : (Value) {VALUE_NIL, .nil = NULL}; \
		break;                                                            \
	}
		case FALA_GREATER: CMP_OP(>);
		case FALA_LESSER: CMP_OP(<);
		case FALA_GREATER_EQ: CMP_OP(>=);
		case FALA_LESSER_EQ: CMP_OP(<=);
#undef CMP_OP
		case FALA_EQ: {
			assert(node.children_count == 2);
			Value left = eval_ast_node(node.children[0], stack, tab);
			Value right = eval_ast_node(node.children[1], stack, tab);
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
		case FALA_NOT: {
			assert(node.children_count == 1);
			Node op = node.children[0];
			Value v = eval_ast_node(op, stack, tab);
			val = (v.tag == VALUE_NIL) ? (Value) {VALUE_TRUE, .num = 1}
			                           : (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case FALA_ID: assert(false);
		case FALA_STRING: {
			val = (Value) {VALUE_STR, .str = sym_table_get(tab, node.index)};
			break;
		}
		case FALA_DECL: {
			Node var = node.children[0];
			Node id = var.children[0];
			Value* cell = env_stack_get_new(&stack, sym_table_get(tab, id.index));

			// var id = exp
			if (node.children_count == 2) {
				val = *cell = eval_ast_node(node.children[1], stack, tab);
				break;
			}

			// var arr [size]
			if (var.children_count == 2) {
				Value size = eval_ast_node(var.children[1], stack, tab);
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
		case FALA_VAR: {
			Node id = node.children[0];
			Value* addr = env_stack_find(stack, sym_table_get(tab, id.index));
			assert(addr && "Variable not previously declared.");

			if (node.children_count == 1) { // id
				val = *addr;
			} else { // id[idx]
				Value idx = eval_ast_node(node.children[1], stack, tab);
				assert(
					addr->tag == VALUE_ARR
					&& "Variable does not correspond to a previously declared array"
				);
				assert(idx.tag == VALUE_NUM && "Index is not a number");
				val = (Value) {VALUE_NUM, .num = addr->arr.data[idx.num].num};
			}
			break;
		}
		case FALA_NIL: {
			val = (Value) {VALUE_NIL, .nil = NULL};
			break;
		}
		case FALA_TRUE: {
			val = (Value) {VALUE_TRUE, .num = 1};
			break;
		}
		case FALA_LET: {
			Node decls = node.children[0];
			Node exp = node.children[1];
			env_stack_push(&stack);

			for (size_t i = 0; i < decls.children_count; i++) {
				Node decl = decls.children[i];
				eval_ast_node(decl, stack, tab);
			}

			val = eval_ast_node(exp, stack, tab);
			env_stack_pop(&stack);
			break;
		}
	}
	return val;
}

static void print_value(Value val) {
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
	SymbolTable* tab, EnvironmentStack* stack, Node func_node, Node args_node
) {
	assert(func_node.type == FALA_ID && "Unnamed functions are not implemented");

	String func_name = sym_table_get(tab, func_node.index);
	Value* func_ptr = env_stack_find(*stack, func_name);
	assert(func_ptr && "For now, only read and write builtins are implemented");
	Funktion func = (Funktion)func_ptr->func;

	Value* args = malloc(sizeof(Value) * args_node.children_count);
	for (size_t i = 0; i < args_node.children_count; i++)
		args[i] = eval_ast_node(args_node.children[i], *stack, tab);

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

static Value builtin_write(size_t len, Value* args) {
	assert(len == 1 && "Wrong number of arguments. 1 expected.");
	print_value(args[0]);
	return args[0];
}

Interpreter interpreter_init(void) {
	Interpreter inter;
	inter.envs = env_stack_init();
	env_stack_push(&inter.envs);
	*env_stack_get_new(&inter.envs, "read") =
		(Value) {VALUE_FUN, .func = builtin_read};
	*env_stack_get_new(&inter.envs, "write") =
		(Value) {VALUE_FUN, .func = builtin_write};
	return inter;
}

void interpreter_deinit(Interpreter* inter) {
	env_stack_pop(&inter->envs);
	env_stack_deinit(inter->envs);
}
