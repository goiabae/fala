#include "symbol_table.hpp"

#include <algorithm>
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
	auto guid =
		Guid {.index = static_cast<Guid::index_type>(m_symbols.size() - 1)};
	if (not m_symbols_in_scope.contains(scope)) {
		m_symbols_in_scope[scope] = {guid};
	} else {
		m_symbols_in_scope.at(scope).push_back(guid);
	}
	return guid;
}

Guid SymbolTable::define_builtin(StrID name) {
	m_symbols.emplace_back(builtin_scope, name);
	auto guid =
		Guid {.index = static_cast<Guid::index_type>(m_symbols.size() - 1)};
	if (not m_symbols_in_scope.contains(builtin_scope)) {
		m_symbols_in_scope[builtin_scope] = {guid};
	} else {
		m_symbols_in_scope.at(builtin_scope).push_back(guid);
	}
	return guid;
}

Guid SymbolTable::use_of(StrID name, ScopeIndex scope, NodeIndex node) {
	m_node_scopes[node] = scope;
	for (const auto& [i, s] : std::ranges::views::enumerate(m_symbols)) {
		if (name == s.name
		    and (is_child_of(scope, s.declaration_scope) or scope == s.declaration_scope)) {
			const auto guid = Guid {.index = static_cast<Guid::index_type>(i)};
			if (not m_symbols_in_scope.contains(scope)) {
				m_symbols_in_scope[scope] = {guid};
			} else if (std::ranges::find(m_symbols_in_scope.at(scope), guid)
			           == m_symbols_in_scope.at(scope).end()) {
				m_symbols_in_scope.at(scope).push_back(guid);
			}
			return guid;
		}
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

ScopeIndex SymbolTable::make_function_scope(ScopeIndex s, NodeIndex body) {
	FunctionIndex f {
		.index = static_cast<FunctionIndex::index_type>(m_next_function_index++)
	};
	Scope child {
		.parent = s,
		.function = f,
	};
	m_scopes.push_back(child);
	m_body_function[body] = f;
	return ScopeIndex {
		.index = static_cast<ScopeIndex::index_type>(m_scopes.size() - 1),
	};
}

std::optional<RelativeScope> SymbolTable::relative_scope_of(
	Guid guid, NodeIndex node_idx
) const {
	auto maybe_decl_scope = declaration_scope_of(guid);
	if (maybe_decl_scope.has_value()) {
		auto decl_scope = maybe_decl_scope.value();
		auto decl_function = function_of(decl_scope);
		if (decl_scope == builtin_scope) {
			return RelativeScope::BUILTIN;
		} else if (not decl_function.has_value()) {
			return RelativeScope::GLOBAL;
		}
		// If not a builtin and not a global, must be inside a function
		const auto declaration_function = function_of(decl_scope).value();
		const auto usage_scope = scope_at(node_idx);
		const auto usage_function = function_of(usage_scope).value();
		if (usage_function != declaration_function)
			return RelativeScope::FREE;
		else
			return RelativeScope::LOCAL;
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

std::vector<Guid> SymbolTable::symbols_declared_in(ScopeIndex index) const {
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

std::vector<Guid> SymbolTable::symbols_with_name(StrID name) const {
	const std::vector<Guid> syms = all_symbols();
	std::vector<Guid> with_name {};
	for (auto sym : syms) {
		const auto name_ = name_of(sym).value();
		if (name_ == name) with_name.push_back(sym);
	}
	return with_name;
}

std::optional<FunctionIndex> SymbolTable::function_of(ScopeIndex idx) const {
	auto scope = get_scope(idx);
	return scope.function;
}

std::optional<FunctionIndex> SymbolTable::function_of(NodeIndex node) const {
	if (m_node_scopes.contains(node))
		return function_of(m_node_scopes.at(node));
	else if (m_body_function.contains(node))
		return m_body_function.at(node);
	else
		return std::nullopt;
}

std::vector<Guid> SymbolTable::free_symbols_in(FunctionIndex func) const {
	std::vector<Guid> free_symbols {};
	auto scopes = function_scopes(func);
	for (auto scope : scopes) {
		auto symbols = symbols_used_in(scope);
		for (auto symbol : symbols) {
			auto rel = relative_scope_of(symbol, func);
			if (rel == RelativeScope::FREE) {
				auto n = std::ranges::find(free_symbols, symbol) == free_symbols.end();
				if (n) {
					free_symbols.push_back(symbol);
				}
			}
		}
	}
	return free_symbols;
}

std::vector<Guid> SymbolTable::symbols_used_in(ScopeIndex scope) const {
	return m_symbols_in_scope.at(scope);
}

RelativeScope SymbolTable::relative_scope_of(
	Guid guid, FunctionIndex func
) const {
	auto symbol = resolve(guid).value();
	if (symbol.declaration_scope == builtin_scope) return RelativeScope::BUILTIN;
	auto maybe_decl_func = function_of(symbol.declaration_scope);
	if (maybe_decl_func.has_value()) {
		auto decl_func = maybe_decl_func.value();
		if (decl_func == func) {
			return RelativeScope::LOCAL;
		} else {
			return RelativeScope::FREE;
		}
	} else {
		return RelativeScope::GLOBAL;
	}
}

std::unordered_set<Guid> SymbolTable::symbols_reachable_from(
	ScopeIndex s
) const {
	std::unordered_set<Guid> reachable_symbols {};
	auto current_symbols = symbols_declared_in(s);
	for (auto y : current_symbols) reachable_symbols.insert(y);
	auto p = parent_of(s);
	if (p.has_value()) {
		reachable_symbols.merge(symbols_reachable_from(p.value()));
	}
	return reachable_symbols;
}
