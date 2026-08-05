#ifndef FALA_LEXER_HPP
#define FALA_LEXER_HPP

#include <vector>

#include "location.hpp"
#include "reader.hpp"
#include "ring.h"

struct Lexer {
	Reader* file;
	Ring ring;

	Lexer(Reader* file, bool is_repl);
	~Lexer();

	Location* loc;
	union TokenValue* value;

	int lex();

	void ensure();
	char peek();
	char advance();
	bool match(char c);

	std::vector<std::string> get_lines() { return lines; }

 private:
	bool is_repl;
	bool has_sent_start {false};
	std::string current_line {};
	std::vector<std::string> lines {};
};

int lexer_lex(union TokenValue* lval, Location* location, Lexer* scanner);

bool is_interactive(Lexer* scanner);

#endif
