#include "options.hpp"

#include <cstring>
#include <iostream>

#ifdef _WIN32
int getopt(int argc, char** argv, const char* opts_);
#else
#	include <getopt.h>
#endif

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
#endif

Options parse_args(int argc, char* argv[]) {
	Options opts {};

	for (char c = 0; (c = (char)getopt(argc, argv, "Vo:cib:")) != -1;)
		switch (c) {
			case 'V': opts.verbosity += 1; break;
			case 'o': opts.output_path = optarg; break;
			case 'c': opts.compile = true; break;
			case 'i': opts.interpret = true; break;
			case 'b': {
				if (strcmp(optarg, "walk") == 0) {
					opts.backend = Backend::WALK;
				} else if (strcmp(optarg, "lir") == 0) {
					opts.backend = Backend::LIR;
				}
#ifdef EXPERIMENTAL_HIR_COMPILER
				else if (strcmp(optarg, "hir") == 0) {
					opts.backend = Backend::HIR;
				}
#endif
				else {
					std::cerr << "Unknown backend: " << optarg << '\n';
					opts.is_invalid = true;
					return opts;
				}
				break;
			}
			default: {
				opts.is_invalid = true;
				return opts;
			}
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
