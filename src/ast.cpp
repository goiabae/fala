#include "ast.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str_pool.h"

static void node_deinit(Node node);

const char* node_repr(enum NodeType type) {
	switch (type) {
		case AST_APP: return "app";
		case AST_BLK: return "block";
		case AST_IF: return "if";
		case AST_WHEN: return "when";
		case AST_FOR: return "for";
		case AST_WHILE: return "while";
		case AST_BREAK: return "break";
		case AST_CONTINUE: return "continue";
		case AST_ASS: return "=";
		case AST_OR: return "or";
		case AST_AND: return "and";
		case AST_GTN: return ">";
		case AST_LTN: return "<";
		case AST_GTE: return ">=";
		case AST_LTE: return "<=";
		case AST_EQ: return "==";
		case AST_ADD: return "+";
		case AST_SUB: return "-";
		case AST_MUL: return "*";
		case AST_DIV: return "/";
		case AST_MOD: return "%";
		case AST_NOT: return "not";
		case AST_DECL: return "decl";
		case AST_LET: return "let";
		case AST_AT: return "at";
		case AST_AS: return "as";
		case AST_NUM:
		case AST_ID:
		case AST_STR:
		case AST_EMPTY:
		case AST_NIL:
		case AST_TRUE:
		case AST_CHAR:
		case AST_PATH:
		case AST_PRIMITIVE_TYPE: assert(false && "unreachable");
	}
	assert(false && "unreachable");
}

AST ast_init(void) {
	AST ast;
	ast.root.type = (enum NodeType)0;
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
	} else if (node.type == AST_CHAR) {
		printf("'%c'", node.character);
		return;
	} else if (node.type == AST_PRIMITIVE_TYPE) {
		switch (node.branch.children[0].num) {
			case 0: printf("int %d", node.branch.children[1].num); break;
			case 1: printf("uint %d", node.branch.children[1].num); break;
			case 2: printf("bool"); break;
			case 3: printf("nil"); break;
		}
		return;
	} else if (node.type == AST_EMPTY) {
		return;
	}

	printf("(");

	printf("%s", node_repr(node.type));

	space += 2;

	for (size_t i = 0; i < node.branch.children_count; i++) {
		printf("\n");
		for (size_t j = 0; j < space; j++) printf(" ");
		ast_node_print(pool, node.branch.children[i], space);
	}

	printf(")");
}

void ast_print(AST ast, STR_POOL pool) { ast_node_print(pool, ast.root, 0); }

Node new_node(NodeType type, size_t len, Node* children) {
	Node node;
	node.type = type;
	node.branch.children_count = len;
	node.branch.children = NULL;
	node.loc.first_column = children[0].loc.first_column;
	node.loc.first_line = children[0].loc.first_line;
	node.loc.last_column = children[len - 1].loc.last_column;
	node.loc.last_line = children[len - 1].loc.last_line;
	assert(len > 0);
	node.branch.children = (Node*)malloc(sizeof(Node) * len);
	memcpy(node.branch.children, children, sizeof(Node) * len);
	return node;
}

Node new_list_node(void) {
	Node node;
	node.type = AST_BLK;
	node.branch.children_count = 0;
	node.branch.children = (Node*)malloc(sizeof(Node) * 100);
	return node;
}

Node new_string_node(NodeType type, Location loc, STR_POOL pool, String str) {
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

Node new_nil_node(Location loc) { return Node {AST_NIL, loc, {}}; }

Node new_true_node(Location loc) { return Node {AST_TRUE, loc, {}}; }

Node new_char_node(Location loc, char character) {
	return Node {AST_CHAR, loc, {character}};
}

Node new_empty_node(void) { return Node {AST_EMPTY, {}, {}}; }

Node list_append_node(Node list, Node next) {
	if (list.branch.children_count == 0) list.loc = next.loc;
	list.loc.last_column = next.loc.last_column;
	list.loc.last_line = next.loc.last_line;
	list.branch.children[list.branch.children_count++] = next;
	return list;
}

Node list_prepend_node(Node list, Node next) {
	if (list.branch.children_count != 0)
		for (size_t i = list.branch.children_count; i > 0; i--)
			list.branch.children[i] = list.branch.children[i - 1];
	else
		list.loc = next.loc;
	// FIXME might be out of order
	list.loc.first_column = next.loc.last_column;
	list.loc.first_line = next.loc.last_line;
	list.branch.children_count++;
	list.branch.children[0] = next;
	return list;
}

static void node_deinit(Node node) {
	if (node.type == AST_ID || node.type == AST_NUM || node.type == AST_STR || node.type == AST_CHAR)
		return;
	for (size_t i = 0; i < node.branch.children_count; i++)
		node_deinit(node.branch.children[i]);
	free(node.branch.children);
}
