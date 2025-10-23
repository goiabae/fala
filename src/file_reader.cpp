
#include "file_reader.hpp"

#include <stdexcept>

FileReader::FileReader(const char* path, const char* mode)
: m_fd {fopen(path, mode)}, name {path} {
	if (m_fd == nullptr)
		throw std::domain_error("Could not open file " + std::string(path));
}

FileReader::FileReader(FILE* fd)
: m_fd {fd}, m_owned {false}, name {"<unnamed>.fala"} {}

FileReader::~FileReader() {
	if (m_owned && m_fd != nullptr) fclose(m_fd);
}

FileReader::FileReader(FileReader&& other) {
	m_owned = other.m_owned;
	other.m_owned = false;
	m_fd = other.m_fd;
}

std::string FileReader::get_path() const { return name; }
bool FileReader::at_eof() const { return feof(m_fd); }
bool FileReader::is_interactive() const { return false; }

size_t FileReader::read_at_most(char* buffer, size_t limit) {
	return fread(buffer, sizeof(char), limit, m_fd);
}
