#include "main.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#	include <getopt.h>
#endif

// clang-format off
// need to be included in this order
#include "parser.h"
#include "lexer.h"
// clang-format on

typedef struct Interpreter {
	EnvironmentStack envs;
} Interpreter;

static void print_value(Value val);
static void value_deinit(Value val);

static Value* env_stack_find(EnvironmentStack stack, const char* name);
static void env_stack_push(EnvironmentStack* stack);
static void env_stack_pop(EnvironmentStack* stack);
static Value* env_stack_get_new(EnvironmentStack* stack, const char* name);

Node new_node(Type type, size_t len, Node children[len]) {
	Node node;
	node.type = type;
	node.children_count = len;
	node.children = NULL;
	if (len > 0) {
		node.children = malloc(sizeof(Node) * len);
		memcpy(node.children, children, sizeof(Node) * len);
	}
	return node;
}

Node new_list_node() {
	Node node;
	node.type = FALA_BLOCK;
	node.children_count = 0;
	node.children = malloc(sizeof(Node) * 100);
	return node;
}

Node list_append_node(Node list, Node next) {
	list.children[list.children_count++] = next;
	return list;
}

static size_t sym_table_insert(SymbolTable* tab, String str) {
	for (size_t i = 0; i < tab->len; i++) {
		if (strcmp(tab->arr[i], str) == 0) {
			free(str);
			return i;
		}
	}
	tab->arr[tab->len++] = str;
	assert(tab->len <= tab->cap && "Symbol table if full");
	return tab->len - 1;
}

static String sym_table_get(SymbolTable* tab, size_t index) {
	return tab->arr[index];
}

Node new_string_node(Type type, SymbolTable* tab, String str) {
	Node node;
	node.index = sym_table_insert(tab, str);
	node.type = type;
	return node;
}

Node new_number_node(Number num) {
	Node node;
	node.type = FALA_NUM;
	node.num = num;
	return node;
}

VarTable var_table_init(void) {
	VarTable tab;
	tab.len = 0;
	tab.cap = 100;

	tab.names = malloc(sizeof(char*) * tab.cap);
	memset(tab.names, 0, sizeof(char*) * tab.cap);

	tab.values = malloc(sizeof(int) * tab.cap);
	memset(tab.values, 0, sizeof(int) * tab.cap);

	return tab;
}

void var_table_deinit(VarTable tab) {
	free(tab.names);
	for (size_t i = 0; i < tab.len; i++) value_deinit(tab.values[i]);
	free(tab.values);
}

Value var_table_insert(VarTable* tab, const char* name, Value value) {
	for (size_t i = 0; i < tab->len; i++)
		if (strcmp(tab->names[i], name) == 0) return (tab->values[i] = value);

	// if name not in table
	tab->names[tab->len] = name;
	tab->values[tab->len] = value;
	tab->len++;
	return value;
}

Value var_table_get(VarTable* tab, const char* name) {
	for (size_t i = 0; i < tab->len; i++)
		if (strcmp(tab->names[i], name) == 0) return tab->values[i];
	assert(false); // FIXME should propagate error
}

// retrieve pointer to location of variable. if variable doesn't exists returns
// a new location
Value* var_table_get_where(VarTable* tab, const char* name) {
	for (size_t i = 0; i < tab->len; i++)
		if (strcmp(tab->names[i], name) == 0) return &tab->values[i];
	return NULL;
}

void yyerror(void* scanner, Context* ctx, SymbolTable* syms, char* err_msg) {
	(void)scanner;
	(void)ctx;
	(void)syms;
	fprintf(stderr, "%s\n", err_msg);
}

static char* node_repr(Type type) {
	switch (type) {
		case FALA_BLOCK: return "do-end";
		case FALA_IF: return "if";
		case FALA_WHEN: return "when";
		case FALA_FOR: return "for";
		case FALA_WHILE: return "while";
		case FALA_IN: return "in";
		case FALA_OUT: return "out";
		case FALA_ASS: return "=";
		case FALA_OR: return "or";
		case FALA_AND: return "and";
		case FALA_GREATER: return ">";
		case FALA_LESSER: return "<";
		case FALA_GREATER_EQ: return ">=";
		case FALA_LESSER_EQ: return "<=";
		case FALA_EQ_EQ: return "==";
		case FALA_ADD: return "+";
		case FALA_SUB: return "-";
		case FALA_MUL: return "*";
		case FALA_DIV: return "/";
		case FALA_MOD: return "%";
		case FALA_NOT: return "!";
		case FALA_NUM: return NULL;
		case FALA_ID: return NULL;
		case FALA_STRING: return NULL;
		case FALA_DECL: return "decl";
		case FALA_VAR: return "var";
	}
	assert(false);
}

