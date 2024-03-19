#ifndef FALA_ENV_H
#define FALA_ENV_H

#include <stdbool.h>
#include <stddef.h>

#define ENV_NOT_FOUND -1

typedef struct Environment {
	size_t len;
	size_t cap;
	size_t scope_count;

	// index into some other structure like a value or register stack
	size_t* indexes;
	size_t* syms;   // symbol table index
	size_t* scopes; // which scope each item belongs to
} Environment;

Environment env_init();
void env_deinit(Environment* env, void (*cb)(void*, size_t), void* cb_data);
size_t env_find(Environment* env, size_t sym_idx, bool* found);
size_t env_get_new(Environment* env, size_t sym_idx);
void env_push(Environment* env);
void env_pop(Environment* env, void (*cb)(void*, size_t), void* cb_data);

#endif
