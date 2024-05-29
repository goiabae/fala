#ifndef FALA_EVAL_H
#define FALA_EVAL_H

#include <stdbool.h>

#include "ast.h"
#include "str_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Value;

typedef struct Function {
	bool is_builtin;
	union {
		struct Value (*builtin)(size_t, struct Value*);
		struct {
			Node* params;
			Node root;
		};
	};
	size_t param_count;
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
		String str; // owned string
		void* nil;
	};
} Value;

typedef struct ValueStack {
	size_t len;
	size_t cap;
	Value* values;
} ValueStack;

typedef struct Environment {
	size_t len;
	size_t cap;
	size_t scope_count;

	// index into some other structure like a value or register stack
	size_t* indexes;
	size_t* syms;   // symbol table index
	size_t* scopes; // which scope each item belongs to
} Environment;

typedef struct Interpreter {
	STR_POOL pool;
	ValueStack values;
	Environment env;

	bool in_loop;
	bool should_break;
	bool should_continue;
} Interpreter;

Interpreter interpreter_init(STR_POOL pool);
void interpreter_deinit(Interpreter* inter);

Value inter_eval(Interpreter* inter, AST ast);

void print_value(Value val);

#ifdef __cplusplus
}
#endif

#endif
