#include "main.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

static void print_value(Value val);
static void value_deinit(Value val);

Node new_node(Type type, void* data, size_t len, Node children[len]) {
	Node node;
	node.type = type;
	node.data = data;
	node.children_count = len;
	node.children = NULL;
	if (len > 0) {
		node.children = malloc(sizeof(Node) * len);
		memcpy(node.children, children, sizeof(Node) * len);
	}
	return node;
}

Node new_block_node() {
	Node node;
	node.type = FALA_BLOCK;
	node.data = NULL;
	node.children_count = 0;
	node.children = malloc(sizeof(Node) * 100);
	return node;
}

Node block_append_node(Node block, Node next) {
	block.children[block.children_count++] = next;
	return block;
}

VarTable* var_table_init(void) {
	VarTable* tab = malloc(sizeof(VarTable));
	tab->len = 0;
	tab->cap = 100;

	tab->names = malloc(sizeof(char*) * tab->cap);
	memset(tab->names, 0, sizeof(char*) * tab->cap);

	tab->values = malloc(sizeof(int) * tab->cap);
	memset(tab->values, 0, sizeof(int) * tab->cap);

	return tab;
}

void var_table_deinit(VarTable* tab) {
	free(tab->names);
	free(tab->values);
	free(tab);
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
	tab->names[tab->len] = name;
	return &tab->values[tab->len++];
}

void yyerror(void* var_table, char* s) {
	(void)var_table;
	fprintf(stderr, "%s\n", s);
}

