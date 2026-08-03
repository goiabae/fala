#ifndef FALA_TYPECHECK_HPP
#define FALA_TYPECHECK_HPP

#include <stdio.h>

#include <map>
#include <unordered_set>
#include <vector>

#include "ast.hpp"
#include "logger.hpp"
#include "str_pool.h"
#include "symbol_table.hpp"
#include "type.hpp"

#define T std::shared_ptr<Type>
#define D std::shared_ptr<Datatype>
#define M std::shared_ptr<Mode>
#define S std::shared_ptr<DataScheme>
#define P std::shared_ptr<PolyType>
#define V std::shared_ptr<TypeVariable>

struct Typechecker {
	Typechecker(AST& ast, StringPool& pool, const SymbolTable& symtab)
	: ast(ast),
		pool(pool),
		symtab(symtab),
		logger("TYPECHECKER", ast.file_name, ast.lines) {}

	bool typecheck();
	T typecheck(NodeIndex node_idx);

	D eval(NodeIndex);

	bool unify(M, M);
	bool unify(D, D);
	bool unify(T, T);
	void mismatch_error(Location, const char*, T, T);
	D make_datavar();
	D make_genvar();
	M make_modevar();
	T deref(T);
	bool submode(M sub, M sup);
	P generalize(T, ScopeIndex);
	P generalize(T, std::unordered_set<V>);
	P dont_generalize(T, ScopeIndex);
	std::unordered_set<V> free_type_variables(ScopeIndex);
	std::unordered_set<V> free_type_variables(D);
	T instantiate(P);
	D substitute(const std::map<std::size_t, V>&, D);

	AST& ast;
	StringPool& pool;
	const SymbolTable& symtab;

	size_t next_var_id {0};
	size_t next_modevar_id {0};
	std::map<Guid, P> variables;
	std::map<NodeIndex, T> node_to_type {};
	Logger logger;

 private:
	D instantiate(const std::map<V, V>&, D);
};

#undef T
#undef D
#undef M
#undef S
#undef P
#undef V

#endif
