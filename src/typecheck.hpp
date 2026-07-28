#ifndef FALA_TYPECHECK_HPP
#define FALA_TYPECHECK_HPP

#include <stdio.h>

#include <map>
#include <vector>

#include "ast.hpp"
#include "env.hpp"
#include "logger.hpp"
#include "str_pool.h"
#include "symbol_table.hpp"
#include "type.hpp"

#define T std::shared_ptr<Type>
#define D std::shared_ptr<Datatype>
#define M std::shared_ptr<Mode>

struct Typechecker {
	Typechecker(AST& ast, StringPool& pool, const SymbolTable& symtab)
	: ast(ast),
		pool(pool),
		symtab(symtab),
		logger("TYPECHECKER", ast.file_name, ast.lines) {}

	bool typecheck();
	T typecheck(NodeIndex node_idx);

	D substitute(D gen, std::vector<D> args);
	D substitute_aux(
		D body, std::vector<TYPE_VARIABLE> vars, std::vector<D> args
	);

	D eval(NodeIndex);

	bool unify(M, M);
	bool unify(D, D);
	bool unify(T, T);
	void mismatch_error(Location, const char*, T, T);
	bool cast_to(T, T);
	D make_datavar();
	M make_modevar();
	T deref(T);
	bool submode(M sub, M sup);

	AST& ast;
	StringPool& pool;
	const SymbolTable& symtab;

	size_t next_var_id {0};
	size_t next_modevar_id {0};
	std::map<Guid, T> variables;
	std::map<NodeIndex, Env<T>::ScopeID> node_to_scope_id {};
	std::map<NodeIndex, T> node_to_type {};
	Logger logger;
};

#undef T
#undef D
#undef M

#endif
