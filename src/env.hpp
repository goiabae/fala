#ifndef FALA_ENV_HPP
#define FALA_ENV_HPP

#include <vector>

#include "str_pool.h"

using std::vector;

// analogous to a symbol table
template<typename T>
struct Env {
	// pointers returned should not be stored
	T* insert(StrID str_id, T value);
	T* insert(StrID str_id);
	T* find(StrID str_id);

	struct Id {
		int idx;
		Id() : idx {-1} {}
	};

	struct Scope {
		Env* env_handle;
	};

	Scope make_scope();

 private:
	// this effectively constructs a reversed tree with child nodes pointing to
	// parent nodes. the root node has a -1 previous back pointer
	vector<T> entries;
	vector<Id> previous_entries;
	vector<StrID> name;

	// lastly added index so far
	Id last_entry;
};

template<typename T>
typename Env<T>::Scope Env<T>::make_scope() {
	return Scope {this};
}

template<typename T>
T* Env<T>::insert(StrID str_id, T value) {
	entries.push_back(value);
	previous_entries.push_back(last_entry);
	name.push_back(str_id);

	const auto last = entries.size() - 1;
	last_entry.idx = (int)last;
	return &entries[last];
}

template<typename T>
T* Env<T>::insert(StrID str_id) {
	return insert(str_id, {});
}

template<typename T>
T* Env<T>::find(StrID str_id) {
	Id cur = last_entry;

	while (name[cur.idx].idx != str_id.idx) {
		if (cur.idx < 0) return nullptr;
		cur = previous_entries[cur.idx];
	}

	return &entries[cur.idx];
}

#endif
