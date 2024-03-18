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
	[AST_APP] = "app",     [AST_NUM] = NULL,    [AST_BLK] = "block",
	[AST_IF] = "if",       [AST_WHEN] = "when", [AST_FOR] = "for",
	[AST_WHILE] = "while", [AST_ASS] = "=",     [AST_OR] = "or",
	[AST_AND] = "and",     [AST_GTN] = ">",     [AST_LTN] = "<",
	[AST_GTE] = ">=",      [AST_LTE] = "<=",    [AST_EQ] = "==",
	[AST_ADD] = "+",       [AST_SUB] = "-",     [AST_MUL] = "*",
	[AST_DIV] = "/",       [AST_MOD] = "%",     [AST_NOT] = "not",
	[AST_ID] = NULL,       [AST_STR] = NULL,    [AST_DECL] = "decl",
	[AST_VAR] = "var",     [AST_LET] = "let",
};

static void print_node(SymbolTable* tab, Node node, unsigned int space) {
	if (node.type == AST_NUM) {
		printf("%d", node.num);
		return;
	} else if (node.type == AST_ID) {
		printf("%s", sym_table_get(tab, node.index));
		return;
	} else if (node.type == AST_STR) {
		printf("\"");
		for (char* it = sym_table_get(tab, node.index); *it != '\0'; it++) {
			if (*it == '\n')
				printf("\\n");
			else
				printf("%c", *it);
		}
		printf("\"");
		return;
	} else if (node.type == AST_NIL) {
		printf("nil");
		return;
	} else if (node.type == AST_TRUE) {
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
