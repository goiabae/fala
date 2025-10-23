#ifndef STRING_READER_HPP
#define STRING_READER_HPP

#include "reader.hpp"

struct StringReader : Reader {
	StringReader(std::string string);

	std::string get_path() const override;
	bool at_eof() const override;
	bool is_interactive() const override;

	size_t read_at_most(char* buffer, size_t limit) override;

 private:
	std::string m_string;
	size_t m_cursor;
};

#endif
