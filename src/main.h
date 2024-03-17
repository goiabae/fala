#ifndef FALA_MAIN_H
#define FALA_MAIN_H

#include <stddef.h>

#include "ast.h"

typedef int Number;
typedef char* String;

void yyerror(void* scanner, void* ast, void* syms, char* err_msg);

size_t sym_table_insert(SymbolTable* tab, String str);

#endif
