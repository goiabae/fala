#ifndef FALA_STR_POOL_H
#define FALA_STR_POOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct StrID {
	size_t idx;
} StrID;

typedef struct StringPool* STR_POOL;

StrID str_pool_intern(STR_POOL pool, char* str);
const char* str_pool_find(STR_POOL pool, StrID id);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

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

#endif
