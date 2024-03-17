#ifndef FALA_MAIN_H
#define FALA_MAIN_H

#include <stddef.h>

typedef int Number;
typedef char* String;

struct Value;

typedef struct Value (*Funktion)(size_t, struct Value*);

typedef struct Value {
	enum {
		VALUE_NUM,
		VALUE_ARR,
		VALUE_STR,
		VALUE_FUN,
	} tag;
	union {
		Number num;
		struct {
			size_t len;
			struct Value* data;
		} arr;
		Funktion func;
		String str;
	};
} Value;

typedef struct VarTable {
	size_t len;
	size_t cap;
	const char** names;
	Value* values;
} VarTable;

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
	FALA_EQ_EQ,
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

typedef struct Context {
	AST ast;
} Context;

typedef struct SymbolTable {
	size_t len;
	size_t cap;
	char** arr;
} SymbolTable;

void yyerror(void* scanner, Context* ctx, SymbolTable* syms, char* err_msg);

Node new_node(Type type, size_t len, Node children[len]);
Node new_list_node();
Node new_string_node(Type type, SymbolTable* tab, String str);
Node new_number_node(Number num);
Node list_append_node(Node list, Node next);

typedef struct Environment {
	VarTable vars;
} Environment;

typedef struct EnvironmentStack {
	size_t len;
	size_t cap;
	Environment* envs;
} EnvironmentStack;

#endif
