#include "ast.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

static void node_deinit(Node node);

AST ast_init(void) {
	AST ast;
	ast.root.type = 0;
	ast.root.num = 0;
	return ast;
}

void ast_deinit(AST ast) { node_deinit(ast.root); }

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

Node list_append_node(Node list, Node next) {
	list.children[list.children_count++] = next;
	return list;
}

static void node_deinit(Node node) {
	if (node.type == FALA_ID || node.type == FALA_NUM || node.type == FALA_STRING)
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

void sym_table_deinit(SymbolTable* tab) { free(tab->arr); }

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
