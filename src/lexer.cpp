#include "lexer.hpp"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <stdexcept>
#include <string>

#include "codepoint.hpp"
#include "codepoint_decoder.hpp"
#include "parser.hpp"
#include "token_value.hpp"

#define FALA_RING_IMPL
#include "ring.h"

using std::string;

using tk = yy::parser::token_type;

static const char* keyword_repr(size_t i);
static int keyword_to_bison(size_t i);

// these are sequential indexes, which I can't guarantee Bison token enums are
enum {
	KW_DO,
	KW_END,
	KW_IF,
	KW_THEN,
	KW_ELSE,
	KW_WHEN,
	KW_FOR,
	KW_FROM,
	KW_TO,
	KW_STEP,
	KW_WHILE,
	KW_BREAK,
	KW_CONTINUE,
	KW_VAR,
	KW_VAL,
	KW_OUT,
	KW_LET,
	KW_IN,
	KW_FUN,
	KW_OR,
	KW_AND,
	KW_NOT,
	KW_NIL,
	KW_TRUE,
	KW_FALSE,
	KW_INT,
	KW_UINT,
	KW_BOOL,
	KW_AS,
	KW_TYPE,
	KW_EFFECT,
	KW_CATCH,
	KW_RESUME,
	KW_UNION,
	KW_MATCH,
	KW_WITH,
	KW_COUNT,
};

const char* keyword_repr(size_t i) {
	switch (i) {
		case KW_DO: return "do";
		case KW_END: return "end";
		case KW_IF: return "if";
		case KW_THEN: return "then";
		case KW_ELSE: return "else";
		case KW_WHEN: return "when";
		case KW_FOR: return "for";
		case KW_FROM: return "from";
		case KW_TO: return "to";
		case KW_STEP: return "step";
		case KW_WHILE: return "while";
		case KW_BREAK: return "break";
		case KW_CONTINUE: return "continue";
		case KW_VAR: return "var";
		case KW_VAL: return "val";
		case KW_OUT: return "out";
		case KW_LET: return "let";
		case KW_IN: return "in";
		case KW_FUN: return "fun";
		case KW_OR: return "or";
		case KW_AND: return "and";
		case KW_NOT: return "not";
		case KW_NIL: return "nil";
		case KW_TRUE: return "true";
		case KW_FALSE: return "false";
		case KW_INT: return "int";
		case KW_UINT: return "uint";
		case KW_BOOL: return "bool";
		case KW_AS: return "as";
		case KW_TYPE: return "type";
		case KW_EFFECT: return "effect";
		case KW_CATCH: return "catch";
		case KW_RESUME: return "resume";
		case KW_UNION: return "union";
		case KW_MATCH: return "match";
		case KW_WITH: return "with";
	}
	assert(false);
}

// maps KW_ enum values to Bison's token enum value
int keyword_to_bison(size_t i) {
	switch (i) {
		case KW_DO: return tk::DO;
		case KW_END: return tk::END;
		case KW_IF: return tk::IF;
		case KW_THEN: return tk::THEN;
		case KW_ELSE: return tk::ELSE;
		case KW_WHEN: return tk::WHEN;
		case KW_FOR: return tk::FOR;
		case KW_FROM: return tk::FROM;
		case KW_TO: return tk::TO;
		case KW_STEP: return tk::STEP;
		case KW_WHILE: return tk::WHILE;
		case KW_BREAK: return tk::BREAK;
		case KW_CONTINUE: return tk::CONTINUE;
		case KW_VAR: return tk::VAR;
		case KW_LET: return tk::LET;
		case KW_IN: return tk::IN;
		case KW_FUN: return tk::FUN;
		case KW_OR: return tk::OR;
		case KW_AND: return tk::AND;
		case KW_NOT: return tk::NOT;
		case KW_NIL: return tk::NIL;
		case KW_TRUE: return tk::TRUE;
		case KW_FALSE: return tk::FALSE;
		case KW_INT: return tk::INT;
		case KW_UINT: return tk::UINT;
		case KW_BOOL: return tk::BOOL;
		case KW_AS: return tk::AS;
		case KW_VAL: return tk::VAL;
		case KW_OUT: return tk::OUT;
		case KW_TYPE: return tk::TYPE;
		case KW_EFFECT: return tk::EFFECT;
		case KW_CATCH: return tk::CATCH;
		case KW_RESUME: return tk::RESUME;
		case KW_UNION: return tk::UNION;
		case KW_MATCH: return tk::MATCH;
		case KW_WITH: return tk::WITH;
	}
	assert(false);
}

