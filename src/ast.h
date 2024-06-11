#ifndef FALA_AST_H
#define FALA_AST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "str_pool.h"

typedef int Number;
typedef char* String;

typedef enum Type {
	AST_APP, // function application
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
} Type;

typedef struct Location {
	int first_line;
	int first_column;
	int last_line;
	int last_column;
} Location;

typedef struct Node {
	Type type;
	Location loc;
	union {
		Number num;
		StrID str_id;
		struct {
			size_t children_count;
			struct Node* children;
		};
	};
} Node;

typedef struct AST {
	Node root;
} AST;

// AST
AST ast_init(void);
void ast_deinit(AST ast);
void ast_print(AST ast, STR_POOL pool);

// nodes
Node new_node(Type type, size_t len, Node* children);
Node new_list_node(void);
Node new_string_node(Type type, Location loc, STR_POOL pool, String str);
Node new_number_node(Location loc, Number num);
Node new_nil_node(Location loc);
Node new_true_node(Location loc);
Node list_append_node(Node list, Node next);
Node list_prepend_node(Node list, Node next);

#ifdef __cplusplus
}
#endif

#endif
