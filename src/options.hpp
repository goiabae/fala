#ifndef OPTIONS_HPP
#define OPTIONS_HPP

enum class Backend {
	WALK,
	LIR,
#ifdef EXPERIMENTAL_HIR_COMPILER
	HIR,
#endif
};

struct Options {
	Backend backend {Backend::LIR};
	bool is_invalid {false};
	unsigned int verbosity {0};
	bool from_stdin {false};
	char* output_path {nullptr};
	bool compile {false};
	bool interpret {false};
	char** argv {nullptr};
	int argc {0};
};

Options parse_args(int argc, char* argv[]);

#endif