// ensures that there are elements to read in the ring buffer
void Lexer::ensure() {
	if (ring.len > 0) return;

	char* buf = new char[ring.cap];
	size_t read = file->read_at_most(buf, ring.cap);

	if (read > 0) ring_write_many(&ring, buf, read);

	for (size_t i = 0; i < read; i++) {
		if (buf[i] == '\n') {
			lines.push_back(current_line);
			current_line = "";
		} else if (buf[i] == '\t') {
			current_line += "  ";
		} else {
			current_line += buf[i];
		}
	}

	delete[] buf;
}

Codepoint Lexer::peek() {
	ensure();
	if (not peaked.has_value()) {
		while (true) {
			if (auto p = decoder.push((std::uint8_t)ring_read(&ring))) {
				peaked = *p;
				return *p;
			}
		}
	} else {
		return peaked.value();
	}
}

Codepoint Lexer::advance() {
	if (peaked.has_value()) {
		auto c = peaked.value();
		peaked = {};
		return c;
	}
	ensure();
	while (true) {
		auto c = ring_read(&ring);
		if (c == EOT) return EOT;
		if (auto p = decoder.push((std::uint8_t)c)) {
			loc->end.byte_offset += static_cast<int>(size(*p));
			if (*p == '\n') {
				loc->end.line++;
				loc->end.column = 0;
			} else if (*p == '\t') {
				loc->end.column += 2;
			} else {
				loc->end.column++;
			}
			return *p;
		}
	}
}

// advances a character if it matches, otherwise do nothing
bool Lexer::match(char c) {
	auto ch = peek();
	auto x = size(ch) == 1 and (std::uint8_t) ch == c;
	if (x) advance();
	return x;
}

static bool is_valid_id_char(Codepoint codepoint) {
	if (is_digit(codepoint)) return true;
	if (is_ascii_alpha(codepoint)) return true;
	if (codepoint == '_') return true;
	std::string portuguese_characters_not_in_ascii {"áàâãéêiíóôúçÁÀÂÃÉÊIÍÓÔÚÇ"};
	CodepointDecoder decoder {};
	std::vector<Codepoint> codepoints;
	for (auto c : portuguese_characters_not_in_ascii)
		if (auto codepoint = decoder.push((std::uint8_t)c))
			codepoints.push_back(*codepoint);
	for (auto cp : codepoints)
		if (codepoint == cp) return true;
	return false;
}

static bool is_valid_starting_id_char(Codepoint codepoint) {
	if (is_ascii_alpha(codepoint)) return true;
	if (codepoint == '_') return true;
	std::string portuguese_characters_not_in_ascii {"áàâãéêiíóôúçÁÀÂÃÉÊIÍÓÔÚÇ"};
	CodepointDecoder decoder {};
	std::vector<Codepoint> codepoints;
	for (auto c : portuguese_characters_not_in_ascii)
		if (auto codepoint = decoder.push((std::uint8_t)c))
			codepoints.push_back(*codepoint);
	for (auto cp : codepoints)
		if (codepoint == cp) return true;
	return false;
}

static char* string_dup(const char* str) {
	const size_t len = strlen(str);
	char* copy = new char[len + 1];
	for (size_t i = 0; i < len; i++) copy[i] = str[i];
	copy[len] = '\0';
	return copy;
}

