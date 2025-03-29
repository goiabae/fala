#include "ast.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "str_pool.h"

using std::vector;

bool node_has_fixed_repr(enum NodeType type) {
	switch (type) {
		case NodeType::NUM:
		case NodeType::ID:
		case NodeType::STR:
		case NodeType::EMPTY:
		case NodeType::CHAR:
		case NodeType::PATH:
		case NodeType::PRIMITIVE_TYPE: return false;
		default: return true;
	}
}

// returns the string for nodes with a fixed representation, independent of the
// program's values
const char* node_repr(enum NodeType type) {
	if (!node_has_fixed_repr(type)) assert(false && "unreachable");

	switch (type) {
		case NodeType::APP: return "app";
		case NodeType::BLK: return "block";
		case NodeType::IF: return "if";
		case NodeType::WHEN: return "when";
		case NodeType::FOR: return "for";
		case NodeType::WHILE: return "while";
		case NodeType::BREAK: return "break";
		case NodeType::CONTINUE: return "continue";
		case NodeType::ASS: return "=";
		case NodeType::OR: return "or";
		case NodeType::AND: return "and";
		case NodeType::GTN: return ">";
		case NodeType::LTN: return "<";
		case NodeType::GTE: return ">=";
		case NodeType::LTE: return "<=";
		case NodeType::EQ: return "==";
		case NodeType::ADD: return "+";
		case NodeType::SUB: return "-";
		case NodeType::MUL: return "*";
		case NodeType::DIV: return "/";
		case NodeType::MOD: return "%";
		case NodeType::NOT: return "not";
		case NodeType::DECL: return "decl";
		case NodeType::LET: return "let";
		case NodeType::AT: return "at";
		case NodeType::AS: return "as";
		case NodeType::NIL: return "nil";
		case NodeType::TRUE: return "true";
		case NodeType::FALSE: return "false";
		default: assert(false && "unreachable");
	}
}

static void ast_node_print(
	AST* ast, STR_POOL pool, NodeIndex node_idx, unsigned int space
) {
	const auto& node = ast->at(node_idx);
	if (node.type == NodeType::NUM) {
		printf("%d", node.num);
		return;
	} else if (node.type == NodeType::ID) {
		printf("%s", str_pool_find(pool, node.str_id));
		return;
	} else if (node.type == NodeType::STR) {
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
	} else if (node.type == NodeType::CHAR) {
		printf("'%c'", node.character);
		return;
	} else if (node.type == NodeType::PRIMITIVE_TYPE) {
		switch (ast->at(node[0]).num) {
			case 0: printf("int %d", ast->at(node[1]).num); break;
			case 1: printf("uint %d", ast->at(node[1]).num); break;
			case 2: printf("bool"); break;
			case 3: printf("nil"); break;
		}
		return;
	} else if (node.type == NodeType::EMPTY) {
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

NodeIndex new_node(AST* ast, NodeType type, vector<NodeIndex> children) {
	size_t len = children.size();

	assert(len > 0);

	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = type;
	node.branch.children_count = len;
	node.branch.children = NULL;

	{
		const auto& first = ast->at(children[0]);
		const auto& last = ast->at(children[len - 1]);

		node.loc.begin.column = first.loc.begin.column;
		node.loc.begin.line = first.loc.begin.line;
		node.loc.end.column = last.loc.end.column;
		node.loc.end.line = last.loc.end.line;
	}

	node.branch.children = new NodeIndex[len];
	for (size_t i = 0; i < len; i++) node.branch.children[i] = children[i];

	return idx;
}

NodeIndex new_list_node(AST* ast) {
	constexpr auto max_list_children_count = 100; // FIXME: unhardcode this
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = NodeType::BLK;
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

	node.type = NodeType::NUM;
	node.loc = loc;
	node.num = num;

	return idx;
}

NodeIndex new_nil_node(AST* ast, Location loc) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = NodeType::NIL;
	node.loc = loc;

	return idx;
}

NodeIndex new_true_node(AST* ast, Location loc) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = NodeType::TRUE;
	node.loc = loc;

	return idx;
}

NodeIndex new_false_node(AST* ast, Location loc) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = NodeType::FALSE;
	node.loc = loc;

	return idx;
}

NodeIndex new_char_node(AST* ast, Location loc, char character) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = NodeType::CHAR;
	node.loc = loc;
	node.character = character;

	return idx;
}

NodeIndex new_empty_node(AST* ast) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = NodeType::EMPTY;

	return idx;
}

NodeIndex list_append_node(AST* ast, NodeIndex list_idx, NodeIndex next_idx) {
	auto& list = ast->at(list_idx);
	const auto& next = ast->at(next_idx);

	if (list.branch.children_count == 0) list.loc = next.loc;
	list.loc.end.column = next.loc.end.column;
	list.loc.end.line = next.loc.end.line;
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
	list.loc.begin.column = next.loc.end.column;
	list.loc.begin.line = next.loc.end.line;
	list.branch.children_count++;
	list.branch.children[0] = next_idx;

	return list_idx;
}

// FIXME: maybe put this in a destructor?
void node_deinit(AST* ast, NodeIndex node_idx) {
	auto& node = ast->at(node_idx);

	// if it's a terminal node, we don't free the children and children indexes
	// buffer
	if (node.type == NodeType::ID || node.type == NodeType::NUM
	    || node.type == NodeType::STR || node.type == NodeType::CHAR)
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

bool operator<(NodeIndex a, NodeIndex b) { return a.index < b.index; }
