#include "string_reader.hpp"

StringReader::StringReader(std::string string)
: m_string {string}, m_cursor {0} {}

std::string StringReader::get_path() const { return "<string>"; }

bool StringReader::at_eof() const { return m_cursor >= m_string.size(); }

bool StringReader::is_interactive() const { return false; }

size_t StringReader::read_at_most(char* buffer, size_t limit) {
	size_t read = 0;
	while (read < limit and not at_eof()) {
		buffer[read++] = m_string[m_cursor++];
	}
	return read;
}
