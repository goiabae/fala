#ifndef FALA_EVAL_HPP
#define FALA_EVAL_HPP

#include <stdbool.h>

#include <ostream>

#include "ast.h"
#include "str_pool.h"

struct Value;

struct Function {
	bool is_builtin;
	union {
		struct Value (*builtin)(size_t, struct Value*);
		struct {
			Node* params;
			Node root;
		};
	};
	size_t param_count;

	Function(struct Value (*_builtin)(size_t, struct Value*), size_t count)
	: is_builtin(true), builtin(_builtin), param_count(count) {}

	Function(Node _root, Node* _params, size_t count)
	: is_builtin(false), params(_params), root(_root), param_count(count) {}
};

struct Arr {
	size_t len;
	struct Value* data;
};

struct Value {
	enum class Type { NUM, ARR, STR, FUN, NIL, TRUE };

	Type type;
	union {
		Number num;
		Arr arr;
		Function func;
		String str; // owned string
	};

	Value() : type(Type::NIL) {}
	Value(bool x) { type = x ? Type::TRUE : Type::NIL; }
	Value(Number _num) : type(Type::NUM), num(_num) {}
	Value(String _str) : type(Type::STR), str(_str) {}
	Value(Function _func) : type(Type::FUN), func(_func) {}

	operator bool() const { return type != Type::NIL; }

	friend std::ostream& operator<<(std::ostream& st, Value& val);
};

struct ValueStack {
	size_t len;
	size_t cap;
	Value* values;
};

struct Environment {
	size_t len;
	size_t cap;
	size_t scope_count;

	// index into some other structure like a value or register stack
	size_t* indexes;
	size_t* syms;   // symbol table index
	size_t* scopes; // which scope each item belongs to
};

struct Interpreter {
	Interpreter(STR_POOL _pool);
	~Interpreter();

	STR_POOL pool;
	ValueStack values;
	Environment env;

	bool in_loop;
	bool should_break;
	bool should_continue;

	Value eval(AST ast);
};

void print_value(Value val);

#endif