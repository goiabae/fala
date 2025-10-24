#ifndef FALA_LEXER_HPP
#define FALA_LEXER_HPP

#include "ast.hpp"
#include "reader.hpp"
#include "ring.h"

struct Lexer {
	Reader *file;
	Ring ring;

	Lexer(Reader *file);
	~Lexer();

	Location *loc;
	union TokenValue *value;

	int lex();

	void ensure();
	char peek();
	char advance();
	bool match(char c);

	std::vector<std::string> get_lines() { return lines; }

 private:
	std::string current_line {};
	std::vector<std::string> lines {};
};

int lexer_lex(union TokenValue *lval, Location *location, Lexer *scanner);

bool is_interactive(Lexer *scanner);

#endif
