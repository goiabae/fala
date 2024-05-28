#ifndef FALA_LEXER_H
#define FALA_LEXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>

#include "ast.h"
#include "file.hpp"
#include "ring.h"

// necessary for lval of GNU bison rule values
#define YYSTYPE union TokenValue

union TokenValue {
	int num;
	char *str;
	Node node;
};

typedef struct Lexer *LEXER;

int lexer_lex(union TokenValue *lval, Location *location, LEXER scanner);

bool is_interactive(LEXER scanner);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
struct Lexer {
	File *file;
	Ring ring;

	Lexer(File &_file);
	~Lexer();
};
#endif

#endif
