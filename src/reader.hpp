#ifndef READER_HPP
#define READER_HPP

#include <string>

struct Reader {
	virtual ~Reader() {};

	virtual std::string get_path() const = 0;
	virtual bool at_eof() const = 0;
	virtual bool is_interactive() const = 0;

	virtual size_t read_at_most(char* buffer, size_t limit) = 0;
};

#endif
