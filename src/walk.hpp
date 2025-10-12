#ifndef FALA_EVAL_HPP
#define FALA_EVAL_HPP

#include <stdbool.h>

#include <memory>
#include <ostream>
#include <variant>

#include "ast.hpp"
#include "env.hpp"
#include "str_pool.h"

namespace walk {

struct BuiltinFunction;

struct CustomFunction {
	std::vector<NodeIndex> param_idxs;
	NodeIndex body_idx;
};

struct Array;

struct Nil {};

using Value = std::variant<
	BuiltinFunction, CustomFunction, Array, int, bool, std::string, Nil>;

using ValueCell = std::shared_ptr<Value>;

struct Array {
	size_t size;
	std::vector<ValueCell> items;
};

struct BuiltinFunction {
	size_t param_count;
	ValueCell (*builtin)(std::vector<ValueCell>);
};

struct Interpreter {
	Interpreter(StringPool& _pool);

	StringPool& pool;
	Env<ValueCell> env;

	bool in_loop;
	bool should_break;
	bool should_continue;

	ValueCell eval(AST& ast);
};

void print_value(ValueCell val);

} // namespace walk

#endif
