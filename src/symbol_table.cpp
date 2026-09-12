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

Guid SymbolTable::define(
	StrID name, ScopeIndex scope, NodeIndex node, Namespace space
) {
	const Symbol sym {
		.declaration_scope = scope,
		.name = name,
		.space = space,
	};
	m_symbols.push_back(sym);
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

Guid SymbolTable::define_builtin(StrID name, Namespace space) {
	const Symbol sym {
		.declaration_scope = builtin_scope,
		.name = name,
		.space = space,
	};
	m_symbols.push_back(sym);
	auto guid =
		Guid {.index = static_cast<Guid::index_type>(m_symbols.size() - 1)};
	if (not m_symbols_in_scope.contains(builtin_scope)) {
		m_symbols_in_scope[builtin_scope] = {guid};
	} else {
		m_symbols_in_scope.at(builtin_scope).push_back(guid);
	}
	return guid;
}

std::optional<Guid> SymbolTable::use_of(
	StrID name, ScopeIndex scope, NodeIndex node, Namespace space
) {
	m_node_scopes[node] = scope;
	auto cur = scope;
	while (true) {
		auto syms = symbols_declared_in(cur);
		for (const auto s : syms) {
			const auto s_name = name_of(s).value();
			const auto s_space = namespace_of(s);
			if (name == s_name and s_space == space) {
				if (not m_symbols_in_scope.contains(scope)) {
					m_symbols_in_scope[scope] = {s};
				} else if (std::ranges::find(m_symbols_in_scope.at(scope), s)
				           == m_symbols_in_scope.at(scope).end()) {
					m_symbols_in_scope.at(scope).push_back(s);
				}
				return s;
			}
		}
		auto parent = parent_of(cur);
		if (not parent.has_value()) return std::nullopt;
		cur = parent.value();
	}
}

ScopeIndex SymbolTable::make_child_scope(ScopeIndex s) {
	Scope child {
		.parent = s,
		.function = function_of(s),
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

std::vector<Guid> SymbolTable::all_symbols(Namespace space) const {
	std::vector<Guid> syms {};
	for (auto i = 0ul; i < m_symbols.size(); i++) {
		const auto guid = Guid {.index = static_cast<Guid::index_type>(i)};
		const auto ns = namespace_of(guid);
		if (ns == space)
			syms.push_back(Guid {.index = static_cast<Guid::index_type>(i)});
	}
	return syms;
}

std::vector<Guid> SymbolTable::symbols_with_name(
	StrID name, Namespace space
) const {
	const std::vector<Guid> syms = all_symbols(space);
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

std::vector<Guid> SymbolTable::free_symbols_in(
	FunctionIndex func, Namespace space
) const {
	std::vector<Guid> free_symbols {};
	auto scopes = function_scopes(func);
	for (auto scope : scopes) {
		auto symbols = symbols_used_in(scope, space);
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

std::vector<Guid> SymbolTable::symbols_used_in(
	ScopeIndex scope, Namespace space
) const {
	std::vector<Guid> syms {};
	for (auto sym : m_symbols_in_scope.at(scope)) {
		const auto ns = namespace_of(sym);
		if (ns == space) syms.push_back(sym);
	}
	return syms;
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
	ScopeIndex s, Namespace space
) const {
	std::unordered_set<Guid> reachable_symbols {};
	auto current_symbols = symbols_declared_in(s);
	for (auto y : current_symbols) {
		const auto ns = namespace_of(y);
		if (ns == space) reachable_symbols.insert(y);
	}
	auto p = parent_of(s);
	if (p.has_value()) {
		reachable_symbols.merge(symbols_reachable_from(p.value(), space));
	}
	return reachable_symbols;
}

std::ostream& operator<<(std::ostream& st, const SymbolTable& symtab) {
	st << '{' << '\n';
	st << "\t";
	st << "all symbols" << '\n';
	for (const auto& [i, symbol] :
	     std::ranges::views::enumerate(symtab.m_symbols)) {
		st << "\t";
		st << i << ' ' << '{';
		st << " name = " << symbol.name.index;
		st << ';' << ' ';
		st << "decl = " << symbol.declaration_scope.index;
		st << ' ' << '}';
		st << '\n';
	}
	st << "\t";
	st << "symbols declared/used in each scope" << '\n';
	for (const auto& [scope, symbols] : symtab.m_symbols_in_scope) {
		st << "\t";
		st << scope.index;
		st << '[' << ' ';
		for (auto symbol : symbols) {
			st << symbol.index << ' ';
		}
		st << ']';
		st << '\n';
	}
	st << "\t";
	st << "associated function body index with a function index" << '\n';
	for (const auto& [body, func] : symtab.m_body_function) {
		st << "\t";
		st << body.index;
		st << ' ' << '=' << ' ';
		st << func.index;
		st << '\n';
	}
	st << "\t";
	st << "all scopes" << '\n';
	for (const auto& [i, scope] :
	     std::ranges::views::enumerate(symtab.m_scopes)) {
		st << "\t";
		st << i << ' ' << '{';
		if (scope.function.has_value()) {
			st << " function = " << scope.function.value().index;
		}
		if (scope.parent.has_value()) {
			st << "; parent = ";
			st << scope.parent.value().index;
		}
		st << '}';
		st << '\n';
	}
	st << '}' << '\n';
	return st;
}

Namespace SymbolTable::namespace_of(Guid guid) const {
	return resolve(guid).value().space;
}
