#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lir.hpp"
#include "vm.hpp"

// clang-format off
// need to be included in this order
#include "parser.hpp"
#include "lexer.hpp"
// clang-format on

#include "ast.hpp"
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
	AST ast {};
	yy::parser parser {&lexer, &ast, &pool};
	if (parser.parse()) exit(1); // FIXME propagate error up
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

// getopt comes from POSIX which is not available on Windows
#ifdef _WIN32
int optind = 1;
char* optarg = nullptr;

bool is_option(const char* str) { return strlen(str) == 2 && str[0] == '-'; }

int getopt(int argc, char** argv, const char* opts_) {
	static const char* opts = opts_;

	for (size_t i = 0; i < strlen(opts); i++) {
		if (opts[i] == ':') continue;

		// iterate the remaining arguments
		for (int j = optind; j < argc; j++) {
			int window_len = (is_option(argv[j]) && argv[j][1] == opts[i]) ? 2 : 1;

			// option found in argument vector
			if (is_option(argv[j]) && argv[j][1] == opts[i]) {
				if (opts[i + 1] == ':' && !is_option(argv[j + 1])) {
					char* opt = argv[j];
					char* arg = argv[j + 1];

					for (int k = j + 1; k >= optind + 1; k--) argv[k] = argv[k - 1 - 1];

					argv[optind] = opt;
					argv[optind + 1] = arg;
				} else {
					char* opt = argv[j];

					for (int k = j + 0; k >= optind + 0; k--) argv[k] = argv[k - 1];

					argv[optind] = opt;
				}

				optind += window_len;
				optarg = (window_len > 1) ? argv[j + 1] : nullptr;

				return opts[i];
			}
		}
	}

	return -1;
}
#else
#	include <getopt.h>
#endif

Options parse_args(int argc, char* argv[]) {
	Options opts {};

	for (char c = 0; (c = (char)getopt(argc, argv, "Vwo:ci")) != -1;) switch (c) {
			case 'V': opts.verbose = true; break;
			case 'w': opts.use_walk_interpreter = true; break;
			case 'o': opts.output_path = optarg; break;
			case 'c': opts.compile = true; break;
			case 'i': opts.interpret = true; break;
			default: break;
		}

	opts.argv = &argv[optind];
	opts.argc = argc - optind;

	if (opts.argc < 1) {
		opts.is_invalid = true;
		return opts;
	}

	if (opts.argc > 0 && strcmp(opts.argv[0], "-") == 0) opts.from_stdin = true;

	if (!(opts.compile || opts.interpret)) opts.is_invalid = true;

	return opts;
}

int interpret(Options opts) {
	File fd = opts.from_stdin ? stdin : File(opts.argv[0], "r");
	if (!fd) return 1;

	StringPool pool;

	while (!fd.at_eof()) {
		AST ast = parse(fd, pool);

		if (opts.verbose) {
			ast_print(&ast, &pool);
			printf("\n");
		}

		Typechecker checker {ast, pool};
		checker.typecheck();

		if (opts.use_walk_interpreter) {
			walk::Interpreter inter {&pool};
			auto val = inter.eval(ast);
			if (opts.from_stdin) {
				print_value(val);
				printf("\n");
			}
		} else {
			compiler::Compiler comp;
			auto chunk = comp.compile(ast, pool);

			if (opts.verbose) {
				lir::print_chunk(stdout, chunk);
				printf("\n");
			}

			vm::run(chunk);
		}
	}

	return 0;
}

int compile(Options opts) {
	File input = opts.from_stdin ? stdin : File(opts.argv[0], "r");
	if (!input) return 1;

	StringPool pool;
	AST ast = parse(input, pool);

	if (opts.verbose) {
		ast_print(&ast, &pool);
		printf("\n");
	}

	Typechecker checker {ast, pool};
	checker.typecheck();

	compiler::Compiler comp;
	auto chunk = comp.compile(ast, pool);

	File output = (opts.output_path) ? File(opts.output_path, "w") : stdout;
	if (!output) return 1;

	print_chunk(output.get_descriptor(), chunk);

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
