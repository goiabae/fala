#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#ifdef FALA_WITH_READLINE
#	include <readline/history.h>
#	include <readline/readline.h>
#endif

#include "lexer.hpp"
#include "parser.hpp"

#define FALA_RING_IMPL
#include "ring.h"

using std::string;

using tk = yy::parser::token_type;

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
	KW_LET,
	KW_IN,
	KW_FUN,
	KW_OR,
	KW_AND,
	KW_NOT,
	KW_NIL,
	KW_TRUE,
	KW_INT,
	KW_UINT,
	KW_BOOL,
	KW_AS,
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
		case KW_LET: return "let";
		case KW_IN: return "in";
		case KW_FUN: return "fun";
		case KW_OR: return "or";
		case KW_AND: return "and";
		case KW_NOT: return "not";
		case KW_NIL: return "nil";
		case KW_TRUE: return "true";
		case KW_INT: return "int";
		case KW_UINT: return "uint";
		case KW_BOOL: return "bool";
		case KW_AS: return "as";
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
		case KW_INT: return tk::INT;
		case KW_UINT: return tk::UINT;
		case KW_BOOL: return tk::BOOL;
		case KW_AS: return tk::AS;
	}
	assert(false);
}

static size_t read_line(char* buf, size_t count, FILE* fd) {
	size_t read = 0;
#ifdef FALA_WITH_READLINE
	(void)fd;
	char* line = readline("fala> ");
	if (!line) return 0;
	add_history(line);
	read = strlen(line);
	if (read > count) return 0;
	strncpy(buf, line, read);
	buf[read++] = '\n';
	buf[read] = '\0';
	free(line);
#else
	printf("fala> ");
	char c = '\0';
	while (read < count && c != EOF)
		buf[read++] = c = ((c = (char)getc(fd)) == '\n') ? EOF : c;
#endif
	return read;
}

// ensures that there are elements to read in the ring buffer
void Lexer::ensure() {
	if (ring.len > 0) return;

	char* buf = new char[ring.cap];
	size_t read = (file->get_descriptor() == stdin)
	              ? read_line(buf, ring.cap, file->get_descriptor())
	              : fread(buf, sizeof(char), ring.cap, file->get_descriptor());

	if (read > 0) ring_write_many(&ring, buf, read);
	delete[] buf;
}

char Lexer::peek() {
	ensure();
	return ring_peek(&ring);
}

char Lexer::advance() {
	ensure();
	char c = ring_read(&ring);
	loc->end.byte_offset++;
	if (c == '\n') {
		loc->end.line++;
		loc->end.column = 0;
	} else {
		loc->end.column++;
	}
	return c;
}

// advances a character if it matches, otherwise do nothing
bool Lexer::match(char c) {
	char ch = ring_peek(&ring);
	if (ch == c) advance();
	return ch == c;
}

static bool is_valid_id_char(char c) { return isalnum(c) || c == '_'; }

static char* string_dup(const char* str) {
	const size_t len = strlen(str);
	char* copy = (char*)malloc(sizeof(char) * (len + 1));
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

	char c = advance();
	if (c < 0) return tk::YYEOF;
	switch (c) {
		case '(': return tk::PAREN_OPEN;
		case ')': return tk::PAREN_CLOSE;
		case '[': return tk::BRACKET_OPEN;
		case ']': return tk::BRACKET_CLOSE;
		case ';': return tk::SEMICOL;
		case ':': return tk::COLON;
		case ',': return tk::COMMA;
		case '=': return match('=') ? tk::EQ_EQ : tk::EQ;
		case '>': return match('=') ? tk::GREATER_EQ : tk::GREATER;
		case '<': return match('=') ? tk::LESSER_EQ : tk::LESSER;
		case '+': return tk::PLUS;
		case '-': return tk::MINUS;
		case '*': return tk::ASTER;
		case '/': return tk::SLASH;
		case '.': return tk::DOT;
		case '%':
			return tk::PERCT;

			// just skip whitespace
		case ' ':
		case '\t': return lex();
		case '\n': {
#ifdef FALA_WITH_READLINE
			if (is_interactive(this))
				return EOF;
			else
#endif
				return lex();
		}

		case '#': {
			while ((c = peek()) != '\n') advance();
			return lex();
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
					str += c;
				}
			}
			value->str = string_dup(str.c_str());
			return tk::STRING;
		}
		case '\'': {
			char character = advance();
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
				value->character = character;
			}

			if (!match('\'')) assert(false && "LEXER: Invalid character literal");
			return tk::CHAR;
		}
		default: {
			if (isdigit(c)) {
				long num = c - '0';
				while (isdigit(c = peek())) num = num * 10 + (advance() - '0');
				value->num = (Number)num;
				return tk::NUMBER;
			} else if (isalpha(c) || c == '_') {
				string str {c};
				while (is_valid_id_char(peek())) str += advance();

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

bool is_interactive(Lexer* lexer) {
	return lexer->file->get_descriptor() == stdin;
}

Lexer::Lexer(File& _file) : file(&_file), ring(ring_init()) {}

Lexer::~Lexer() { ring_deinit(&ring); }
