#ifndef FALA_LEXER_H
#define FALA_LEXER_H

#include <stdio.h>

#include "ast.h"
#include "ring.h"

// necessary for lval of GNU bison rule values
#define YYSTYPE union TokenValue

typedef struct Lexer {
	FILE *fd;
	Ring ring;
} Lexer;

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

typedef void *LEXER;

LEXER lexer_init_from_file(FILE *fd);
void lexer_deinit(LEXER lexer);
int lexer_lex(union TokenValue *lval, Location *location, LEXER scanner);

bool is_interactive(LEXER scanner);

#endif
