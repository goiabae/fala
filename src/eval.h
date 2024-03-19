#ifndef FALA_EVAL_H
#define FALA_EVAL_H

#include "ast.h"
#include "env.h"

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

typedef struct ValueStack {
	size_t len;
	size_t cap;
	Value* values;
} ValueStack;

typedef struct Interpreter {
	SymbolTable syms;
	ValueStack values;
	Environment env;
} Interpreter;

Interpreter interpreter_init();
void interpreter_deinit(Interpreter* inter);

Value ast_eval(Interpreter* inter, AST ast);

void print_value(Value val);

#endif
