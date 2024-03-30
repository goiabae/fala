#include "ast.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

static void node_deinit(Node node);

// clang-format can't format this properly
// relevant issue https://github.com/llvm/llvm-project/issues/61560
static const char* node_repr[] = {
	[AST_APP] = "app",     [AST_NUM] = NULL,    [AST_BLK] = "block",
	[AST_IF] = "if",       [AST_WHEN] = "when", [AST_FOR] = "for",
	[AST_WHILE] = "while", [AST_ASS] = "=",     [AST_OR] = "or",
	[AST_AND] = "and",     [AST_GTN] = ">",     [AST_LTN] = "<",
	[AST_GTE] = ">=",      [AST_LTE] = "<=",    [AST_EQ] = "==",
	[AST_ADD] = "+",       [AST_SUB] = "-",     [AST_MUL] = "*",
	[AST_DIV] = "/",       [AST_MOD] = "%",     [AST_NOT] = "not",
	[AST_ID] = NULL,       [AST_STR] = NULL,    [AST_DECL] = "decl",
	[AST_VAR] = "var",     [AST_LET] = "let",
};

AST ast_init(void) {
	AST ast;
	ast.root.type = 0;
	ast.root.num = 0;
	return ast;
}

void ast_deinit(AST ast) { node_deinit(ast.root); }

static void ast_node_print(SymbolTable* tab, Node node, unsigned int space) {
	if (node.type == AST_NUM) {
		printf("%d", node.num);
		return;
	} else if (node.type == AST_ID) {
		printf("%s", sym_table_get(tab, node.index));
		return;
	} else if (node.type == AST_STR) {
		printf("\"");
		for (char* it = sym_table_get(tab, node.index); *it != '\0'; it++) {
			if (*it == '\n')
				printf("\\n");
			else
				printf("%c", *it);
		}
		printf("\"");
		return;
	} else if (node.type == AST_NIL) {
		printf("nil");
		return;
	} else if (node.type == AST_TRUE) {
		printf("true");
		return;
	}

	printf("(");

	printf("%s", node_repr[node.type]);

	space += 2;

	for (size_t i = 0; i < node.children_count; i++) {
		printf("\n");
		for (size_t j = 0; j < space; j++) printf(" ");
		ast_node_print(tab, node.children[i], space);
	}

	printf(")");
}

void ast_print(AST ast, SymbolTable* syms) {
	ast_node_print(syms, ast.root, 0);
}

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
	node.type = AST_BLK;
	node.children_count = 0;
	node.children = malloc(sizeof(Node) * 100);
	return node;
}

Node new_string_node(Type type, SymbolTable* tab, String str) {
	Node node;
	node.index = sym_table_insert(tab, str);
	node.type = type;
	return node;
}

Node new_number_node(Number num) {
	Node node;
	node.type = AST_NUM;
	node.num = num;
	return node;
}

Node new_nil_node() { return (Node) {.type = AST_NIL}; }
Node new_true_node() { return (Node) {.type = AST_TRUE}; }

Node list_append_node(Node list, Node next) {
	list.children[list.children_count++] = next;
	return list;
}

static void node_deinit(Node node) {
	if (node.type == AST_ID || node.type == AST_NUM || node.type == AST_STR)
		return;
	for (size_t i = 0; i < node.children_count; i++)
		node_deinit(node.children[i]);
	free(node.children);
}

SymbolTable sym_table_init() {
	SymbolTable syms;
	syms.cap = 100;
	syms.arr = malloc(sizeof(char*) * 100);
	syms.len = 0;
	return syms;
}

void sym_table_deinit(SymbolTable* tab) {
	for (size_t i = 0; i < tab->len; i++) free(tab->arr[i]);
	free(tab->arr);
}

size_t sym_table_insert(SymbolTable* tab, String str) {
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

String sym_table_get(SymbolTable* tab, size_t index) { return tab->arr[index]; }
