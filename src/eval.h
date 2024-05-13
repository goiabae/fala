#ifndef FALA_EVAL_H
#define FALA_EVAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"
#include "env.h"

struct Value;

typedef struct Function {
	bool is_builtin;
	union {
		struct Value (*builtin)(size_t, struct Value*);
		struct {
			size_t argc;
			Node* args;
			Node root;
		};
	};
} Function;

enum ValueKind {
	VALUE_NUM,
	VALUE_ARR,
	VALUE_STR,
	VALUE_FUN,
	VALUE_NIL,
	VALUE_TRUE
};

typedef struct Value {
	enum ValueKind tag;
	union {
		Number num;
		struct {
			size_t len;
			struct Value* data;
		} arr;
		Function func;
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

	bool in_loop;
	bool should_break;
	bool should_continue;
} Interpreter;

Interpreter interpreter_init();
void interpreter_deinit(Interpreter* inter);

Value ast_eval(Interpreter* inter, AST ast);

void print_value(Value val);

#ifdef __cplusplus
}
#endif

#endif
