#ifndef SCOPE_ANALYSER_HPP
#define SCOPE_ANALYSER_HPP

#include "ast.hpp"
#include "str_pool.h"
#include "symbol_table.hpp"

class ScopeAnalyser {
 public:
	ScopeAnalyser(AST& ast, StringPool& pool) : m_ast(ast), m_pool(pool) {}

 public:
	SymbolTable analyse();

 private:
	void analyse(NodeIndex, ScopeIndex);
	void analyse_type(NodeIndex, ScopeIndex);
	void analyse_pattern(NodeIndex, ScopeIndex);
	void analyse_declaration(NodeIndex, ScopeIndex);
	void define_builtin(const char*);
	void define_builtin_type(const char*);
	void define_builtin_effset(const char*);
	Guid define(NodeIndex, ScopeIndex, Namespace);
	Guid refere(NodeIndex, ScopeIndex, Namespace);

 private:
	AST& m_ast;
	StringPool& m_pool;
	SymbolTable m_symtab;
};

#endif
