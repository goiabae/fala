#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FALA_WITH_READLINE
#	include <readline/history.h>
#	include <readline/readline.h>
#endif

#include "lexer.h"
#include "parser.h"

#define FALA_RING_IMPL
extern "C" {
#include "ring.h"
}

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
	}
	assert(false);
}

// maps KW_ enum values to Bison's token enum value
int keyword_to_bison(size_t i) {
	switch (i) {
		case KW_DO: return DO;
		case KW_END: return END;
		case KW_IF: return IF;
		case KW_THEN: return THEN;
		case KW_ELSE: return ELSE;
		case KW_WHEN: return WHEN;
		case KW_FOR: return FOR;
		case KW_FROM: return FROM;
		case KW_TO: return TO;
		case KW_STEP: return STEP;
		case KW_WHILE: return WHILE;
		case KW_BREAK: return BREAK;
		case KW_CONTINUE: return CONTINUE;
		case KW_VAR: return VAR;
		case KW_LET: return LET;
		case KW_IN: return IN;
		case KW_FUN: return FUN;
		case KW_OR: return OR;
		case KW_AND: return AND;
		case KW_NOT: return NOT;
		case KW_NIL: return NIL;
		case KW_TRUE: return TRUE;
	}
	assert(false);
}

#ifndef FALA_WITH_READLINE
static size_t read_line(char* buf, size_t count, FILE* fd) {
	size_t i = 0;
	char c = '\0';
	while (i < count && c != EOF)
		buf[i++] = c = ((c = (char)getc(fd)) == '\n') ? EOF : c;
	return i;
}
#endif

// ensures that there are elements to read in the ring buffer
static void ensure(Lexer* lexer) {
	if (lexer->ring.len == 0) {
		char* buf;
		size_t read = 0;

#ifdef FALA_WITH_READLINE
		if (lexer->file->get_descriptor() == stdin) {
			buf = readline("fala> ");
			if (!buf) return;
			add_history(buf);
			read = strlen(buf);
		} else {
			buf = (char*)malloc(sizeof(char) * lexer->ring.cap);
			read = fread(
				buf, sizeof(char), lexer->ring.cap, lexer->file->get_descriptor()
			);
		}
#else
		buf = (char*)malloc(sizeof(char) * lexer->ring.cap);
		if (lexer->file->get_descriptor() == stdin) {
			printf("fala> ");
			read = read_line(buf, lexer->ring.cap, lexer->file->get_descriptor());
		} else
			read = fread(
				buf, sizeof(char), lexer->ring.cap, lexer->file->get_descriptor()
			);
#endif

		if (read > 0) ring_write_many(&lexer->ring, buf, read);
#ifdef FALA_WITH_READLINE
		if (lexer->file->get_descriptor() == stdin) ring_write(&lexer->ring, '\n');
#endif
		free(buf);
	}
}

static char peek(Lexer* lexer) {
	ensure(lexer);
	return ring_peek(&lexer->ring);
}

static char advance(Lexer* lexer, Location* loc) {
	ensure(lexer);
	char c = ring_read(&lexer->ring);
	if (c == '\n') {
		loc->last_line++;
		loc->last_column = 0;
	} else {
		loc->last_column++;
	}
	return c;
}

// advances a character if it matches, otherwise do nothing
static bool match(Lexer* lexer, Location* loc, char c) {
	char ch = ring_peek(&lexer->ring);
	if (ch == c) advance(lexer, loc);
	return ch == c;
}

static bool is_valid_id_char(char c) { return isalnum(c) || c == '_'; }

static char* string_dup(char* str) {
	const size_t len = strlen(str);
	char* copy = (char*)malloc(sizeof(char) * (len + 1));
	for (size_t i = 0; i < len; i++) copy[i] = str[i];
	copy[len] = '\0';
	return copy;
}

extern "C" int lexer_lex(union TokenValue* value, Location* loc, LEXER lexer) {
	loc->first_line = loc->last_line;
	loc->first_column = loc->last_column;

	char c = advance(lexer, loc);
	if (c < 0) return YYEOF;
	switch (c) {
		case '(': return PAREN_OPEN;
		case ')': return PAREN_CLOSE;
		case '[': return BRACKET_OPEN;
		case ']': return BRACKET_CLOSE;
		case ';': return SEMICOL;
		case ',': return COMMA;
		case '=': return match(lexer, loc, '=') ? EQ_EQ : EQ;
		case '>': return match(lexer, loc, '=') ? GREATER_EQ : GREATER;
		case '<': return match(lexer, loc, '=') ? LESSER_EQ : LESSER;
		case '+': return PLUS;
		case '-': return MINUS;
		case '*': return ASTER;
		case '/': return SLASH;
		case '.': return DOT;
		case '%':
			return PERCT;

			// just skip whitespace
		case ' ':
		case '\t': return lexer_lex(value, loc, lexer);
		case '\n': {
#ifdef FALA_WITH_READLINE
			if (is_interactive(lexer))
				return EOF;
			else
#endif
				return lexer_lex(value, loc, lexer);
		}

		case '#': {
			while ((c = peek(lexer)) != '\n') advance(lexer, loc);
			return lexer_lex(value, loc, lexer);
		}

		case '"': {
			char buf[256];
			size_t len = 0;
			while ((c = advance(lexer, loc)) != '"') {
				if (c == '\\') {
					if (match(lexer, loc, 'n'))
						buf[len++] = '\n';
					else if (match(lexer, loc, 't'))
						buf[len++] = '\t';
					else if (match(lexer, loc, 'r'))
						buf[len++] = '\r';
				} else {
					buf[len++] = c;
				}
			}
			buf[len] = '\0';
			value->str = string_dup(buf);
			return STRING;
		}
		default: {
			if (isdigit(c)) {
				long num = c - '0';
				while (isdigit(c = peek(lexer))) {
					num = num * 10 + (c - '0');
					advance(lexer, loc);
				}
				value->num = (Number)num;
				return NUMBER;
			} else if (isalpha(c) || c == '_') {
				char buf[256] {};
				buf[0] = c;
				size_t len = 1;
				while (is_valid_id_char(c = peek(lexer))) {
					buf[len++] = c;
					advance(lexer, loc);
				}
				buf[len] = '\0';

				// could be a reserved keyword
				for (size_t i = 0; i < KW_COUNT; i++)
					if (strcmp(buf, keyword_repr(i)) == 0) return keyword_to_bison(i);

				value->str = string_dup(buf);
				return ID;
			}
			assert(false && "LEX_ERR: Unrecognized character");
		}
	};
	assert(false && "unreachable");
}

bool is_interactive(LEXER lexer) {
	return lexer->file->get_descriptor() == stdin;
}

Lexer::Lexer(File& _file) : file(&_file), ring(ring_init()) {}

Lexer::~Lexer() { ring_deinit(&ring); }
