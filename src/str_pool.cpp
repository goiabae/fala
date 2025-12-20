#include "str_pool.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

constexpr size_t default_pool_size = 256;

StringPool::StringPool()
: m_len(0), m_cap(default_pool_size), m_arr(new char*[default_pool_size]) {}

StringPool::~StringPool() { delete[] m_arr; }

StrID StringPool::intern(const char* str) {
	for (auto i = 0u; i < m_len; i++)
		if (std::strcmp(m_arr[i], str) == 0) return StrID {i};

	auto i = (unsigned int)m_len++;
	auto len = strlen(str);
	m_arr[i] = new char[len + 1];
	std::memcpy(m_arr[i], str, sizeof(char) * (len + 1));
	assert(
		m_len <= m_cap && "String pool is already full. Can't intern more strings."
	);
	return StrID {i};
}

const char* StringPool::find(StrID id) const {
	assert((unsigned int)id < m_cap);
	return m_arr[(unsigned int)id];
}
