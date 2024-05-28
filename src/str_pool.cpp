#include "str_pool.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

constexpr size_t default_pool_size = 256;

StringPool::StringPool()
: m_len(0), m_cap(default_pool_size), m_arr(new char*[default_pool_size]) {}

StringPool::~StringPool() { delete[] m_arr; }

StrID StringPool::intern(char* str) {
	for (size_t i = 0; i < m_len; i++) {
		if (std::strcmp(m_arr[i], str) == 0) {
			free(str);
			return StrID {i};
		}
	}
	m_arr[m_len++] = str;
	assert(
		m_len <= m_cap && "String pool is already full. Can't intern more strings."
	);
	return StrID {m_len - 1};
}

const char* StringPool::find(StrID id) const {
	assert(id.idx < m_cap);
	return m_arr[id.idx];
}

extern "C" StrID str_pool_intern(STR_POOL pool, char* str) {
	assert(pool != nullptr);
	return pool->intern(str);
}

extern "C" const char* str_pool_find(STR_POOL pool, StrID id) {
	assert(pool != nullptr);
	return pool->find(id);
}
