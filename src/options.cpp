#include "options.hpp"

#include <cstring>

Options parse_args(int argc, char* argv[]) {
	Options opts {};

	for (char c = 0; (c = (char)getopt(argc, argv, "Vwo:ci")) != -1;) switch (c) {
			case 'V': opts.verbosity += 1; break;
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
