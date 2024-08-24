#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.hpp"

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
#include "file.hpp"
#include "str_pool.h"
#include "typecheck.hpp"
#include "walk.hpp"

typedef int Number;
typedef char* String;

struct Options {
	bool use_walk_interpreter {false};
	bool is_invalid {false};
	bool verbose {false};
	bool from_stdin {false};
	char* output_path {nullptr};
	bool compile {false};
	bool interpret {false};
	char** argv {nullptr};
	int argc {0};
};

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

Options parse_args(int argc, char* argv[]) {
	Options opts {};

	// getopt comes from POSIX which is not available on Windows
#ifndef _WIN32
	for (char c = 0; (c = (char)getopt(argc, argv, "Vwo:ci")) != -1;) switch (c) {
			case 'V': opts.verbose = true; break;
			case 'w': opts.use_walk_interpreter = true; break;
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
				case 'w': opts.use_walk_interpreter = true; break;
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

int interpret(Options opts) {
	File fd = opts.from_stdin ? stdin : File(opts.argv[0], "r");
	if (!fd) return 1;

	StringPool pool;

	while (!fd.at_eof()) {
		AST ast = parse(fd, pool);
		typecheck(ast);

		if (opts.verbose) {
			ast_print(ast, &pool);
			printf("\n");
		}

		if (opts.use_walk_interpreter) {
			walk::Interpreter inter {&pool};
			auto val = inter.eval(ast);
			if (opts.from_stdin) {
				print_value(val);
				printf("\n");
			}
		} else {
			Compiler comp;
			Chunk chunk = comp.compile(ast, pool);
			vm::run(chunk);
		}

		ast_deinit(ast);
	}

	return 0;
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

	typecheck(ast);

	Compiler comp;
	Chunk chunk = comp.compile(ast, pool);

	if (opts.output_path) {
		File out(opts.output_path, "w");
		print_chunk(out.get_descriptor(), chunk);
	} else {
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
	else
		compile(opts);

	return 0;
}
