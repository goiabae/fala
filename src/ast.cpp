#include "ast.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str_pool.h"

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

static void ast_node_print(
	AST* ast, STR_POOL pool, NodeIndex node_idx, unsigned int space
) {
	const auto& node = ast->at(node_idx);
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
		switch (ast->at(node[0]).num) {
			case 0: printf("int %d", ast->at(node[1]).num); break;
			case 1: printf("uint %d", ast->at(node[1]).num); break;
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
		ast_node_print(ast, pool, node[i], space);
	}

	printf(")");
}

void ast_print(AST* ast, STR_POOL pool) {
	ast_node_print(ast, pool, ast->root_index, 0);
}

NodeIndex new_node(AST* ast, NodeType type, size_t len, NodeIndex* children) {
	assert(len > 0);

	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = type;
	node.branch.children_count = len;
	node.branch.children = NULL;

	{
		const auto& first = ast->at(children[0]);
		const auto& last = ast->at(children[len - 1]);

		node.loc.first_column = first.loc.first_column;
		node.loc.first_line = first.loc.first_line;
		node.loc.last_column = last.loc.last_column;
		node.loc.last_line = last.loc.last_line;
	}

	node.branch.children = new NodeIndex[len];
	for (size_t i = 0; i < len; i++) node.branch.children[i] = children[i];

	return idx;
}

NodeIndex new_list_node(AST* ast) {
	constexpr auto max_list_children_count = 100; // FIXME: unhardcode this
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = AST_BLK;
	node.branch.children_count = 0;
	node.branch.children = new NodeIndex[max_list_children_count];

	return idx;
}

NodeIndex new_string_node(
	AST* ast, NodeType type, Location loc, STR_POOL pool, String str
) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.loc = loc;
	node.str_id = str_pool_intern(pool, str);
	node.type = type;

	return idx;
}

NodeIndex new_number_node(AST* ast, Location loc, Number num) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = AST_NUM;
	node.loc = loc;
	node.num = num;

	return idx;
}

NodeIndex new_nil_node(AST* ast, Location loc) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = AST_NIL;
	node.loc = loc;

	return idx;
}

NodeIndex new_true_node(AST* ast, Location loc) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = AST_TRUE;
	node.loc = loc;

	return idx;
}

NodeIndex new_char_node(AST* ast, Location loc, char character) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = AST_CHAR;
	node.loc = loc;
	node.character = character;

	return idx;
}

NodeIndex new_empty_node(AST* ast) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = AST_EMPTY;

	return idx;
}

NodeIndex list_append_node(AST* ast, NodeIndex list_idx, NodeIndex next_idx) {
	auto& list = ast->at(list_idx);
	const auto& next = ast->at(next_idx);

	if (list.branch.children_count == 0) list.loc = next.loc;
	list.loc.last_column = next.loc.last_column;
	list.loc.last_line = next.loc.last_line;
	list.branch.children[list.branch.children_count++] = next_idx;

	return list_idx;
}

NodeIndex list_prepend_node(AST* ast, NodeIndex list_idx, NodeIndex next_idx) {
	auto& list = ast->at(list_idx);
	const auto& next = ast->at(next_idx);

	if (list.branch.children_count != 0)
		for (size_t i = list.branch.children_count; i > 0; i--)
			list.branch.children[i] = list.branch.children[i - 1];
	else
		list.loc = next.loc;
	// FIXME might be out of order
	list.loc.first_column = next.loc.last_column;
	list.loc.first_line = next.loc.last_line;
	list.branch.children_count++;
	list.branch.children[0] = next_idx;

	return list_idx;
}

// FIXME: maybe put this in a destructor?
void node_deinit(AST* ast, NodeIndex node_idx) {
	auto& node = ast->at(node_idx);

	// if it's a terminal node, we don't free the children and children indexes
	// buffer
	if (node.type == AST_ID || node.type == AST_NUM || node.type == AST_STR || node.type == AST_CHAR)
		return;

	for (size_t i = 0; i < node.branch.children_count; i++)
		node_deinit(ast, node[i]);
	free(node.branch.children);
}

Node& AST::at(NodeIndex node_idx) {
	assert(node_idx.index >= 0 && node_idx.index < 2048);
	return nodes[(size_t)node_idx.index];
}

NodeIndex AST::alloc_node() { return {next_free_index.index++}; }

NodeIndex& Node::operator[](size_t index) const {
	return branch.children[index];
}

void ast_set_root(AST* ast, NodeIndex node_idx) { ast->root_index = node_idx; }

AST::~AST() { node_deinit(this, root_index); }