int lexer_lex(union TokenValue* value, Location* loc, Lexer* lexer) {
	lexer->loc = loc;
	lexer->value = value;
	return lexer->lex();
}

int Lexer::lex() {
	loc->begin.byte_offset = loc->end.byte_offset;
	loc->begin.line = loc->end.line;
	loc->begin.column = loc->end.column;

	if (not has_sent_start) {
		has_sent_start = true;
		if (is_repl)
			return tk::START_REPL;
		else
			return tk::START_MODULE;
	}

	auto c = advance();
	if (c == EOT) {
		lines.push_back(current_line);
		return tk::YYEOF;
	}
	switch (c) {
		case '(': return tk::PAREN_OPEN;
		case ')': return tk::PAREN_CLOSE;
		case '[': return tk::BRACKET_OPEN;
		case ']': return tk::BRACKET_CLOSE;
		case '{': return tk::CURLY_OPEN;
		case '}': return tk::CURLY_CLOSE;
		case '!': return tk::BANG;
		case ';': return tk::SEMICOL;
		case ':': return tk::COLON;
		case ',': return tk::COMMA;
		case '=': return match('=') ? tk::EQ_EQ : tk::EQ;
		case '>': return match('=') ? tk::GREATER_EQ : tk::GREATER;
		case '<': return match('=') ? tk::LESSER_EQ : tk::LESSER;
		case '+': return tk::PLUS;
		case '-': {
			if (match('-')) {
				while ((c = peek()) != '\n') advance();
				return lex();
			} else {
				return tk::MINUS;
			}
		}
		case '*': return tk::ASTER;
		case '/': return tk::SLASH;
		case '.': return match('.') ? tk::DOTDOT : tk::DOT;
		case '%': return tk::PERCT;
		case '#':
			return tk::POUND;

			// just skip whitespace
		case ' ':
		case '\t': return lex();
		case '\n': {
			if (file->is_interactive())
				return tk::YYEOF;
			else
				return tk::NEWLINE;
		}

		case '"': {
			string str;
			while ((c = advance()) != '"') {
				if (c == '\\') {
					if (match('n'))
						str += '\n';
					else if (match('t'))
						str += '\t';
					else if (match('r'))
						str += '\r';
					else
						assert(false && "LEXER: Unknown escape sequence");
				} else {
					str += to_bytestring(c);
				}
			}
			value->str = string_dup(str.c_str());
			return tk::STRING;
		}
		case '\'': {
			auto character = advance();
			if (character == '\\') {
				if (match('n'))
					value->character = '\n';
				else if (match('t'))
					value->character = '\t';
				else if (match('r'))
					value->character = '\r';
				else
					assert(false && "LEXER: Unknown espace sequence");
			} else {
				value->character = to_ascii(character);
			}

			if (!match('\'')) assert(false && "LEXER: Invalid character literal");
			return tk::CHAR;
		}
		default: {
			if (is_digit(c)) {
				long num = c - '0';
				while (is_digit(c = peek())) num = num * 10 + (advance() - '0');
				value->num = (Number)num;
				return tk::NUMBER;
			} else if (is_valid_starting_id_char(c)) {
				string str = to_bytestring(c);
				while (is_valid_id_char(peek())) str += to_bytestring(advance());

				// could be a reserved keyword
				for (size_t i = 0; i < KW_COUNT; i++)
					if (str == keyword_repr(i)) return keyword_to_bison(i);

				value->str = string_dup(str.c_str());
				return tk::ID;
			}
			assert(false && "LEX_ERR: Unrecognized character");
		}
	};
	assert(false && "unreachable");
}

bool is_interactive(Lexer* lexer) { return lexer->file->is_interactive(); }

Lexer::Lexer(Reader* file, bool is_repl)
: file {file},
	ring {ring_init()},
	loc {nullptr},
	value {nullptr},
	is_repl {is_repl} {}

Lexer::~Lexer() { ring_deinit(&ring); }
