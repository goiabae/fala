#include "line_reader.hpp"

#ifdef WITH_READLINE
#	include <readline/readline.h>
#	include <readline/history.h>
#endif

#include <cstdio>
#include <cstring>
#include <iostream>

std::string LineReader::get_path() const { return "<repl-input>"; }

bool LineReader::at_eof() const { return std::feof(stdin); }

bool LineReader::is_interactive() const { return true; }

size_t LineReader::read_at_most(char* buffer, size_t limit) {
#ifdef WITH_READLINE
	char* line = readline("fala> ");
	if (line == nullptr) return 0;
	add_history(line);
	size_t read = std::strlen(line);
	if (read > limit) return 0;
	strncpy(buffer, line, read);
	buffer[read++] = '\n';
	buffer[read] = '\0';
	free(line);
	return read;
#else
	std::cout << "fala> ";
	auto p = std::fgets(buffer, (int)limit, stdin);
	if (p == nullptr) return 0;
	return std::strlen(buffer);
#endif
}
