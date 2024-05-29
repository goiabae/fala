#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#	include <getopt.h>
#endif

// clang-format off
// need to be included in this order
extern "C" {
#include "parser.h"
}
#include "lexer.h"
// clang-format on

#include "ast.h"
#include "compiler.hpp"
#include "eval.hpp"
#include "file.hpp"
#include "str_pool.h"

typedef int Number;
typedef char* String;

typedef struct Options {
	bool is_invalid;
	bool verbose;
	bool from_stdin;
	char* output_path;
	bool compile;
	bool interpret;
	char** argv;
	int argc;
} Options;

static AST parse(File& file, StringPool& pool) {
	Lexer lexer(file);
	AST ast = ast_init();
	if (yyparse(&lexer, &ast, &pool)) exit(1); // FIXME propagate error up
	return ast;
}

static void usage() {
	printf(
		"Usage:\n"
		"\tfala <mode> [<options> ...] <filepath>\n"
		"\n"
		"Filepath:\n"
		"\tif <filepath> is \"-\", then stdin is used and a REPL session is "
		"started\n"
		"\n"
		"Options:\n"
		"\t-V          verbose output\n"
		"\t-o <path>   output file path. if no path is provided, stdout is used\n"
		"\n"
		"Modes:\n"
		"\t-c          compile\n"
		"\t-i          intepret\n"
	);
}

static Options parse_args(int argc, char* argv[]) {
	Options opts;
	opts.verbose = false;
	opts.is_invalid = false;
	opts.output_path = NULL;
	opts.from_stdin = false;
	opts.compile = false;
	opts.interpret = false;

	// getopt comes from POSIX which is not available on Windows
#ifndef _WIN32
	for (char c = 0; (c = (char)getopt(argc, argv, "Vo:ci")) != -1;) switch (c) {
			case 'V': opts.verbose = true; break;
			case 'o': opts.output_path = optarg; break;
			case 'c': opts.compile = true; break;
			case 'i': opts.interpret = true; break;
			default: break;
		}
#else
	size_t optind = 1;

	for (size_t i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] != '\0') {
			switch (argv[i][1]) {
				// FIXME parser -o output file
				case 'V': opts.verbose = true; break;
				case 'c': opts.compile = true; break;
				case 'i': opts.interpret = true; break;
			}
			optind++;
		} else
			break;
	}

#endif
	opts.argv = &argv[optind];
	opts.argc = argc - optind;

	if (opts.argc < 1) {
		opts.is_invalid = true;
		return opts;
	}

	if (opts.argc > 0 && strcmp(opts.argv[0], "-") == 0) opts.from_stdin = true;

	return opts;
}

static int interpret(Options opts) {
	File fd = opts.from_stdin ? stdin : File(opts.argv[0], "r");
	if (!fd) return 1;

	StringPool pool;
	Interpreter inter(&pool);
	Value val;

	while (!fd.at_eof()) {
		AST ast = parse(fd, pool);
		if (opts.verbose) {
			ast_print(ast, &pool);
			printf("\n");
		}
		val = inter.eval(ast);
		if (opts.from_stdin) {
			print_value(val);
			printf("\n");
		}
		ast_deinit(ast);
	}

	return (val.type == Value::Type::NUM) ? val.num : 0;
}

static int compile(Options opts) {
	File fd = opts.from_stdin ? stdin : File(opts.argv[0], "r");
	if (!fd) return 1;

	StringPool pool;
	AST ast = parse(fd, pool);
	if (opts.verbose) {
		ast_print(ast, &pool);
		printf("\n");
	}

	Compiler comp;
	Chunk chunk = comp.compile(ast, pool);

	if (opts.output_path) {
		File out(opts.output_path, "w");
		print_chunk(out.get_descriptor(), chunk);
	} else if (opts.verbose) {
		print_chunk(stdout, chunk);
	}

	ast_deinit(ast);
	return 0;
}

int main(int argc, char* argv[]) {
	Options opts = parse_args(argc, argv);
	if (opts.is_invalid) {
		usage();
		return 1;
	}

	if (opts.compile)
		compile(opts);
	else if (opts.interpret)
		interpret(opts);

	return 0;
}
