#include "ast.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ranges>
#include <stdexcept>
#include <vector>

#include "str_pool.h"

using std::vector;

bool AST::is_empty() { return root_index.index == -1; }

bool is_branch_node(NodeType type) {
	switch (type) {
		case NodeType::EMPTY: return false;
		case NodeType::APP: return true;
		case NodeType::NUM: return false;
		case NodeType::BLK: return true;
		case NodeType::IF: return true;
		case NodeType::WHEN: return true;
		case NodeType::FOR: return true;
		case NodeType::WHILE: return true;
		case NodeType::BREAK: return true;
		case NodeType::CONTINUE: return true;
		case NodeType::ASS: return true;
		case NodeType::OR: return true;
		case NodeType::AND: return true;
		case NodeType::GTN: return true;
		case NodeType::LTN: return true;
		case NodeType::GTE: return true;
		case NodeType::LTE: return true;
		case NodeType::EQ: return true;
		case NodeType::AT: return true;
		case NodeType::ADD: return true;
		case NodeType::SUB: return true;
		case NodeType::MUL: return true;
		case NodeType::DIV: return true;
		case NodeType::MOD: return true;
		case NodeType::NOT: return true;
		case NodeType::ID: return false;
		case NodeType::STR: return false;
		case NodeType::VAR_DECL: return true;
		case NodeType::FUN_DECL: return true;
		case NodeType::NIL: return false;
		case NodeType::TRUE: return false;
		case NodeType::FALSE: return false;
		case NodeType::LET: return true;
		case NodeType::CHAR: return false;
		case NodeType::PATH: return true;
		case NodeType::AS: return true;
		case NodeType::INSTANCE: return true;
	}
	assert(false);
}

const char* node_type_repr(enum NodeType type) {
	switch (type) {
		case NodeType::EMPTY: return "NodeType::EMPTY";
		case NodeType::APP: return "NodeType::APP";
		case NodeType::NUM: return "NodeType::NUM";
		case NodeType::BLK: return "NodeType::BLK";
		case NodeType::IF: return "NodeType::IF";
		case NodeType::WHEN: return "NodeType::WHEN";
		case NodeType::FOR: return "NodeType::FOR";
		case NodeType::WHILE: return "NodeType::WHILE";
		case NodeType::BREAK: return "NodeType::BREAK";
		case NodeType::CONTINUE: return "NodeType::CONTINUE";
		case NodeType::ASS: return "NodeType::ASS";
		case NodeType::OR: return "NodeType::OR";
		case NodeType::AND: return "NodeType::AND";
		case NodeType::GTN: return "NodeType::GTN";
		case NodeType::LTN: return "NodeType::LTN";
		case NodeType::GTE: return "NodeType::GTE";
		case NodeType::LTE: return "NodeType::LTE";
		case NodeType::EQ: return "NodeType::EQ";
		case NodeType::AT: return "NodeType::AT";
		case NodeType::ADD: return "NodeType::ADD";
		case NodeType::SUB: return "NodeType::SUB";
		case NodeType::MUL: return "NodeType::MUL";
		case NodeType::DIV: return "NodeType::DIV";
		case NodeType::MOD: return "NodeType::MOD";
		case NodeType::NOT: return "NodeType::NOT";
		case NodeType::ID: return "NodeType::ID";
		case NodeType::STR: return "NodeType::STR";
		case NodeType::VAR_DECL: return "NodeType::VAR_DECL";
		case NodeType::FUN_DECL: return "NodeType::FUN_DECL";
		case NodeType::NIL: return "NodeType::NIL";
		case NodeType::TRUE: return "NodeType::TRUE";
		case NodeType::FALSE: return "NodeType::FALSE";
		case NodeType::LET: return "NodeType::LET";
		case NodeType::CHAR: return "NodeType::CHAR";
		case NodeType::PATH: return "NodeType::PATH";
		case NodeType::INSTANCE: return "NodeType::INSTANCE";
		case NodeType::AS: return "NodeType::AS";
	}
	assert(false);
}

