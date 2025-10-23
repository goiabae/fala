#ifndef FILE_READER_HPP
#define FILE_READER_HPP

#include <stdio.h>

#include <string>

#include "reader.hpp"

struct FileReader : Reader {
	FileReader(const char* path, const char* mode);
	FileReader(FILE* fd);

	~FileReader() override;
	FileReader& operator=(const FileReader& other) = delete;
	FileReader& operator=(FileReader&& other) = delete;
	FileReader(const FileReader& other) = delete;
	FileReader(FileReader&& other);

	std::string get_path() const override;
	bool at_eof() const override;
	bool is_interactive() const override;

	size_t read_at_most(char* buffer, size_t limit) override;

 private:
	FILE* m_fd;
	bool m_owned {true};
	std::string name {};
};

#endif