static void print_node(SymbolTable* tab, Node node, unsigned int space) {
	if (node.type == FALA_NUM) {
		printf("%d", node.num);
		return;
	} else if (node.type == FALA_ID) {
		printf("%s", sym_table_get(tab, node.index));
		return;
	} else if (node.type == FALA_STRING) {
		printf("\"");
		for (char* it = sym_table_get(tab, node.index); *it != '\0'; it++) {
			if (*it == '\n')
				printf("\\n");
			else
				printf("%c", *it);
		}
		printf("\"");
		return;
	}

	printf("(");

	printf("%s", node_repr(node.type));

	space += 2;

	for (size_t i = 0; i < node.children_count; i++) {
		printf("\n");
		for (size_t j = 0; j < space; j++) printf(" ");
		print_node(tab, node.children[i], space);
	}

	printf(")");
}

static void print_ast(AST ast, SymbolTable* syms) {
	print_node(syms, ast.root, 0);
}

static void node_deinit(Node node) {
	if (node.type == FALA_ID || node.type == FALA_NUM || node.type == FALA_STRING)
		return;
	for (size_t i = 0; i < node.children_count; i++)
		node_deinit(node.children[i]);
	free(node.children);
}

static AST ast_init(void) { return (AST) {}; }
static void ast_deinit(AST ast) { node_deinit(ast.root); }

