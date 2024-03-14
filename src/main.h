#ifndef FALA_MAIN_H
#define FALA_MAIN_H

#include <stddef.h>

typedef struct VarTable {
	size_t len;
	size_t cap;
	const char** names;
	int* values;
} VarTable;

void yyerror(void* var_table, char* s);

VarTable* var_table_init(void);
void var_table_deinit(VarTable* tab);
int var_table_insert(VarTable* tab, const char* name, int value);
int var_table_get(VarTable* tab, const char* name);

#endif
