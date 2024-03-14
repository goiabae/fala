#include <stdio.h>

#include "parser.h"
#include "lexer.h"

void yyerror(void* var_table, char *s) {
	(void)var_table;
	fprintf(stderr, "%s\n", s);
}

int main(void) {
	yyin = stdin;
	yyparse(NULL);
}
