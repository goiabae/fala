#ifndef LINE_READER_HPP
#define LINE_READER_HPP

#include "reader.hpp"

struct LineReader : Reader {
	std::string get_path() const override;
	bool at_eof() const override;
	bool is_interactive() const override;

	size_t read_at_most(char* buffer, size_t limit) override;
};

#endif
