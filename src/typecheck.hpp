#ifndef FALA_TYPECHECK_HPP
#define FALA_TYPECHECK_HPP

#include <stdio.h>

#include <format>
#include <map>
#include <unordered_set>
#include <vector>

#include "ast.hpp"
#include "logger.hpp"
#include "str_pool.h"
#include "symbol_table.hpp"
#include "type.hpp"

struct Typechecker {
	Typechecker(AST& ast, StringPool& pool, const SymbolTable& symtab)
	: ast(ast),
		pool(pool),
		symtab(symtab),
		logger("TYPECHECKER", ast.file_name, ast.lines) {}

	bool typecheck();

 private:
	using T = std::shared_ptr<Type>;
	using D = std::shared_ptr<Datatype>;
	using M = std::shared_ptr<Mode>;
	using S = std::shared_ptr<DataScheme>;
	using P = std::shared_ptr<PolyType>;
	using V = std::shared_ptr<TypeVariable>;
	using E = std::shared_ptr<EffectSet>;
	using TE = std::pair<T, E>;

 private:
	TE typecheck(NodeIndex node_idx);
	void typecheck_pattern(NodeIndex node_idx, D data);
	D eval(NodeIndex);
	E eval_effset(NodeIndex);
	template<typename... Args>
	void mismatch_error(
		Location, std::format_string<Args...>, T, T, Args... args
	);
	std::unordered_set<V> free_type_variables(ScopeIndex);
	std::unordered_set<V> free_type_variables(D);
	void declare(NodeIndex node_idx);

 public:
	std::map<Guid, P> variables;
	std::map<NodeIndex, T> node_to_type {};
	std::unordered_set<Guid> constructors {};
	std::map<Guid, std::shared_ptr<Effect>> name_to_effect {};

 private:
	AST& ast;
	StringPool& pool;
	const SymbolTable& symtab;
	size_t next_var_id {0};
	size_t next_modevar_id {0};
	size_t next_effsetvar_id {0};
	std::map<Guid, D> type_names {};
	std::map<Guid, E> effset_names {};
	Logger logger;

 public:
	T instantiate(P);
	P generalize(T, ScopeIndex);
	P generalize(T, std::unordered_set<V>);
	P dont_generalize(T, ScopeIndex);
	D substitute_bind(const std::map<std::size_t, V>&, D);
	D substitute(const std::map<std::size_t, V>&, D);

 public:
	bool unify(M, M);
	bool unify(D, D);
	bool unify(T, T);
	bool unify(E, E);
	bool cast(T, T);
	bool cast(D, D);
	bool cast(E, E);
	bool cast(M, M);
	bool submode(M sub, M sup);

 public:
	std::shared_ptr<TypeVariable> make_datavar();
	std::shared_ptr<TypeVariable> make_genvar();
	M make_modevar();
	E make_effsetvar();

 private:
	bool unify_aux(D, D, bool should_cast);
	bool unify_aux(M, M, bool should_cast);
	bool unify_aux(T, T, bool should_cast);
	bool unify_aux(E, E, bool should_cast);
	D instantiate(const std::map<V, V>&, D);
};

#endif
