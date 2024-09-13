#ifndef FALA_LEXER_HPP
#define FALA_LEXER_HPP

#include "ast.hpp"
#include "file.hpp"
#include "ring.h"

// necessary for lval of GNU bison rule values
#define YYSTYPE union TokenValue

union TokenValue {
	int num;
	char *str;
	char character;
	NodeIndex node;
};

struct Lexer {
	File *file;
	Ring ring;

	Lexer(File &_file);
	~Lexer();

	Location *loc;
	union TokenValue *value;

	int lex();

	void ensure();
	char peek();
	char advance();
	bool match(char c);
};

int lexer_lex(union TokenValue *lval, Location *location, Lexer *scanner);

bool is_interactive(Lexer *scanner);

#endif
