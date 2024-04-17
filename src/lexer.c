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
#include "ring.h"

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

static const char* keywords[KW_COUNT] = {
	[KW_DO] = "do",     [KW_END] = "end",     [KW_IF] = "if",
	[KW_THEN] = "then", [KW_ELSE] = "else",   [KW_WHEN] = "when",
	[KW_FOR] = "for",   [KW_FROM] = "from",   [KW_TO] = "to",
	[KW_STEP] = "step", [KW_WHILE] = "while", [KW_VAR] = "var",
	[KW_LET] = "let",   [KW_IN] = "in",       [KW_FUN] = "fun",
	[KW_OR] = "or",     [KW_AND] = "and",     [KW_NOT] = "not",
	[KW_NIL] = "nil",   [KW_TRUE] = "true",
};

static const int keyword_parser_map[KW_COUNT] = {
	[KW_DO] = DO,     [KW_END] = END,   [KW_IF] = IF,       [KW_THEN] = THEN,
	[KW_ELSE] = ELSE, [KW_WHEN] = WHEN, [KW_FOR] = FOR,     [KW_FROM] = FROM,
	[KW_TO] = TO,     [KW_STEP] = STEP, [KW_WHILE] = WHILE, [KW_VAR] = VAR,
	[KW_LET] = LET,   [KW_IN] = IN,     [KW_FUN] = FUN,     [KW_OR] = OR,
	[KW_AND] = AND,   [KW_NOT] = NOT,   [KW_NIL] = NIL,     [KW_TRUE] = TRUE,
};

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
		if (lexer->fd == stdin) {
			buf = readline("fala> ");
			if (!buf) return;
			add_history(buf);
			read = strlen(buf);
		} else {
			buf = malloc(sizeof(char) * lexer->ring.cap);
			read = fread(buf, sizeof(char), lexer->ring.cap, lexer->fd);
		}
#else
		buf = malloc(sizeof(char) * lexer->ring.cap);
		if (lexer->fd == stdin) {
			printf("fala> ");
			read = read_line(buf, lexer->ring.cap, lexer->fd);
		} else
			read = fread(buf, sizeof(char), lexer->ring.cap, lexer->fd);
#endif

		if (read > 0) ring_write_many(&lexer->ring, buf, read);
#ifdef FALA_WITH_READLINE
		if (lexer->fd == stdin) ring_write(&lexer->ring, '\n');
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
	char* copy = malloc(sizeof(char) * (len + 1));
	for (size_t i = 0; i < len; i++) copy[i] = str[i];
	copy[len] = '\0';
	return copy;
}

int lexer_lex(union TokenValue* value, Location* loc, void* _lexer) {
	loc->first_line = loc->last_line;
	loc->first_column = loc->last_column;

	Lexer* lexer = (Lexer*)_lexer;
	(void)loc;
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
				char buf[256] = {[0] = c};
				size_t len = 1;
				while (is_valid_id_char(c = peek(lexer))) {
					buf[len++] = c;
					advance(lexer, loc);
				}
				buf[len] = '\0';

				// could be a reserved keyword
				for (size_t i = 0; i < KW_COUNT; i++)
					if (strcmp(buf, keywords[i]) == 0) return keyword_parser_map[i];

				value->str = string_dup(buf);
				return ID;
			}
			assert(false && "LEX_ERR: Unrecognized character");
		}
	};
	assert(false && "unreachable");
}

LEXER lexer_init_from_file(FILE* fd) {
	Lexer* lexer = malloc(sizeof(Lexer));
	lexer->fd = fd;
	lexer->ring = ring_init();
	return lexer;
}

void lexer_deinit(LEXER _lexer) {
	Lexer* lexer = (Lexer*)_lexer;
	ring_deinit(&lexer->ring);
	free(lexer);
}

bool is_interactive(LEXER lexer) { return ((Lexer*)lexer)->fd == stdin; }
