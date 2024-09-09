#ifndef FALA_ENV_HPP
#define FALA_ENV_HPP

#include <cstddef>
#include <utility>
#include <vector>

#include "str_pool.h"

using std::pair;
using std::vector;

// analogous to a symbol table
// TODO: make this persistent and put into a separate semantic analysis step
template<typename T>
struct Env {
	T* insert(StrID str_id, T value);
	T* insert(StrID str_id);
	T* find(StrID str_id);

	// lexical scope
	struct Scope;
	Scope make_scope();

	Env() { root_scope = new Scope(this); };
	~Env() { delete root_scope; };

 private:
	vector<vector<pair<size_t, T>>> m_vec;
	Scope* root_scope;
};

// a resource that behaves like a smart pointer, but can be stack allocated
// move-only
template<typename T>
struct Env<T>::Scope {
	Scope(Env<T>* env) : m_env {env} { m_env->m_vec.push_back({}); }
	~Scope() {
		if (m_owned) {
			// FIXME
			// for (auto& v : m_env->m_vec.back()) v.second.deinit();
			m_env->m_vec.pop_back();
		}
	}
	Scope(const Scope& other) = delete;
	Scope& operator=(const Scope& other) = delete;
	Scope(Scope&& other) {
		m_env = other.m_env;
		other.m_owned = false;
		m_owned = true;
	}
	Scope& operator=(Scope&& other) {
		m_env = other.m_env;
		other.m_owned = false;
		m_owned = true;
		return *this;
	}

 private:
	Env<T>* m_env;
	bool m_owned {true};
};

template<typename T>
typename Env<T>::Scope Env<T>::make_scope() {
	return Scope(this);
}

template<typename T>
T* Env<T>::insert(StrID str_id, T value) {
	m_vec.back().push_back({str_id.idx, value});
	T& op = m_vec.back().back().second;
	return &op;
}

template<typename T>
T* Env<T>::insert(StrID str_id) {
	m_vec.back().push_back({str_id.idx, {}});
	T& op = m_vec.back().back().second;
	return &op;
}

template<typename T>
T* Env<T>::find(StrID str_id) {
	for (size_t i = m_vec.size(); i-- > 0;)
		for (size_t j = m_vec[i].size(); j-- > 0;)
			if (m_vec[i][j].first == str_id.idx) return &m_vec[i][j].second;
	return nullptr;
}

#endif
