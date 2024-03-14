#include "main.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

VarTable* var_table_init(void) {
	VarTable* tab = malloc(sizeof(VarTable));
	tab->len = 0;
	tab->cap = 100;

	tab->names = malloc(sizeof(char*) * tab->cap);
	memset(tab->names, 0, sizeof(char*) * tab->cap);

	tab->values = malloc(sizeof(int) * tab->cap);
	memset(tab->values, 0, sizeof(int) * tab->cap);

	return tab;
}

void var_table_deinit(VarTable* tab) {
	free(tab->names);
	free(tab->values);
	free(tab);
}

int var_table_insert(VarTable* tab, const char* name, int value) {
	for (size_t i = 0; i < tab->len; i++)
		if (strcmp(tab->names[i], name) == 0) return (tab->values[i] = value);

	// if name not in table
	tab->names[tab->len] = name;
	tab->values[tab->len] = value;
	tab->len++;
	return value;
}

int var_table_get(VarTable* tab, const char* name) {
	for (size_t i = 0; i < tab->len; i++)
		if (strcmp(tab->names[i], name) == 0) return tab->values[i];
	assert(false); // FIXME should propagate error
}

void yyerror(void* var_table, char* s) {
	(void)var_table;
	fprintf(stderr, "%s\n", s);
}

int main(void) {
	VarTable* tab = var_table_init();
	yyin = stdin;
	yyparse((void*)tab);
	var_table_deinit(tab);
}