static char* node_repr(Type type, void* data) {
	(void)data;
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

static void print_node(Node node, unsigned int space) {
	if (node.type == FALA_NUM) {
		printf("%d", *(int*)node.data);
		return;
	} else if (node.type == FALA_ID) {
		printf("%s", (char*)node.data);
		return;
	} else if (node.type == FALA_STRING) {
		printf("\"");
		for (char* it = node.data; *it != '\0'; it++) {
			if (*it == '\n')
				printf("\\n");
			else
				printf("%c", *it);
		}
		printf("\"");
		return;
	}

	printf("(");

	printf("%s", node_repr(node.type, node.data));

	space += 2;

	for (size_t i = 0; i < node.children_count; i++) {
		printf("\n");
		for (size_t j = 0; j < space; j++) printf(" ");
		print_node(node.children[i], space);
	}

	printf(")");
}

static void print_ast(AST ast) { print_node(ast.root, 0); }

static void node_deinit(Node node) {
	for (size_t i = 0; i < node.children_count; i++)
		node_deinit(node.children[i]);
	if (node.type == FALA_ID || node.type == FALA_NUM) free(node.data);
	free(node.children);
}

static AST ast_init(void) { return (AST) {}; }
static void ast_deinit(AST ast) { node_deinit(ast.root); }

static Value ast_node_eval(Node node, VarTable* vars);

static Value ast_node_eval(Node node, VarTable* vars) {
	Value val;

#define BIN_OP(OP)                                           \
	{                                                          \
		assert(node.children_count == 2);                        \
		Value left = ast_node_eval(node.children[0], vars);      \
		Value right = ast_node_eval(node.children[1], vars);     \
		assert(left.tag == VALUE_NUM && right.tag == VALUE_NUM); \
		val = (Value) {VALUE_NUM, .num = left.num OP right.num}; \
		break;                                                   \
	}

	switch (node.type) {
		case FALA_NUM: return (Value) {VALUE_NUM, .num = *(int*)node.data}; break;
		case FALA_BLOCK: {
			for (size_t i = 0; i < (node.children_count - 1); i++)
				(void)ast_node_eval(node.children[i], vars);
			val = ast_node_eval(node.children[node.children_count - 1], vars);
			break;
		}
		case FALA_IF: {
			assert(node.children_count == 3);
			Value cond = ast_node_eval(node.children[0], vars);
			assert(cond.tag == VALUE_NUM);
			if (cond.num)
				val = ast_node_eval(node.children[1], vars);
			else
				val = ast_node_eval(node.children[2], vars);
			break;
		}
		case FALA_WHEN: {
			assert(node.children_count == 2);
			Value cond = ast_node_eval(node.children[0], vars);
			assert(cond.tag == VALUE_NUM);
			if (cond.num)
				val = ast_node_eval(node.children[1], vars);
			else
				val = (Value) {VALUE_NUM, .num = 0};
			break;
		}
		case FALA_FOR: {
			assert(node.children_count == 4);
			Node var = node.children[0];
			assert(var.children_count == 1);
			Node id = var.children[0];
			Value from = ast_node_eval(node.children[1], vars);
			Value to = ast_node_eval(node.children[2], vars);
			assert(from.tag == VALUE_NUM && to.tag == VALUE_NUM);
			Node exp = node.children[3];
			if (from.num <= to.num) {
				for (size_t i = from.num; i <= (size_t)(to.num - 1); i++) {
					var_table_insert(vars, (char*)id.data, (Value) {VALUE_NUM, .num = i});
					ast_node_eval(exp, vars);
				}
			} else {
				for (size_t i = from.num; i >= (size_t)(to.num + 1); i--) {
					var_table_insert(vars, (char*)id.data, (Value) {VALUE_NUM, .num = i});
					ast_node_eval(exp, vars);
				}
			}
			var_table_insert(vars, (char*)id.data, to);
			val = ast_node_eval(exp, vars);
			break;
		}
		case FALA_WHILE: {
			assert(node.children_count == 2);
			Node cond = node.children[0];
			Node exp = node.children[1];
			while (ast_node_eval(cond, vars).num) val = ast_node_eval(exp, vars);
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
			else
				val = (Value) {VALUE_NUM, .num = num};
			break;
		}
		case FALA_OUT: {
			assert(node.children_count == 1);
			val = ast_node_eval(node.children[0], vars);
			print_value(val);
			break;
		}
		case FALA_ASS: {
			assert(node.children_count == 2);
			Node var = node.children[0];
			assert(var.type == FALA_VAR);
			Node id = var.children[0];
			Value value = ast_node_eval(node.children[1], vars);
			if (var.children_count == 2) {
				Value idx = ast_node_eval(var.children[1], vars);
				assert(idx.tag == VALUE_NUM);
				Value arr = var_table_get(vars, (char*)id.data);
				arr.arr.data[idx.num] = value;
			} else {
				var_table_insert(vars, (char*)id.data, value);
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
			Value v = ast_node_eval(op, vars);
			val = (Value) {VALUE_NUM, .num = !v.num};
			break;
		}
		case FALA_ID: assert(false);
		case FALA_STRING: {
			assert(node.children_count == 0);
			val = (Value) {VALUE_STR, .str = (char*)node.data};
			break;
		}
		case FALA_DECL: {
			Node var = node.children[0];
			Node id = var.children[0];
			Value* cell = var_table_get_where(vars, (char*)id.data);
			if (var.children_count == 2) {
				Value size = ast_node_eval(var.children[1], vars);
				*cell = (Value) {
					VALUE_ARR,
					.arr.data = malloc(sizeof(Value) * size.num),
					.arr.len = size.num};
				memset(cell->arr.data, 0, sizeof(Value) * size.num);
				val = (Value) {VALUE_NUM, .num = 0};
			} else if (node.children_count == 2) {
				Value ass = ast_node_eval(node.children[1], vars);
				val = (*cell = ass);
			} else
				val = (Value) {VALUE_NUM, .num = 0};
			break;
		}
		case FALA_VAR: {
			Node id = node.children[0];
			if (node.children_count == 1) {
				val = var_table_get(vars, (char*)id.data);
			} else {
				Value arr = var_table_get(vars, (char*)id.data);
				Value idx = ast_node_eval(node.children[1], vars);
				assert(arr.tag == VALUE_ARR && idx.tag == VALUE_NUM);
				val = (Value) {VALUE_NUM, .num = arr.arr.data[idx.num].num};
			}
			break;
		}
	}

#undef BIN_OP

	return val;
}

// return is the exit code of the ran program
static Value ast_eval(AST ast) {
	VarTable* vars = var_table_init();
	Value val = ast_node_eval(ast.root, vars);
	var_table_deinit(vars);
	return val;
}

static Context context_init() { return (Context) {.ast = ast_init()}; }

static void context_deinit(Context ctx) {
	(void)ctx;
	return;
}

static AST context_get_ast(Context ctx) { return ctx.ast; }

static AST parse(FILE* fd) {
	yyin = fd;
	Context ctx = context_init();
	if (yyparse(&ctx)) {
		exit(1); // FIXME propagate error up
	}
	AST ast = context_get_ast(ctx);
	context_deinit(ctx);
	return ast;
}

static void print_value(Value val) {
	if (val.tag == VALUE_NUM)
		printf("%d", val.num);
	else
		printf("%s", val.str);
}

static void value_deinit(Value val) {
	if (val.tag == VALUE_STR) free(val.str);
}

void usage() {
	printf(
		"Usage:\n"
		"\tfala <filepath>\n"
	);
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		usage();
		return 1;
	}

	FILE* fd = (argv[1][0] == '-') ? stdin : fopen(argv[1], "r");
	AST ast = parse(fd);
	print_ast(ast);
	printf("\n");

	Value val = ast_eval(ast);

	ast_deinit(ast);

	if (val.tag == VALUE_NUM)
		return val.num;
	else
		return 0;
}
