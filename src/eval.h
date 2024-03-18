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
		VALUE_NIL,
		VALUE_TRUE,
	} tag;
	union {
		Number num;
		struct {
			size_t len;
			struct Value* data;
		} arr;
		Funktion func;
		String str;
		void* nil;
	};
} Value;

typedef struct Variable {
	size_t sym;   // symbol table index
	size_t scope; // declaration scope index
} Variable;

typedef struct Environment {
	size_t len;
	size_t cap;
	size_t scope_count;
	Variable* vars;
	Value* values;
} Environment;

typedef struct Interpreter {
	Environment env;
} Interpreter;

Interpreter interpreter_init(SymbolTable* syms);
void interpreter_deinit(Interpreter* inter);

Value ast_eval(Interpreter* inter, SymbolTable* syms, AST ast);

void print_value(Value val);

#endif
