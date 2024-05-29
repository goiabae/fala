#include "ast.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str_pool.h"

static void node_deinit(Node node);

// clang-format can't format this properly
// relevant issue https://github.com/llvm/llvm-project/issues/61560
static const char* node_repr[] = {
	[AST_APP] = "app",     [AST_NUM] = NULL,      [AST_BLK] = "block",
	[AST_IF] = "if",       [AST_WHEN] = "when",   [AST_FOR] = "for",
	[AST_WHILE] = "while", [AST_BREAK] = "break", [AST_CONTINUE] = "continue",
	[AST_ASS] = "=",       [AST_OR] = "or",       [AST_AND] = "and",
	[AST_GTN] = ">",       [AST_LTN] = "<",       [AST_GTE] = ">=",
	[AST_LTE] = "<=",      [AST_EQ] = "==",       [AST_ADD] = "+",
	[AST_SUB] = "-",       [AST_MUL] = "*",       [AST_DIV] = "/",
	[AST_MOD] = "%",       [AST_NOT] = "not",     [AST_ID] = NULL,
	[AST_STR] = NULL,      [AST_DECL] = "decl",   [AST_VAR] = "var",
	[AST_LET] = "let",
};

AST ast_init(void) {
	AST ast;
	ast.root.type = 0;
	ast.root.num = 0;
	return ast;
}

void ast_deinit(AST ast) { node_deinit(ast.root); }

static void ast_node_print(STR_POOL pool, Node node, unsigned int space) {
	if (node.type == AST_NUM) {
		printf("%d", node.num);
		return;
	} else if (node.type == AST_ID) {
		printf("%s", str_pool_find(pool, node.str_id));
		return;
	} else if (node.type == AST_STR) {
		printf("\"");
		for (char* it = (char*)str_pool_find(pool, node.str_id); *it != '\0';
		     it++) {
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
		ast_node_print(pool, node.children[i], space);
	}

	printf(")");
}

void ast_print(AST ast, STR_POOL pool) { ast_node_print(pool, ast.root, 0); }

Node new_node(Type type, size_t len, Node* children) {
	Node node;
	node.type = type;
	node.children_count = len;
	node.children = NULL;
	node.loc.first_column = children[0].loc.first_column;
	node.loc.first_line = children[0].loc.first_line;
	node.loc.last_column = children[len - 1].loc.last_column;
	node.loc.last_line = children[len - 1].loc.last_line;
	assert(len > 0);
	node.children = malloc(sizeof(Node) * len);
	memcpy(node.children, children, sizeof(Node) * len);
	return node;
}

Node new_list_node(void) {
	Node node;
	node.type = AST_BLK;
	node.children_count = 0;
	node.children = malloc(sizeof(Node) * 100);
	return node;
}

Node new_string_node(Type type, Location loc, STR_POOL pool, String str) {
	Node node;
	node.loc = loc;
	node.str_id = str_pool_intern(pool, str);
	node.type = type;
	return node;
}

Node new_number_node(Location loc, Number num) {
	Node node;
	node.type = AST_NUM;
	node.loc = loc;
	node.num = num;
	return node;
}

Node new_nil_node(Location loc) { return (Node) {.type = AST_NIL, .loc = loc}; }
Node new_true_node(Location loc) {
	return (Node) {.type = AST_TRUE, .loc = loc};
}

Node list_append_node(Node list, Node next) {
	if (list.children_count == 0) list.loc = next.loc;
	list.loc.last_column = next.loc.last_column;
	list.loc.last_line = next.loc.last_line;
	list.children[list.children_count++] = next;
	return list;
}

Node list_prepend_node(Node list, Node next) {
	if (list.children_count != 0)
		for (size_t i = list.children_count; i > 0; i--)
			list.children[i] = list.children[i - 1];
	else
		list.loc = next.loc;
	// FIXME might be out of order
	list.loc.first_column = next.loc.last_column;
	list.loc.first_line = next.loc.last_line;
	list.children_count++;
	list.children[0] = next;
	return list;
}

static void node_deinit(Node node) {
	if (node.type == AST_ID || node.type == AST_NUM || node.type == AST_STR)
		return;
	for (size_t i = 0; i < node.children_count; i++)
		node_deinit(node.children[i]);
	free(node.children);
}
