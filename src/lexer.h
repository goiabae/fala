#ifndef FALA_LEXER_H
#define FALA_LEXER_H

#include <stdio.h>

#include "ast.h"

typedef void *LEXER;

LEXER lexer_init_from_file(FILE *fd);
void lexer_deinit(LEXER lexer);

bool is_interactive(LEXER scanner);

typedef struct Location {
	int first_line;
	int first_column;
	int last_line;
	int last_column;
} Location;

union TokenValue {
	int num;
	char *str;
	Node node;
};

// necessary for lval of GNU bison rule values
#define YYSTYPE union TokenValue

int lexer_lex(union TokenValue *lval, Location *location, LEXER scanner);

#endif
