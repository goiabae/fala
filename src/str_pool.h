#ifndef FALA_STR_POOL_HPP
#define FALA_STR_POOL_HPP

#include <cstddef>
#include <ostream>

#include "indexes.hpp"

// Interns strings and returns lightweight handles.
// Handy for avoiding error-prone handling of owned strings
struct StringPool {
 public:
	StringPool();
	~StringPool();

	StringPool(const StringPool& other) = delete;
	StringPool& operator=(const StringPool& other) = delete;

	// str is already interned, str gets freed
	StrID intern(const char* str);
	const char* find(StrID id) const;

	friend std::ostream& operator<<(std::ostream&, const StringPool&);

 private:
	size_t m_len;
	size_t m_cap;
	char** m_arr;
};

std::ostream& operator<<(std::ostream&, const StringPool&);

#endif