bool node_has_fixed_repr(enum NodeType type) {
	switch (type) {
		case NodeType::NUM:
		case NodeType::ID:
		case NodeType::STR:
		case NodeType::EMPTY:
		case NodeType::CHAR:
		case NodeType::PATH:
		case NodeType::INSTANCE: return false;
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
		case NodeType::VAR_DECL: return "fun_decl";
		case NodeType::FUN_DECL: return "var_decl";
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
	AST* ast, const StringPool& pool, NodeIndex node_idx, unsigned int space
) {
	const auto& node = ast->at(node_idx);
	if (node.type == NodeType::NUM) {
		printf("%d", node.num);
		return;
	} else if (node.type == NodeType::ID) {
		printf("%s", pool.find(node.str_id));
		return;
	} else if (node.type == NodeType::STR) {
		printf("\"");
		for (char* it = (char*)pool.find(node.str_id); *it != '\0'; it++) {
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
	} else if (node.type == NodeType::PATH) {
		ast_node_print(ast, pool, node[0], space);
		return;
	} else if (node.type == NodeType::INSTANCE) {
		ast_node_print(ast, pool, node[0], space);
		printf("<");
		auto arguments_idx = node[1];
		const auto& arguments_node = ast->at(arguments_idx);
		for (auto arg_idx : arguments_node) {
			ast_node_print(ast, pool, arg_idx, space);
			printf(", ");
		}
		printf(">");
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

static void print_spaces(FILE* fd, unsigned int count) {
	for (size_t i = 0; i < count; i++) fprintf(fd, " ");
}

static void ast_node_print_detailed(
	AST* ast, const StringPool& pool, NodeIndex node_idx, unsigned int space
) {
	const auto fd = stderr;
	const auto& node = ast->at(node_idx);

	print_spaces(fd, space);
	fprintf(fd, "{\n");
	print_spaces(fd, space + 2);
	fprintf(fd, "type = %s\n", node_type_repr(node.type));
	print_spaces(fd, space + 2);
	fprintf(fd, "index = %d\n", node_idx.index);
	print_spaces(fd, space + 2);
	fprintf(fd, "loc = %d\n", node.loc.begin.byte_offset);

	if (node.type == NodeType::NUM) {
		print_spaces(fd, space + 2);
		fprintf(fd, "num = %d\n", node.num);
	} else if (node.type == NodeType::ID) {
		print_spaces(fd, space + 2);
		fprintf(fd, "id = %s\n", pool.find(node.str_id));
	} else if (node.type == NodeType::STR) {
		print_spaces(fd, space + 2);
		fprintf(fd, "str = \"");
		for (char* it = (char*)pool.find(node.str_id); *it != '\0'; it++) {
			if (*it == '\n')
				fprintf(fd, "\\n");
			else
				fprintf(fd, "%c", *it);
		}
		fprintf(fd, "\"\n");
	} else if (node.type == NodeType::CHAR) {
		print_spaces(fd, space + 2);
		fprintf(fd, "char = '%c'\n", node.character);
	} else if (node.type == NodeType::EMPTY) {
	} else {
		print_spaces(fd, space + 2);
		fprintf(fd, "children = %zu [\n", node.branch.children_count);

		for (auto idx : node) {
			ast_node_print_detailed(ast, pool, idx, space + 2 + 2);
		}

		print_spaces(fd, space + 2);
		fprintf(fd, "]\n");
	}

	print_spaces(fd, space);
	fprintf(fd, "}\n");
}

void ast_print(AST* ast, const StringPool& pool) {
	ast_node_print(ast, pool, ast->root_index, 0);
}

void ast_print_detailed(AST* ast, const StringPool& pool) {
	ast_node_print_detailed(ast, pool, ast->root_index, 0);
}

NodeIndex new_node(AST* ast, NodeType type, vector<NodeIndex> children) {
	size_t len = children.size();

	assert(len > 0);

	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.type = type;
	node.branch.children_count = len;
	node.branch.children = NULL;

	for (auto child_idx : children) {
		ast->at(child_idx).parent_idx = idx;
	}

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
	AST* ast, NodeType type, Location loc, StringPool& pool, String str
) {
	auto idx = ast->alloc_node();
	auto& node = ast->at(idx);

	node.loc = loc;
	node.str_id = pool.intern(str);
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
	assert(node_idx.index >= 0 && node_idx.index < (int)nodes.max_size());
	return nodes[(size_t)node_idx.index];
}

const Node& AST::at(NodeIndex node_idx) const {
	assert(node_idx.index >= 0 && node_idx.index < (int)nodes.max_size());
	return nodes[(size_t)node_idx.index];
}

NodeIndex AST::alloc_node() {
	auto idx = next_free_index.index++;
	auto& node = nodes[(size_t)idx];
	node.parent_idx = NodeIndex {INVALID_NODE_INDEX};
	return {idx};
}

NodeIndex& Node::operator[](size_t index) const {
	return branch.children[index];
}

void ast_set_root(AST* ast, NodeIndex node_idx) { ast->root_index = node_idx; }

AST::~AST() {
	if (not is_empty()) node_deinit(this, root_index);
}

bool operator<(NodeIndex a, NodeIndex b) { return a.index < b.index; }

size_t Node::size() const {
	// FIXME: assert this is not a terminal node
	return branch.children_count;
}

struct NodeRef {
	NodeIndex index;
	const AST& ast;
	const Node& operator()() const { return ast.at(index); }
};

// NOTE: This assumes both ASTs use the same StringPool

bool operator==(const NodeRef& a, const NodeRef& b) {
	if (a().type != b().type) return false;

	auto type = a().type;

	if (type == NodeType::NUM) return a().num == b().num;

	if (type == NodeType::ID or type == NodeType::STR)
		return a().str_id == b().str_id;

	if (type == NodeType::CHAR) return a().character == b().character;

	if (type == NodeType::TRUE or type == NodeType::FALSE or type == NodeType::NIL
	    or type == NodeType::EMPTY)
		return true;

	if (is_branch_node(a().type)) {
		if (a().size() != b().size()) return false;
		for (const auto& [ai, bi] : std::ranges::views::zip(a(), b()))
			if (NodeRef {ai, a.ast} != NodeRef {bi, b.ast}) return false;
	} else {
		throw std::logic_error("");
	}

	return true;
}

bool operator==(const AST& a, const AST& b) {
	NodeRef ra {a.root_index, a};
	NodeRef rb {b.root_index, b};
	return ra == rb;
}
