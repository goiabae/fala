#ifndef FILE_HPP
#define FILE_HPP

#include <stdio.h>

#include <string>

struct File;

#ifdef __cplusplus
struct File {
	File(const char* path, const char* mode)
	: m_fd {fopen(path, mode)}, name {path} {}
	File(FILE* fd) : m_fd {fd}, m_owned {false}, name {"<unnamed>.fala"} {}
	~File() {
		if (m_owned && m_fd != nullptr) fclose(m_fd);
	}

	File& operator=(const File& other) = delete;
	File& operator=(File&& other) = delete;
	File(const File& other) = delete;
	File(File&& other) {
		m_owned = other.m_owned;
		other.m_owned = false;
		m_fd = other.m_fd;
	}

	bool operator!() { return !m_fd; }

	FILE* get_descriptor() { return m_fd; }
	bool at_eof() { return feof(m_fd); }

	std::string get_name() { return name; }

 private:
	FILE* m_fd;
	bool m_owned {true};
	std::string name {};
};
#endif

#endif
