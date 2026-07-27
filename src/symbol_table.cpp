#include "symbol_table.hpp"

#include <cassert>
#include <optional>
#include <ranges>
#include <stdexcept>

#include "ast.hpp"

std::optional<SymbolTable::Symbol> SymbolTable::resolve(Guid guid) const {
	if (guid.index >= m_symbols.size())
		return std::nullopt;
	else
		return m_symbols[guid.index];
}

ScopeIndex SymbolTable::scope_at(NodeIndex node_idx) const {
	if (m_node_scopes.contains(node_idx))
		return m_node_scopes.at(node_idx);
	else
		throw std::runtime_error("node index is not associated with any scope");
}

bool SymbolTable::is_child_of(ScopeIndex child, ScopeIndex parent) const {
	auto p = parent_of(child);
	if (p.has_value()) {
		const auto c = p.value();
		return c == parent || is_child_of(c, parent);
	} else {
		return false;
	}
}

bool SymbolTable::is_parent_of(ScopeIndex parent, ScopeIndex child) const {
	return is_child_of(child, parent);
}

std::optional<ScopeIndex> SymbolTable::parent_of(ScopeIndex i) const {
	return get_scope(i).parent;
}

SymbolTable::Scope SymbolTable::get_scope(ScopeIndex i) const {
	if (i.index >= m_scopes.size())
		throw std::runtime_error("invalid scope index (above bounds)");
	else
		return m_scopes.at(i.index);
}

Guid SymbolTable::define(StrID name, ScopeIndex scope, NodeIndex node) {
	m_symbols.emplace_back(scope, name);
	m_node_scopes[node] = scope;
	return Guid {.index = static_cast<Guid::index_type>(m_symbols.size() - 1)};
}

Guid SymbolTable::define_builtin(StrID name) {
	m_symbols.emplace_back(builtin_scope, name);
	return Guid {.index = static_cast<Guid::index_type>(m_symbols.size() - 1)};
}

Guid SymbolTable::use_of(StrID name, ScopeIndex scope, NodeIndex node) {
	m_node_scopes[node] = scope;
	for (const auto& [i, s] : std::ranges::views::enumerate(m_symbols)) {
		if (name == s.name
		    and (is_child_of(scope, s.declaration_scope) or scope == s.declaration_scope))
			return Guid {.index = static_cast<Guid::index_type>(i)};
	}
	throw std::runtime_error("undefined symbol");
}

ScopeIndex SymbolTable::make_child_scope(ScopeIndex s) {
	Scope child {
		.parent = s,
		.function = get_scope(s).function,
	};
	m_scopes.push_back(child);
	return ScopeIndex {
		.index = static_cast<ScopeIndex::index_type>(m_scopes.size() - 1),
	};
}

ScopeIndex SymbolTable::make_function_scope(ScopeIndex s) {
	Scope child {
		.parent = s,
		.function = FunctionIndex {
			.index = static_cast<FunctionIndex::index_type>(m_next_function_index++)
		},
	};
	m_scopes.push_back(child);
	return ScopeIndex {
		.index = static_cast<ScopeIndex::index_type>(m_scopes.size() - 1),
	};
}

std::optional<RelativeScope> SymbolTable::relative_scope_of(
	Guid guid, NodeIndex node_idx
) const {
	auto node_scope = scope_at(node_idx);
	auto maybe_decl_scope = declaration_scope_of(guid);
	if (maybe_decl_scope.has_value()) {
		auto decl_scope = maybe_decl_scope.value();
		if (decl_scope == builtin_scope) {
			return RelativeScope::BUILTIN;
		} else if (decl_scope == root_scope) {
			return RelativeScope::GLOBAL;
		} else if (node_scope == decl_scope) {
			return RelativeScope::LOCAL;
		} else if (is_child_of(node_scope, decl_scope)) {
			return RelativeScope::FREE;
		} else {
			assert(false);
		}
	} else {
		return std::nullopt;
	}
}

std::optional<StrID> SymbolTable::name_of(Guid guid) const {
	constexpr auto get_name = [](auto&& x) { return x.name; };
	return resolve(guid).transform(get_name);
}

std::optional<ScopeIndex> SymbolTable::declaration_scope_of(Guid guid) const {
	constexpr auto get_declaration_scope = [](auto&& x) {
		return x.declaration_scope;
	};
	return resolve(guid).transform(get_declaration_scope);
}

std::vector<Guid> SymbolTable::symbols_in(ScopeIndex index) const {
	std::vector<Guid> guids {};
	for (const auto& [i, s] : std::ranges::views::enumerate(m_symbols)) {
		if (s.declaration_scope == index)
			guids.push_back(Guid {.index = static_cast<Guid::index_type>(i)});
	};
	return guids;
}

std::vector<ScopeIndex> SymbolTable::function_scopes(
	FunctionIndex function
) const {
	std::vector<ScopeIndex> result {};
	for (const auto& [i, s] : std::ranges::views::enumerate(m_scopes))
		if (s.function.has_value() and s.function.value() == function)
			result.push_back(
				ScopeIndex {.index = static_cast<ScopeIndex::index_type>(i)}
			);
	return result;
}

std::vector<Guid> SymbolTable::all_symbols() const {
	std::vector<Guid> syms {};
	for (auto i = 0ul; i < m_symbols.size(); i++) {
		syms.push_back(Guid {.index = static_cast<Guid::index_type>(i)});
	}
	return syms;
}
