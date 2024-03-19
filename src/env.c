#include "env.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

Environment env_init() {
	Environment env;
	env.len = 0;
	env.cap = 32;
	env.scope_count = 0;
	env.indexes = malloc(sizeof(size_t) * env.cap);
	env.syms = malloc(sizeof(size_t) * env.cap);
	env.scopes = malloc(sizeof(size_t) * env.cap);
	return env;
}

void env_deinit(Environment* env, void (*cb)(void*, size_t), void* cb_data) {
	for (size_t i = 0; i < env->len; i++) cb(cb_data, i);
	free(env->indexes);
	free(env->scopes);
	free(env->syms);
}

void env_push(Environment* env) { env->scope_count++; }

// go from top to bottom of the stack and remove vars belonging to the current
// scope
void env_pop(Environment* env, void (*cb)(void*, size_t), void* cb_data) {
	assert(env->scope_count > 0 && "Environment was empty");
	for (size_t i = env->len; i-- > 0; env->len--) {
		if (env->scopes[i] != (env->scope_count - 1)) break;
		cb(cb_data, i);
	}
	env->scope_count--;
}

// search for sym in env. if not found return NULL
size_t env_find(Environment* env, size_t sym_idx, bool* found) {
	for (size_t i = env->len; i > 0;) {
		--i;
		if (env->syms[i] == sym_idx) return i;
	}
	*found = false;
	return 0;
}

// add sym to environment and return a new value cell
size_t env_get_new(Environment* env, size_t sym_idx) {
	env->scopes[env->len] = env->scope_count - 1;
	env->syms[env->len] = sym_idx;
	env->indexes[env->len] = env->len;
	return env->len++;
}
