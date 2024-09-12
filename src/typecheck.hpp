#ifndef FALA_TYPECHECK_HPP
#define FALA_TYPECHECK_HPP

#include <utility>
#include <vector>

#include "ast.h"
#include "env.hpp"
#include "str_pool.h"
#include "type.hpp"

struct Typechecker {
	size_t next_var_id {0};

	Typechecker(StringPool& pool) : pool(pool) {}

	~Typechecker() {
		for (Type* t : ts) delete t;
	}

	Type* make_var() {
		auto t = new TypeVariable {next_var_id++};
		ts.push_back(t);
		return t;
	}

	Type* add_type(Type* t) {
		ts.push_back(t);
		return t;
	}

	std::vector<Type*> ts {};
	Env<Type*> env {};
	StringPool& pool;
};

bool typecheck(AST& ast, StringPool& pool);

#endif
