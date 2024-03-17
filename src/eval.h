#ifndef FALA_EVAL_H
#define FALA_EVAL_H

#include "ast.h"

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

typedef struct Environment {
	VarTable vars;
} Environment;

typedef struct EnvironmentStack {
	size_t len;
	size_t cap;
	Environment* envs;
} EnvironmentStack;

typedef struct Interpreter {
	EnvironmentStack envs;
} Interpreter;

Interpreter interpreter_init(void);
void interpreter_deinit(Interpreter* inter);

Value ast_eval(Interpreter* inter, SymbolTable* syms, AST ast);

#endif
