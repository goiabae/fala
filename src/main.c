#include "main.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

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

void yyerror(void* var_table, char* s) {
	(void)var_table;
	fprintf(stderr, "%s\n", s);
}

static char* node_repr(Type type, void* data) {
	switch (type) {
		case FALA_BLOCK: return "do-end"; break;
		case FALA_ASS: return "="; break;
		case FALA_OR: return "or"; break;
		case FALA_AND: return "and"; break;
		case FALA_GREATER: return ">"; break;
		case FALA_LESSER: return "<"; break;
		case FALA_GREATER_EQ: return ">="; break;
		case FALA_LESSER_EQ: return "<="; break;
		case FALA_EQ_EQ: return "=="; break;
		case FALA_ADD: return "+"; break;
		case FALA_SUB: return "-"; break;
		case FALA_MUL: return "*"; break;
		case FALA_DIV: return "/"; break;
		case FALA_MOD: return "%"; break;
		case FALA_NOT: return "!"; break;
		default: break;
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

static Value ast_node_eval_bin_op(
	Node node, VarTable* vars, Value (*op)(Value, Value)
) {
	assert(node.children_count == 2);
	Node left = node.children[0];
	Node right = node.children[1];
	Value val = op(ast_node_eval(left, vars), ast_node_eval(right, vars));
	return val;
}

static Value op_add(Value left, Value right) { return left + right; }
static Value op_sub(Value left, Value right) { return left - right; }
static Value op_mul(Value left, Value right) { return left * right; }
static Value op_div(Value left, Value right) { return left / right; }
static Value op_mod(Value left, Value right) { return left % right; }
static Value op_gt(Value left, Value right) { return left > right; }
static Value op_lt(Value left, Value right) { return left < right; }
static Value op_gte(Value left, Value right) { return left >= right; }
static Value op_lte(Value left, Value right) { return left <= right; }
static Value op_eql(Value left, Value right) { return left == right; }
static Value op_or(Value left, Value right) { return left || right; }
static Value op_and(Value left, Value right) { return left && right; }

static Value ast_node_eval(Node node, VarTable* vars) {
	Value val;

#define BIN_OP(OP)                            \
	val = ast_node_eval_bin_op(node, vars, OP); \
	break

	switch (node.type) {
		case FALA_NUM: return *(int*)node.data; break;
		case FALA_BLOCK: {
			for (size_t i = 0; i < node.children_count; i++)
				val = ast_node_eval(node.children[i], vars);
			break;
		}
		case FALA_ASS: {
			assert(node.children_count == 2);
			Node id = node.children[0];
			assert(id.type == FALA_ID);
			Node expr = node.children[1];
			val = var_table_insert(vars, (char*)id.data, ast_node_eval(expr, vars));
			break;
		}
		case FALA_OR: BIN_OP(op_or);
		case FALA_AND: BIN_OP(op_and);
		case FALA_GREATER: BIN_OP(op_gt);
		case FALA_LESSER: BIN_OP(op_lt);
		case FALA_GREATER_EQ: BIN_OP(op_gte);
		case FALA_LESSER_EQ: BIN_OP(op_lte);
		case FALA_EQ_EQ: BIN_OP(op_eql);
		case FALA_ADD: BIN_OP(op_add);
		case FALA_SUB: BIN_OP(op_sub);
		case FALA_MUL: BIN_OP(op_mul);
		case FALA_DIV: BIN_OP(op_div);
		case FALA_MOD: BIN_OP(op_mod);
		case FALA_NOT: {
			assert(node.children_count == 1);
			Node op = node.children[0];
			val = !ast_node_eval(op, vars);
			break;
		}
		case FALA_ID: {
			assert(node.children_count == 0);
			val = var_table_get(vars, (char*)node.data);
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
static void context_deinit(Context ctx) { return; }

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

static void print_value(Value val) { printf("%d", val); }

int main(void) {
	AST ast = parse(stdin);
	print_ast(ast);
	printf("\n");

	Value val = ast_eval(ast);
	print_value(val);
	printf("\n");

	ast_deinit(ast);
}
