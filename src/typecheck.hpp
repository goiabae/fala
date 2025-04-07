#ifndef FALA_TYPECHECK_HPP
#define FALA_TYPECHECK_HPP

#include <stdio.h>

#include <map>
#include <vector>

#include "ast.hpp"
#include "env.hpp"
#include "str_pool.h"
#include "type.hpp"

struct Typechecker {
	Typechecker(AST& ast, StringPool& pool) : ast(ast), pool(pool) {}

	bool typecheck();
	TYPE typecheck(NodeIndex node_idx, Env<TYPE>::ScopeID scope_id);

	bool unify(TYPE, TYPE);
	void print_type(FILE* fd, TYPE);
	void mismatch_error(Location, const char*, TYPE, TYPE);

	TYPE make_nil();
	TYPE make_bool();
	TYPE make_void();
	TYPE make_integer(int bit_count, Sign sign);
	TYPE make_array(TYPE item_type);
	TYPE make_function(std::vector<TYPE> inputs, TYPE output);
	TYPE make_typevar();

	bool is_nil(TYPE);
	bool is_bool(TYPE);
	bool is_void(TYPE);

	bool is_integer(TYPE);
	INTEGER to_integer(TYPE);

	bool is_array(TYPE);
	ARRAY to_array(TYPE);

	bool is_function(TYPE);
	FUNCTION to_function(TYPE);

	bool is_typevar(TYPE);
	TYPE_VARIABLE to_typevar(TYPE);

	AST& ast;
	StringPool& pool;

	size_t next_var_id {0};
	Env<TYPE> env {};
	std::map<NodeIndex, Env<TYPE>::ScopeID> node_to_scope_id {};
	std::map<NodeIndex, TYPE> node_to_type {};
};

#endif
