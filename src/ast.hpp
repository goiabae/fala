#ifndef FALA_AST_H
#define FALA_AST_H

#include <stddef.h>

#include <array>
#include <vector>

#include "str_pool.h"

typedef int Number;
typedef char* String;

enum class NodeType {
	EMPTY, // node for saying there is no node
	APP,   // function application
	NUM,
	BLK, // block
	IF,
	WHEN,
	FOR,
	WHILE,
	BREAK,
	CONTINUE,
	ASS,
	OR,
	AND,
	GTN, // greater than
	LTN, // lesser than
	GTE, // greater or eq to
	LTE, // lesser or eq to
	EQ,
	AT,
	ADD,
	SUB,
	MUL,
	DIV,
	MOD,
	NOT,
	ID,
	STR,
	VAR_DECL,
	FUN_DECL,
	NIL,
	TRUE,
	FALSE,
	LET,
	CHAR,
	PATH,
	INT_TYPE,
	UINT_TYPE,
	BOOL_TYPE,
	NIL_TYPE,
	AS,
};

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
void ast_print_detailed(AST* ast, STR_POOL pool);
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
NodeIndex new_false_node(AST* ast, Location loc);
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

	NodeIndex* begin() const { return &branch.children[0]; }
	NodeIndex* end() const { return &branch.children[branch.children_count]; }
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

bool operator<(NodeIndex a, NodeIndex b);

#endif
