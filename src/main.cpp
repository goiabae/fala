#include <utility>

#include "ast.hpp"
#include "compiler.hpp"
#include "file.hpp"
#include "file_reader.hpp"
#include "lexer.hpp"
#include "line_reader.hpp"
#include "lir.hpp"
#include "logger.hpp"
#include "options.hpp"
#include "parser.hpp"
#include "str_pool.h"
#include "typecheck.hpp"
#include "vm.hpp"
#include "walk.hpp"

#ifdef EXPERIMENTAL_HIR_COMPILER
#	include "hir_compiler.hpp"
#endif

namespace {

void usage() {
	printf(
		"Usage:\n"
		"\tfala <mode> [<options> ...] <filepath>\n"
		"\n"
		"Filepath:\n"
		"\tif <filepath> is \"-\", then stdin is used and a REPL session is "
		"started\n"
		"\n"
		"Options:\n"
		"\t-V          verbose output. use multiple times to increase verbosity\n"
		"\t-o <path>   output file path. if no path is provided, stdout is used\n"
		"\t-b <name>   backend to be used. one of: walk, lir"
#ifdef EXPERIMENTAL_HIR_COMPILER
		", hir"
#endif
		"\n"
		"\n"
		"Modes:\n"
		"\t-c          compile\n"
		"\t-i          intepret\n"
	);
}

void print_phase(const Options& opts, std::string phase) {
	if (opts.verbosity >= 1)
		std::cerr << ANSI_COLOR_YELLOW << "INFO" << ANSI_COLOR_RESET << ": "
							<< phase << "..." << '\n';
}

int interpret(Options opts) {
	Reader* fd = opts.from_stdin
	             ? static_cast<Reader*>(new LineReader())
	             : static_cast<Reader*>(new FileReader(opts.argv[0], "r"));

	StringPool pool;

	while (!fd->at_eof()) {
		print_phase(opts, "parsing");
		AST ast = parse(fd, pool);
		if (ast.is_empty()) break;

		if (opts.verbosity >= 2) {
			ast_print(&ast, pool);
			printf("\n");
		}

		print_phase(opts, "type checking");
		Typechecker checker {ast, pool};
		checker.typecheck();

		if (opts.backend == Backend::WALK) {
			print_phase(opts, "interpreting(walk)");
			walk::Interpreter inter {pool, ast, std::cin, std::cout};
			auto val = inter.eval();
			if (opts.from_stdin) {
				std::cout << val;
				printf("\n");
			}
		} else if (opts.backend == Backend::LIR) {
			print_phase(opts, "compiling(lir)");
			compiler::Compiler comp {ast, pool};
			auto chunk = comp.compile();

			if (opts.verbosity >= 2) {
				lir::print_chunk(stdout, chunk);
				printf("\n");
			}

			print_phase(opts, "interpreting(lir)");
			lir::VM vm {std::cin, std::cout};
			vm.should_print_result = opts.from_stdin;
			vm.run(chunk);
		} else {
			std::cerr << "Backend can't be used for interpreting" << '\n';
			return 1;
		}
	}

	return 0;
}

int compile(Options opts) {
	Reader* input = opts.from_stdin
	                ? static_cast<Reader*>(new LineReader())
	                : static_cast<Reader*>(new FileReader(opts.argv[0], "r"));

	StringPool pool;
	print_phase(opts, "parsing");
	AST ast = parse(input, pool);
	if (ast.is_empty()) return 1;

	if (opts.verbosity >= 3) {
		ast_print_detailed(&ast, pool);
	} else if (opts.verbosity >= 2) {
		ast_print(&ast, pool);
		printf("\n");
	}

	print_phase(opts, "type checking");
	Typechecker checker {ast, pool};
	checker.typecheck();

	if (opts.backend == Backend::LIR) {
		print_phase(opts, "compiling(lir)");
		compiler::Compiler comp {ast, pool};
		auto chunk = comp.compile();

		File output = (opts.output_path) ? File(opts.output_path, "w") : stdout;

		print_phase(opts, "saving output");
		print_chunk(output.get_descriptor(), chunk);

		return 0;
#ifdef EXPERIMENTAL_HIR_COMPILER
	} else if (opts.backend == Backend::HIR) {
		hir_compiler::Compiler hir_comp {ast, pool, checker};
		print_phase(opts, "compiling(lir)");
		auto code = hir_comp.compile();

		File output = (opts.output_path) ? File(opts.output_path, "w") : stdout;

		print_phase(opts, "saving output");
		hir::print_code(output.get_descriptor(), code, pool, 0);

		return 0;
#endif
	} else {
		std::cerr << "Can't compile with backend\n";
		return 1;
	}
}

} // namespace

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
		std::unreachable();

	return 0;
}
