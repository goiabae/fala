#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include <map>
#include <optional>

#include "ast.hpp"
#include "index.hpp"
#include "str_pool.h"

enum class RelativeScope {
	GLOBAL,
	LOCAL,
	BUILTIN,
	FREE,
};

using ScopeIndex = Index<5>;
using FunctionIndex = Index<6>;

class SymbolTable {
 public:
	Guid define(StrID, ScopeIndex, NodeIndex);
	Guid use_of(StrID, ScopeIndex, NodeIndex);
	ScopeIndex make_child_scope(ScopeIndex);
	ScopeIndex make_function_scope(ScopeIndex, NodeIndex);
	std::optional<RelativeScope> relative_scope_of(Guid, NodeIndex) const;
	RelativeScope relative_scope_of(Guid, FunctionIndex) const;
	std::optional<StrID> name_of(Guid) const;
	std::optional<ScopeIndex> declaration_scope_of(Guid) const;
	std::vector<Guid> symbols_declared_in(ScopeIndex) const;
	Guid define_builtin(StrID);
	std::vector<Guid> all_symbols() const;
	std::vector<Guid> symbols_with_name(StrID) const;
	std::optional<FunctionIndex> function_of(ScopeIndex) const;
	std::optional<FunctionIndex> function_of(NodeIndex) const;
	std::vector<Guid> symbols_used_in(ScopeIndex) const;
	std::vector<Guid> free_symbols_in(FunctionIndex) const;
	ScopeIndex scope_at(NodeIndex) const;
	std::optional<ScopeIndex> parent_of(ScopeIndex) const;

 public:
	static constexpr ScopeIndex builtin_scope {.index = 0};
	static constexpr ScopeIndex root_scope {.index = 1};

 private:
	struct Symbol {
		ScopeIndex declaration_scope;
		StrID name;
	};

	struct Scope {
		std::optional<ScopeIndex> parent;
		std::optional<FunctionIndex> function;
	};

 private:
	std::optional<Symbol> resolve(Guid) const;
	bool is_child_of(ScopeIndex, ScopeIndex) const;
	bool is_parent_of(ScopeIndex, ScopeIndex) const;
	Scope get_scope(ScopeIndex) const;
	std::vector<ScopeIndex> function_scopes(FunctionIndex) const;

 private:
	std::vector<Symbol> m_symbols {};
	std::vector<Scope> m_scopes {
		Scope {
			.parent = std::nullopt,
			.function = std::nullopt,
		},
		Scope {
			.parent = builtin_scope,
			.function = std::nullopt,
		}
	};
	std::map<NodeIndex, ScopeIndex> m_node_scopes {};
	std::map<ScopeIndex, std::vector<Guid>> m_symbols_in_scope {};
	std::map<NodeIndex, FunctionIndex> m_body_function {};
	std::size_t m_next_function_index {0};
};

#endif
