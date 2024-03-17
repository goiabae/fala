#include "main.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#	include <getopt.h>
#endif

// clang-format off
// need to be included in this order
#include "parser.h"
#include "lexer.h"
// clang-format on

#include "ast.h"
#include "eval.h"

typedef struct Options {
	bool is_invalid;
	bool verbose;
#ifdef FALA_WITH_REPL
	bool run_repl;
#endif
	char** argv;
	int argc;
} Options;

static const char* node_repr[] = {
	[FALA_APP] = "app",       [FALA_NUM] = NULL,       [FALA_BLOCK] = "block",
	[FALA_IF] = "if",         [FALA_WHEN] = "when",    [FALA_FOR] = "for",
	[FALA_WHILE] = "while",   [FALA_ASS] = "=",        [FALA_OR] = "or",
	[FALA_AND] = "and",       [FALA_GREATER] = ">",    [FALA_LESSER] = "<",
	[FALA_GREATER_EQ] = ">=", [FALA_LESSER_EQ] = "<=", [FALA_EQ] = "==",
	[FALA_ADD] = "+",         [FALA_SUB] = "-",        [FALA_MUL] = "*",
	[FALA_DIV] = "/",         [FALA_MOD] = "%",        [FALA_NOT] = "not",
	[FALA_ID] = NULL,         [FALA_STRING] = NULL,    [FALA_DECL] = "decl",
	[FALA_VAR] = "var",       [FALA_LET] = "let",
};

static void print_node(SymbolTable* tab, Node node, unsigned int space) {
	if (node.type == FALA_NUM) {
		printf("%d", node.num);
		return;
	} else if (node.type == FALA_ID) {
		printf("%s", sym_table_get(tab, node.index));
		return;
	} else if (node.type == FALA_STRING) {
		printf("\"");
		for (char* it = sym_table_get(tab, node.index); *it != '\0'; it++) {
			if (*it == '\n')
				printf("\\n");
			else
				printf("%c", *it);
		}
		printf("\"");
		return;
	} else if (node.type == FALA_NIL) {
		printf("nil");
		return;
	} else if (node.type == FALA_TRUE) {
		printf("true");
		return;
	}

	printf("(");

	printf("%s", node_repr[node.type]);

	space += 2;

	for (size_t i = 0; i < node.children_count; i++) {
		printf("\n");
		for (size_t j = 0; j < space; j++) printf(" ");
		print_node(tab, node.children[i], space);
	}

	printf(")");
}

static void print_ast(AST ast, SymbolTable* syms) {
	print_node(syms, ast.root, 0);
}

static AST parse(FILE* fd, SymbolTable* syms) {
	yyscan_t scanner = NULL;
	yylex_init(&scanner);
	yyset_in(fd, scanner);

	AST ast = ast_init();
	if (yyparse(scanner, &ast, syms)) {
		exit(1); // FIXME propagate error up
	}

	yylex_destroy(scanner);
	return ast;
}

static void usage() {
	printf(
		"Usage:\n"
		"\tfala [<options> ...] <filepath>\n"
		"Options:\n"
		"\t-V    verbose output"
	);
}

static Options parse_args(int argc, char* argv[]) {
	Options opts;
	opts.verbose = false;
	opts.is_invalid = false;

	// getopt comes from POSIX which is not available on Windows
#ifndef _WIN32
	for (char c = 0; (c = getopt(argc, argv, "V")) != -1;) switch (c) {
			case 'V': opts.verbose = true; break;
			default: break;
		}

	opts.argv = &argv[optind];
	opts.argc = argc - optind;
#else
	opts.argv = &argv[1];
	opts.argc = argc - 1;
#endif

	if (opts.argc < 1) opts.is_invalid = true;

	return opts;
}

#ifdef FALA_WITH_REPL
static int repl(Options opts) {
	FILE* fd = stdin;
	Interpreter inter = interpreter_init();
	SymbolTable syms = sym_table_init();

	while (!feof(fd)) {
		AST ast = parse(fd, &syms);
		if (opts.verbose) {
			print_ast(ast, &syms);
			printf("\n");
		}

		Value val = ast_eval(&inter, &syms, ast);
		print_value(val);
		printf("\n");
		ast_deinit(ast);
	}

	sym_table_deinit(&syms);
	interpreter_deinit(&inter);
	fclose(fd);
	return 0;
}
#endif

static int interpret(Options opts) {
#ifdef FALA_WITH_REPL
	FILE* fd = fopen(opts.argv[0], "r");
#else
	FILE* fd =
		(strcmp(opts.argv[0], "-") == 0) ? stdin : fopen(opts.argv[0], "r");
#endif

	if (!fd) return 1;

	Interpreter inter = interpreter_init();
	SymbolTable syms = sym_table_init();

	AST ast = parse(fd, &syms);
	if (opts.verbose) {
		print_ast(ast, &syms);
		printf("\n");
	}

	Value val = ast_eval(&inter, &syms, ast);

	ast_deinit(ast);
	sym_table_deinit(&syms);
	interpreter_deinit(&inter);
	fclose(fd);

	if (val.tag == VALUE_NUM)
		return val.num;
	else
		return 0;
}

int main(int argc, char* argv[]) {
	Options opts = parse_args(argc, argv);
	if (opts.is_invalid) {
		usage();
		return 1;
	}

#ifdef FALA_WITH_REPL
	if (opts.run_repl)
		return repl(opts);
	else
#endif
		return interpret(opts);
}
