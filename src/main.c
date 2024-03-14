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

int var_table_insert(VarTable* tab, const char* name, int value) {
	for (size_t i = 0; i < tab->len; i++)
		if (strcmp(tab->names[i], name) == 0) return (tab->values[i] = value);

	// if name not in table
	tab->names[tab->len] = name;
	tab->values[tab->len] = value;
	tab->len++;
	return value;
}

int var_table_get(VarTable* tab, const char* name) {
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
		case FALA_THEN: return "then"; break;
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

static void ast_deinit(AST ast) { node_deinit(ast.root); }

static Context context_init() { return (Context) {}; }
static void context_deinit(Context ctx) { return; }
static AST context_get_ast(Context ctx) { return ctx.ast; }

static AST parse(FILE* fd) {
	yyin = fd;
	Context ctx = context_init();
	yyparse(&ctx);
	AST ast = context_get_ast(ctx);
	context_deinit(ctx);
	return ast;
}

int main(void) {
	AST ast = parse(stdin);
	print_ast(ast);
}
