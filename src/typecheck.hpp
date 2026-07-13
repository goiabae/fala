#ifndef FALA_TYPECHECK_HPP
#define FALA_TYPECHECK_HPP

#include <stdio.h>

#include <map>
#include <vector>

#include "ast.hpp"
#include "env.hpp"
#include "logger.hpp"
#include "str_pool.h"
#include "type.hpp"

#define T std::shared_ptr<Type>
#define D std::shared_ptr<Datatype>
#define M std::shared_ptr<Mode>

struct Typechecker {
	Typechecker(AST& ast, StringPool& pool)
	: ast(ast), pool(pool), logger("TYPECHECKER", ast.file_name, ast.lines) {}

	bool typecheck();
	T typecheck(NodeIndex node_idx, Env<T>::ScopeID scope_id);

	D substitute(D gen, std::vector<D> args);
	D substitute_aux(
		D body, std::vector<TYPE_VARIABLE> vars, std::vector<D> args
	);

	D eval(NodeIndex, Env<T>::ScopeID scope_id);

	bool unify(M, M);
	bool unify(D, D);
	bool unify(T, T);
	void mismatch_error(Location, const char*, T, T);
	bool cast_to(T, T);
	D make_datavar();
	M make_modevar();
	T deref(T);

	AST& ast;
	StringPool& pool;

	size_t next_var_id {0};
	size_t next_modevar_id {0};
	Env<T> env {};
	std::map<NodeIndex, Env<T>::ScopeID> node_to_scope_id {};
	std::map<NodeIndex, T> node_to_type {};
	Logger logger;
};

#undef T
#undef D
#undef M

#endif
