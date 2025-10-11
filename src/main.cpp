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
#include "options.hpp"
#include "str_pool.h"
#include "typecheck.hpp"
#include "walk.hpp"

#ifdef EXPERIMENTAL_HIR_COMPILER
#	include "hir_compiler.hpp"
#endif

typedef int Number;
typedef char* String;

static AST parse(File& file, StringPool& pool) {
	Lexer lexer(file);
	AST ast {};
	ast.file_name = lexer.file->get_name();
	yy::parser parser {&lexer, &ast, pool};
	if (parser.parse()) exit(1); // FIXME propagate error up
	ast.lines = lexer.get_lines();
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

int interpret(Options opts) {
	File fd = opts.from_stdin ? stdin : File(opts.argv[0], "r");
	if (!fd) return 1;

	StringPool pool;

	while (!fd.at_eof()) {
		AST ast = parse(fd, pool);
		if (ast.is_empty()) break;

		if (opts.verbose) {
			ast_print(&ast, pool);
			printf("\n");
		}

		Typechecker checker {ast, pool};
		checker.typecheck();

		if (opts.use_walk_interpreter) {
			walk::Interpreter inter {pool};
			auto val = inter.eval(ast);
			if (opts.from_stdin) {
				print_value(val);
				printf("\n");
			}
		} else {
			compiler::Compiler comp {ast, pool};
			auto chunk = comp.compile();

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
	if (ast.is_empty()) return 1;

	if (opts.verbose) {
		ast_print(&ast, pool);
		printf("\n");
	}

	Typechecker checker {ast, pool};
	checker.typecheck();

#ifdef EXPERIMENTAL_HIR_COMPILER
	ast_print_detailed(&ast, &pool);
	hir_compiler::Compiler hir_comp {ast, pool, checker};
	auto code = hir_comp.compile();
	hir::print_code(stderr, code, pool, 0);
#endif

	compiler::Compiler comp {ast, pool};
	auto chunk = comp.compile();

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
