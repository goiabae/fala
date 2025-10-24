#ifndef OPTIONS_HPP
#define OPTIONS_HPP

struct Options {
	bool use_walk_interpreter {false};
	bool is_invalid {false};
	unsigned int verbosity {0};
	bool from_stdin {false};
	char* output_path {nullptr};
	bool compile {false};
	bool interpret {false};
	char** argv {nullptr};
	int argc {0};
};

#ifdef _WIN32
int getopt(int argc, char** argv, const char* opts_);
#else
#	include <getopt.h>
#endif

Options parse_args(int argc, char* argv[]);

#endif
