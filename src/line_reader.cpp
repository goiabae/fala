#include "line_reader.hpp"

#include <cstdio>
#include <cstring>
#include <iostream>

std::string LineReader::get_path() const { return "<repl-input>"; }

bool LineReader::at_eof() const { return std::feof(stdin); }

bool LineReader::is_interactive() const { return true; }

size_t LineReader::read_at_most(char* buffer, size_t limit) {
	std::cout << "fala> ";
	auto p = std::fgets(buffer, (int)limit, stdin);
	if (p == nullptr) return 0;
	return std::strlen(buffer);
}