static Value ast_node_eval(
	Node node, EnvironmentStack stack, SymbolTable* tab
) {
	Value val;

#define BIN_OP(OP)                                             \
	{                                                            \
		assert(node.children_count == 2);                          \
		Value left = ast_node_eval(node.children[0], stack, tab);  \
		Value right = ast_node_eval(node.children[1], stack, tab); \
		assert(left.tag == VALUE_NUM && right.tag == VALUE_NUM);   \
		val = (Value) {VALUE_NUM, .num = left.num OP right.num};   \
		break;                                                     \
	}

	switch (node.type) {
		case FALA_NUM: return (Value) {VALUE_NUM, .num = node.num}; break;
		case FALA_BLOCK: {
			env_stack_push(&stack);
			for (size_t i = 0; i < node.children_count; i++)
				val = ast_node_eval(node.children[i], stack, tab);
			env_stack_pop(&stack);
			break;
		}
		case FALA_IF: {
			assert(node.children_count == 3);
			Value cond = ast_node_eval(node.children[0], stack, tab);
			assert(cond.tag == VALUE_NUM);
			if (cond.num)
				val = ast_node_eval(node.children[1], stack, tab);
			else
				val = ast_node_eval(node.children[2], stack, tab);
			break;
		}
		case FALA_WHEN: {
			assert(node.children_count == 2);
			Value cond = ast_node_eval(node.children[0], stack, tab);
			assert(cond.tag == VALUE_NUM);
			if (cond.num)
				val = ast_node_eval(node.children[1], stack, tab);
			else
				val = (Value) {VALUE_NUM, .num = 0};
			break;
		}
		case FALA_FOR: {
			env_stack_push(&stack);

			Node var = node.children[0];
			Node id = var.children[0];
			Value from = ast_node_eval(node.children[1], stack, tab);
			Value to = ast_node_eval(node.children[2], stack, tab);
			assert(from.tag == VALUE_NUM && to.tag == VALUE_NUM);
			Node exp = node.children[3];

			int inc = (from.num <= to.num) ? 1 : -1;
			for (Number i = from.num; i != to.num; i += inc) {
				Value* addr = env_stack_find(stack, tab->arr[id.index]);
				if (!addr) addr = env_stack_get_new(&stack, tab->arr[id.index]);
				*addr = (Value) {VALUE_NUM, .num = i};
				val = ast_node_eval(exp, stack, tab);
			}

			env_stack_pop(&stack);
			break;
		}
		case FALA_WHILE: {
			assert(node.children_count == 2);
			Node cond = node.children[0];
			Node exp = node.children[1];
			while (ast_node_eval(cond, stack, tab).num)
				val = ast_node_eval(exp, stack, tab);
			break;
		}
		case FALA_IN: {
			char* buf = malloc(sizeof(char) * 100);
			fgets(buf, 100, stdin);
			const size_t len = strlen(buf);
			buf[len - 1] = '\0';
			long num = 0;
			if (sscanf(buf, "%ld", &num) == 0)
				val = (Value) {VALUE_STR, .str = buf};
			else {
				free(buf);
				val = (Value) {VALUE_NUM, .num = num};
			}
			break;
		}
		case FALA_OUT: {
			assert(node.children_count == 1);
			val = ast_node_eval(node.children[0], stack, tab);
			print_value(val);
			break;
		}
		case FALA_ASS: {
			assert(node.children_count == 2);
			Node var = node.children[0];
			assert(var.type == FALA_VAR);
			Node id = var.children[0];
			Value value = ast_node_eval(node.children[1], stack, tab);
			if (var.children_count == 2) {
				Value idx = ast_node_eval(var.children[1], stack, tab);
				assert(idx.tag == VALUE_NUM);
				Value* arr = env_stack_find(stack, sym_table_get(tab, id.index));
				assert(arr);
				arr->arr.data[idx.num] = value;
			} else {
				Value* addr = env_stack_find(stack, sym_table_get(tab, id.index));
				assert(!addr);
				*addr = value;
			}
			val = value;
			break;
		}
		case FALA_OR: BIN_OP(||);
		case FALA_AND: BIN_OP(&&);
		case FALA_GREATER: BIN_OP(>);
		case FALA_LESSER: BIN_OP(<);
		case FALA_GREATER_EQ: BIN_OP(>=);
		case FALA_LESSER_EQ: BIN_OP(<=);
		case FALA_EQ_EQ: BIN_OP(==);
		case FALA_ADD: BIN_OP(+);
		case FALA_SUB: BIN_OP(-);
		case FALA_MUL: BIN_OP(*);
		case FALA_DIV: BIN_OP(/);
		case FALA_MOD: BIN_OP(%);
		case FALA_NOT: {
			assert(node.children_count == 1);
			Node op = node.children[0];
			Value v = ast_node_eval(op, stack, tab);
			val = (Value) {VALUE_NUM, .num = !v.num};
			break;
		}
		case FALA_ID: assert(false);
		case FALA_STRING: {
			assert(node.children_count == 0);
			val = (Value) {VALUE_STR, .str = sym_table_get(tab, node.index)};
			break;
		}
		case FALA_DECL: {
			Node var = node.children[0];
			Node id = var.children[0];
			Value* cell = env_stack_get_new(&stack, sym_table_get(tab, id.index));
			if (var.children_count == 2) {
				Value size = ast_node_eval(var.children[1], stack, tab);
				*cell = (Value) {
					VALUE_ARR,
					.arr.data = malloc(sizeof(Value) * size.num),
					.arr.len = size.num};
				memset(cell->arr.data, 0, sizeof(Value) * size.num);
				val = (Value) {VALUE_NUM, .num = 0};
			} else if (node.children_count == 2) {
				Value ass = ast_node_eval(node.children[1], stack, tab);
				val = (*cell = ass);
			} else
				val = (Value) {VALUE_NUM, .num = 0};
			break;
		}
		case FALA_VAR: {
			Node id = node.children[0];
			Value* addr = env_stack_find(stack, sym_table_get(tab, id.index));
			assert(addr && "Variable not previously declared.");

			if (node.children_count == 1) { // id
				val = *addr;
			} else { // id[idx]
				Value idx = ast_node_eval(node.children[1], stack, tab);
				assert(
					addr->tag == VALUE_ARR
					&& "Variable does not correspond to a previously declared array"
				);
				assert(idx.tag == VALUE_NUM && "Index is not a number");
				val = (Value) {VALUE_NUM, .num = addr->arr.data[idx.num].num};
			}
			break;
		}
	}

#undef BIN_OP

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

// return is the exit code of the ran program
static Value ast_eval(Interpreter* inter, SymbolTable* syms, AST ast) {
	Value val = ast_node_eval(ast.root, inter->envs, syms);
	return val;
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

static Value* env_stack_get_new(EnvironmentStack* stack, const char* name) {
	VarTable* last = &stack->envs[stack->len - 1].vars;
	last->names[last->len] = name;
	return &last->values[last->len++];
}

static void env_stack_push(EnvironmentStack* stack) {
	stack->envs[stack->len++] = (Environment) {.vars = var_table_init()};
}

static void env_stack_pop(EnvironmentStack* stack) {
	var_table_deinit(stack->envs[stack->len-- - 1].vars);
}

static Context context_init() { return (Context) {.ast = ast_init()}; }

static void context_deinit(Context ctx) {
	(void)ctx;
	return;
}

static AST context_get_ast(Context ctx) { return ctx.ast; }

static AST parse(FILE* fd, SymbolTable* syms) {
	yyscan_t scanner = NULL;
	yylex_init(&scanner);
	yyset_in(fd, scanner);

	Context ctx = context_init();
	if (yyparse(scanner, &ctx, syms)) {
		exit(1); // FIXME propagate error up
	}

	AST ast = context_get_ast(ctx);
	yylex_destroy(scanner);
	context_deinit(ctx);
	return ast;
}

static void print_value(Value val) {
	if (val.tag == VALUE_NUM)
		printf("%d", val.num);
	else
		printf("%s", val.str);
	printf("\n");
}

static void value_deinit(Value val) {
	if (val.tag == VALUE_STR) {
		printf("%p\n", val.str);
		free(val.str);

	} else if (val.tag == VALUE_ARR)
		free(val.arr.data);
}

static Interpreter interpreter_init(void) {
	Interpreter inter;
	inter.envs = env_stack_init();
	env_stack_push(&inter.envs);
	return inter;
}

static void interpreter_deinit(Interpreter* inter) {
	env_stack_pop(&inter->envs);
	env_stack_deinit(inter->envs);
}

void usage() {
	printf(
		"Usage:\n"
		"\tfala [<options> ...] <filepath>\n"
		"Options:\n"
		"\t-V    verbose output"
	);
}

typedef struct Options {
	bool is_invalid;
	bool verbose;
#ifdef FALA_WITH_REPL
	bool run_repl;
#endif
	char** argv;
	int argc;
} Options;

Options parse_args(int argc, char* argv[]) {
	Options opts;
	opts.verbose = false;
	opts.is_invalid = false;

	// getopt comes from POSIX which is not available on Windows
#ifndef _WIN32
	for (char c = 0; (c = getopt(argc, argv, "V")) != -1;) switch (c) {
			case 'V': opts.verbose = true; break;
			default: break;
		}

	opts.argv = &argv[optind];
	opts.argc = argc - optind;
#else
	opts.argv = &argv[1];
	opts.argc = argc - 1;
#endif

	if (opts.argc < 1) opts.is_invalid = true;

	return opts;
}

#ifdef FALA_WITH_REPL
static int repl(Options opts) {
	FILE* fd = stdin;
	Interpreter inter = interpreter_init();
	SymbolTable syms = sym_table_init();

	while (!feof(fd)) {
		AST ast = parse(fd);
		if (opts.verbose) {
			print_ast(ast);
			printf("\n");
		}

		Value val = ast_eval(ast, syms);
		print_value(val);
		printf("\n");
		ast_deinit(ast);
	}

	sym_table_deinit(&syms);
	interpreter_deinit(&inter);
	fclose(fd);
	return 0;
}
#endif

SymbolTable sym_table_init() {
	SymbolTable syms;
	syms.cap = 100;
	syms.arr = malloc(sizeof(char*) * 100);
	syms.len = 0;
	return syms;
}

void sym_table_deinit(SymbolTable* tab) { free(tab->arr); }

static int interpret(Options opts) {
#ifdef FALA_WITH_REPL
	FILE* fd = fopen(opts.argv[0], "r");
#else
	FILE* fd =
		(strcmp(opts.argv[0], "-") == 0) ? stdin : fopen(opts.argv[0], "r");
#endif

	if (!fd) return 1;

	Interpreter inter = interpreter_init();
	SymbolTable syms = sym_table_init();

	AST ast = parse(fd, &syms);
	if (opts.verbose) {
		print_ast(ast, &syms);
		printf("\n");
	}

	Value val = ast_eval(&inter, &syms, ast);

	ast_deinit(ast);
	sym_table_deinit(&syms);
	interpreter_deinit(&inter);
	fclose(fd);

	if (val.tag == VALUE_NUM)
		return val.num;
	else
		return 0;
}

int main(int argc, char* argv[]) {
	Options opts = parse_args(argc, argv);
	if (opts.is_invalid) {
		usage();
		return 1;
	}

#ifdef FALA_WITH_REPL
	if (opts.run_repl)
		return repl(opts);
	else
#endif
		return interpret(opts);
}
