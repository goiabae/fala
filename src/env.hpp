#ifndef FALA_ENV_HPP
#define FALA_ENV_HPP

#include <cassert>
#include <map>
#include <vector>

#include "ast.hpp"
#include "str_pool.h"

using std::vector;

// analogous to a symbol table
template<typename T>
struct Env {
	struct ScopeID {
		int idx;
		bool operator<(const ScopeID other) const { return idx < other.idx; }
	};

	// pointers returned should not be stored
	T* insert(ScopeID scope_id, StrID str_id, T value);
	T* insert(ScopeID scope_id, StrID str_id);
	T* find(ScopeID scope_id, StrID str_id);

	struct Id {
		int idx;
		Id() : idx {-1} {}
		Id(int idx) : idx {idx} {}
	};

	ScopeID create_child_scope(ScopeID parent_id) {
		ScopeID child_id {scope_count++};
		parent_scope[child_id] = parent_id;
		return child_id;
	}

	ScopeID root_scope_id {0};

 private:
	// this effectively constructs a reversed tree with child nodes pointing to
	// parent nodes. the root node has a -1 previous back pointer
	vector<T> entries;
	vector<Id> previous_entries;
	vector<StrID> name;

	std::map<NodeIndex, ScopeID> node_to_scope; // This shouldn't be here
	std::map<ScopeID, ScopeID> parent_scope;
	std::map<ScopeID, Id> scope_last_entry;

	// lastly added index so far
	Id last_entry_;
	int scope_count = 1; // initial root scope counts

	Id find_last_entry(ScopeID scope_id) {
		auto has_parent = parent_scope.contains(scope_id);
		auto scope_not_empty = scope_last_entry.contains(scope_id);

		if (scope_not_empty) {
			return scope_last_entry[scope_id];
		} else if (has_parent) {
			auto parent_id = parent_scope[scope_id];
			return find_last_entry(parent_id);
		} else {
			return {-1};
		}
	}
};

template<typename T>
T* Env<T>::insert(ScopeID scope_id, StrID str_id, T value) {
	Id previous_entry = find_last_entry(scope_id);
	previous_entries.push_back(previous_entry);

	entries.push_back(value);
	name.push_back(str_id);

	const auto last = entries.size() - 1;
	scope_last_entry[scope_id] = {(int)last};
	return &entries[last];
}

template<typename T>
T* Env<T>::insert(ScopeID scope_id, StrID str_id) {
	return insert(scope_id, str_id, {});
}

template<typename T>
T* Env<T>::find(ScopeID scope_id, StrID str_id) {
	Id cur = find_last_entry(scope_id);

	while (name[cur.idx].idx != str_id.idx) {
		if (cur.idx < 0) return nullptr;
		if (cur.idx > (int)(previous_entries.size() - 1)) assert(false);
		cur = previous_entries[cur.idx];
	}

	return &entries[cur.idx];
}

#endif
