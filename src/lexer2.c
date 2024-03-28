#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

typedef struct Lexer {
	FILE *fd;
	char *buf;
	char *next;
	size_t len;
} Lexer;

LEXER lexer_init_from_file(FILE *fd) {
	Lexer *lexer = malloc(sizeof(Lexer));
	lexer->fd = fd;
	return (LEXER)lexer;
}

void lexer_deinit(LEXER lexer) { free(lexer); }

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

static const int keyword_parser_map[KW_COUNT] = {
	[KW_DO] = DO,     [KW_END] = END,     [KW_IF] = IF,     [KW_THEN] = THEN,
	[KW_ELSE] = ELSE, [KW_WHEN] = WHEN,   [KW_FOR] = FOR,   [KW_FROM] = FROM,
	[KW_TO] = TO,     [KW_WHILE] = WHILE, [KW_VAR] = VAR,   [KW_LET] = LET,
	[KW_IN] = IN,     [KW_FUN] = FUN,     [KW_OR] = OR,     [KW_AND] = AND,
	[KW_NOT] = NOT,   [KW_NIL] = NIL,     [KW_TRUE] = TRUE,
};

static const char *keywords[KW_COUNT] = {
	[KW_DO] = "do",       [KW_END] = "end",   [KW_IF] = "if",
	[KW_THEN] = "then",   [KW_ELSE] = "else", [KW_WHEN] = "when",
	[KW_FOR] = "for",     [KW_FROM] = "from", [KW_TO] = "to",
	[KW_WHILE] = "while", [KW_VAR] = "var",   [KW_LET] = "let",
	[KW_IN] = "in",       [KW_FUN] = "fun",   [KW_OR] = "or",
	[KW_AND] = "and",     [KW_NOT] = "not",   [KW_NIL] = "nil",
	[KW_TRUE] = "true",
};

static char *read_whole_file(FILE *fd, size_t *len) {
	fseek(fd, 0, SEEK_END);
	*len = ftell(fd);
	fseek(fd, 0, SEEK_SET);
	char *str = malloc(sizeof(char) * (*len));
	fread(str, sizeof(char), *len, fd);
	return str;
}

static int advance(Lexer *lexer) {
	if (lexer->next >= &lexer->buf[lexer->len - 1]) return 0;
	char res = lexer->next[0];
	lexer->next++;
	return res;
}

static bool is_valid_id_char(char c) { return c == '_' || isalnum(c); }

int lexer_lex(union TokenValue *lval, Location *location, LEXER interface) {
	Lexer *lexer = (Lexer *)interface;

	if (lexer->buf == NULL) {
		lexer->buf = read_whole_file(lexer->fd, &lexer->len);
		lexer->next = lexer->buf;
	}

	char c = advance(lexer);
	if (!c) return YYEOF;
	switch (c) {
		case '(': return PAREN_OPEN;
		case ')': return PAREN_CLOSE;
		case '[': return BRACKET_OPEN;
		case ']': return BRACKET_CLOSE;
		case ';': return SEMICOL;
		case ',': return COMMA;
		case '=': return (advance(lexer) == '=') ? EQ_EQ : EQ;
		case '>': return (advance(lexer) == '=') ? GREATER_EQ : GREATER;
		case '<': return (advance(lexer) == '=') ? LESSER_EQ : LESSER;
		case '+': return PLUS;
		case '-': return MINUS;
		case '*': return ASTER;
		case '/': return SLASH;
		case '%': return PERCT;

		case ' ': return lexer_lex(lval, location, interface);
		case '\t': return lexer_lex(lval, location, interface);
		case '\n': return lexer_lex(lval, location, interface);

		case '"': {
			const char *beg = lexer->next;
			size_t len = 0;
			while (advance(lexer) != '"') len++;
			char *str = malloc(sizeof(char) * (len + 1));
			strncpy(str, beg, len); // TODO parse string
			str[len] = '\0';
			lval->str = str;
			return STRING;
		}
		default: {
			for (size_t i = 0; i < KW_COUNT; i++) {
				size_t kw_len = strlen(keywords[i]);
				if (strncmp(lexer->next, keywords[i], kw_len) == 0) {
					lexer->next += kw_len;
					return keyword_parser_map[i];
				}
			}
			if (isdigit(c)) {
				long num = c - '0';
				while (isdigit(c = advance(lexer))) num = num * 10 + (c - '0');
				lval->num = num;
				return NUMBER;
			} else if (isalpha(c) || c == '_') {
				const char *beg = lexer->next - 1;
				size_t len = 1;
				while (is_valid_id_char(c = advance(lexer))) len++;
				char *id = malloc(sizeof(char) * (len + 1));
				strncpy(id, beg, len);
				id[len] = '\0';
				lval->str = id;
				return ID;
			} else {
				assert(false && "LEXICAL_ERR: Unrecognized character");
			}
		}
	}
	return 0;
}

bool is_interactive(void *interface) {
	Lexer *scanner = (Lexer *)interface;
	return scanner->fd == stdin;
}
