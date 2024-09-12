#ifndef FALA_EVAL_HPP
#define FALA_EVAL_HPP

#include <stdbool.h>

#include <ostream>

#include "ast.h"
#include "env.hpp"
#include "str_pool.h"

namespace walk {

struct Value;

struct Function {
	struct UserDefined {
		NodeIndex* params;
		NodeIndex root;
	};

	using Builtin = Value (*)(size_t, Value*);

	bool is_builtin;

	union {
		Value (*builtin)(size_t, struct Value*);
		UserDefined custom;
	};

	size_t param_count;

	Function(Builtin _builtin, size_t count)
	: is_builtin(true), builtin(_builtin), param_count(count) {}

	Function(NodeIndex root, NodeIndex* params, size_t count)
	: is_builtin(false), custom {params, root}, param_count(count) {}
};

struct Arr {
	size_t len;
	struct Value* data;
};

struct Value {
	enum class Type { NUM, ARR, STR, FUN, NIL, TRUE, VAR };

	Type type;
	union {
		Number num;
		Arr arr;
		Function func;
		String str; // owned string
		Value* var;
	};

	Value() : type(Type::NIL) {}
	Value(bool x) { type = x ? Type::TRUE : Type::NIL; }
	Value(Number _num) : type(Type::NUM), num(_num) {}
	Value(String _str) : type(Type::STR), str(_str) {}
	Value(Function _func) : type(Type::FUN), func(_func) {}
	Value(Value* var) : type(Type::VAR), var(var) {}

	operator bool() const { return type != Type::NIL; }

	Value to_rvalue() { return (type == Type::VAR) ? *var : *this; }

	friend std::ostream& operator<<(std::ostream& st, Value& val);

	void deinit();
};

struct Interpreter {
	Interpreter(STR_POOL _pool);

	STR_POOL pool;
	Env<Value> env;

	bool in_loop;
	bool should_break;
	bool should_continue;

	Value eval(AST& ast);
};

void print_value(Value val);

} // namespace walk

#endif
