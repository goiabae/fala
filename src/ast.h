#ifndef FALA_AST_H
#define FALA_AST_H

#include <stddef.h>

typedef int Number;
typedef char* String;

typedef enum Type {
	FALA_APP,
	FALA_NUM,
	FALA_BLOCK,
	FALA_IF,
	FALA_WHEN,
	FALA_FOR,
	FALA_WHILE,
	FALA_ASS,
	FALA_OR,
	FALA_AND,
	FALA_GREATER,
	FALA_LESSER,
	FALA_GREATER_EQ,
	FALA_LESSER_EQ,
	FALA_EQ,
	FALA_ADD,
	FALA_SUB,
	FALA_MUL,
	FALA_DIV,
	FALA_MOD,
	FALA_NOT,
	FALA_ID,
	FALA_STRING,
	FALA_DECL,
	FALA_VAR,
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

// nodes
Node new_node(Type type, size_t len, Node children[len]);
Node new_list_node();
Node new_string_node(Type type, SymbolTable* tab, String str);
Node new_number_node(Number num);
Node list_append_node(Node list, Node next);

// symbol table
SymbolTable sym_table_init();
void sym_table_deinit(SymbolTable* tab);
size_t sym_table_insert(SymbolTable* tab, String str);
String sym_table_get(SymbolTable* tab, size_t index);

#endif
