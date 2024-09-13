#ifndef FALA_AST_H
#define FALA_AST_H

#include <stddef.h>

#include <array>
#include <vector>

#include "str_pool.h"

typedef int Number;
typedef char* String;

typedef enum NodeType {
	AST_EMPTY, // node for saying there is no node
	AST_APP,   // function application
	AST_NUM,
	AST_BLK, // block
	AST_IF,
	AST_WHEN,
	AST_FOR,
	AST_WHILE,
	AST_BREAK,
	AST_CONTINUE,
	AST_ASS,
	AST_OR,
	AST_AND,
	AST_GTN, // greater than
	AST_LTN, // lesser than
	AST_GTE, // greater or eq to
	AST_LTE, // lesser or eq to
	AST_EQ,
	AST_AT,
	AST_ADD,
	AST_SUB,
	AST_MUL,
	AST_DIV,
	AST_MOD,
	AST_NOT,
	AST_ID,
	AST_STR,
	AST_DECL,
	AST_NIL,
	AST_TRUE,
	AST_LET,
	AST_CHAR,
	AST_PATH,
	AST_PRIMITIVE_TYPE,
	AST_AS,
} NodeType;

struct Position {
	int byte_offset {0};
	int line {0};
	int column {0};
};

struct Location {
	Position begin {};
	Position end {};
};

typedef struct NodeIndex {
	int index;
} NodeIndex;

typedef struct AST AST;

// AST
void ast_print(AST* ast, STR_POOL pool);
void ast_set_root(AST* ast, NodeIndex node_idx);

// nodes
NodeIndex new_node(AST* ast, NodeType type, std::vector<NodeIndex> children);
NodeIndex new_list_node(AST* ast);
NodeIndex new_string_node(
	AST* ast, NodeType type, Location loc, STR_POOL pool, String str
);
NodeIndex new_number_node(AST* ast, Location loc, Number num);
NodeIndex new_nil_node(AST* ast, Location loc);
NodeIndex new_true_node(AST* ast, Location loc);
NodeIndex new_char_node(AST* ast, Location loc, char character);
NodeIndex new_empty_node(AST* ast);
NodeIndex list_append_node(AST* ast, NodeIndex list, NodeIndex next);
NodeIndex list_prepend_node(AST* ast, NodeIndex list, NodeIndex next);

typedef struct BranchNode {
	size_t children_count;
	NodeIndex* children;
} BranchNode;

struct Node {
	NodeType type;
	Location loc;
	union {
		Number num;
		StrID str_id;
		char character;
		BranchNode branch;
	};

	NodeIndex& operator[](size_t index) const;
};

struct AST {
	~AST();

	// constructor initializes to an invalid initial state
	// after parsing, this should be a proper index
	NodeIndex root_index {-1};
	std::array<Node, 2048> nodes {};

	Node& at(NodeIndex);
	NodeIndex alloc_node();

 private:
	NodeIndex next_free_index {0};
};

#endif
