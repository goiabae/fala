#ifndef FALA_AST_H
#define FALA_AST_H

#include <stddef.h>

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
	AST_ASS,
	AST_OR,
	AST_AND,
	AST_GTN, // greater than
	AST_LTN, // lesser than
	AST_GTE, // greater or eq to
	AST_LTE, // lesser or eq to
	AST_EQ,
	AST_ADD,
	AST_SUB,
	AST_MUL,
	AST_DIV,
	AST_MOD,
	AST_NOT,
	AST_ID,
	AST_STR,
	AST_DECL,
	AST_VAR, // variable id or id[idx]
	AST_NIL,
	AST_TRUE,
	AST_LET,
} Type;

typedef struct Node {
	Type type;
	union {
		Number num;
		size_t index;
		struct {
			size_t children_count;
			struct Node* children;
		};
	};
} Node;

typedef struct AST {
	Node root;
} AST;

typedef struct SymbolTable {
	size_t len;
	size_t cap;
	char** arr;
} SymbolTable;

// AST
AST ast_init(void);
void ast_deinit(AST ast);
void ast_print(AST ast, SymbolTable* syms);

// nodes
Node new_node(Type type, size_t len, Node* children);
Node new_list_node();
Node new_string_node(Type type, SymbolTable* tab, String str);
Node new_number_node(Number num);
Node new_nil_node();
Node new_true_node();
Node list_append_node(Node list, Node next);

// symbol table
SymbolTable sym_table_init();
void sym_table_deinit(SymbolTable* tab);
size_t sym_table_insert(SymbolTable* tab, String str);
String sym_table_get(SymbolTable* tab, size_t index);

#endif
