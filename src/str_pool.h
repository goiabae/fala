#ifndef FALA_STR_POOL_HPP
#define FALA_STR_POOL_HPP

#include <cstddef>

struct StrID {
	size_t idx;
};

// Interns strings and returns lightweight handles.
// Handy for avoiding error-prone handling of owned strings
struct StringPool {
 public:
	StringPool();
	~StringPool();

	StringPool(const StringPool& other) = delete;
	StringPool& operator=(const StringPool& other) = delete;

	// str is already interned, str gets freed
	StrID intern(char* str);
	const char* find(StrID id) const;

 private:
	size_t m_len;
	size_t m_cap;
	char** m_arr;
};

#endif
