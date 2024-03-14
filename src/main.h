#ifndef FALA_MAIN_H
#define FALA_MAIN_H

#include <stddef.h>

typedef int Value;

typedef struct VarTable {
	size_t len;
	size_t cap;
	const char** names;
	Value* values;
} VarTable;

void yyerror(void* var_table, char* s);

VarTable* var_table_init(void);
void var_table_deinit(VarTable* tab);
Value var_table_insert(VarTable* tab, const char* name, Value value);
Value var_table_get(VarTable* tab, const char* name);

typedef enum Type {
	FALA_NUM,
	FALA_THEN,
	FALA_ASS,
	FALA_OR,
	FALA_AND,
	FALA_GREATER,
	FALA_LESSER,
	FALA_GREATER_EQ,
	FALA_LESSER_EQ,
	FALA_EQ_EQ,
	FALA_ADD,
	FALA_SUB,
	FALA_MUL,
	FALA_DIV,
	FALA_MOD,
	FALA_NOT,
	FALA_ID,
} Type;

typedef struct Node {
	Type type;
	void* data; // optional data
	size_t children_count;
	struct Node* children;
} Node;

Node new_node(Type type, void* data, size_t len, Node children[len]);

typedef struct AST {
	Node root;
} AST;

typedef struct Context {
	AST ast;
} Context;

#endif
