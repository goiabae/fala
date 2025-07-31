#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <cassert>
#include <string>
#include <vector>

#include "ast.hpp"

#define ANSI_STYLE_BOLD "\x1b[1m"
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

enum LogLevel {
	WARN,
	ERROR,
	INFO,
};

struct Logger {
	Logger(
		std::string domain, std::string file_name, std::vector<std::string> lines
	)
	: domain {domain}, file_name {file_name}, lines {lines} {}

 private:
	void print_lines(Location loc) {
		assert(loc.begin.line >= 0 && loc.begin.line < lines.size());

		if (loc.begin.line > 0) {
			const auto& prev_line = lines[(size_t)loc.begin.line - 1];
			fprintf(stderr, "     |\t%s\n", prev_line.c_str());
		}

		const auto& line = lines[(size_t)loc.begin.line];
		fprintf(stderr, " %3d |\t", loc.begin.line);
		for (size_t i = 0; i < line.size(); i++) {
			if (i == loc.begin.column) {
				fprintf(stderr, ANSI_STYLE_BOLD);
			}
			fprintf(stderr, "%c", line[i]);
			if (loc.begin.line == loc.end.line && i == loc.end.column) {
				fprintf(stderr, ANSI_COLOR_RESET);
			}
		}
		fprintf(stderr, "\n");

		fprintf(stderr, "     |\t");
		for (size_t i = 0; i < line.size(); i++) {
			if (i == loc.begin.column) {
				fprintf(stderr, "^");
			} else {
				fprintf(stderr, "~");
			}
		}
		fprintf(stderr, "\n");

		if (loc.begin.line < (int)(lines.size() - 1)) {
			const auto& next_line = lines[(size_t)loc.begin.line + 1];
			fprintf(stderr, "     |\t%s\n", next_line.c_str());
		}
	}

 public:
	template<typename... Args>
	void log(LogLevel level, Location loc, std::string format, Args... args) {
		if (level == ERROR) {
			fprintf(
				stderr,
				ANSI_STYLE_BOLD "%s:%d:%d: " ANSI_COLOR_RED "%s ERROR" ANSI_COLOR_RESET
												": ",
				file_name.c_str(),
				loc.begin.line + 1,
				loc.begin.column + 1,
				domain.c_str()
			);
			fprintf(stderr, format.c_str(), args...);
			fprintf(stderr, "\n");
			print_lines(loc);
			exit(1);
		} else {
			throw "TODO: uninplemented";
		}
	}

	void log(LogLevel level, Location loc, std::string format) {
		if (level == ERROR) {
			fprintf(
				stderr,
				ANSI_STYLE_BOLD "%s:%d:%d: " ANSI_COLOR_RED "%s ERROR" ANSI_COLOR_RESET
												": ",
				file_name.c_str(),
				loc.begin.line + 1,
				loc.begin.column + 1,
				domain.c_str()
			);
			fprintf(stderr, format.c_str());
			fprintf(stderr, "\n");
			print_lines(loc);
			exit(1);
		} else {
			throw "TODO: uninplemented";
		}
	}

	template<typename... Args>
	void err(Location loc, std::string format, Args... args) {
		log(ERROR, loc, format, args...);
	}

 private:
	std::string domain;
	std::string file_name;
	std::vector<std::string> lines;
};

#endif
