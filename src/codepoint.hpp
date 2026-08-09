#ifndef CODEPOINT_HPP
#define CODEPOINT_HPP

#include <cstdint>
#include <stdexcept>
#include <string>

using Codepoint = std::uint32_t;

inline std::uint8_t byte(Codepoint codepoint, std::size_t n) {
	return (std::uint8_t)(codepoint >> (8 * (4 - n - 1)));
}

inline std::size_t size(Codepoint codepoint) {
	if ((byte(codepoint, 0) & 0b11111000) == 0b11110000) {
		return 4;
	} else if ((byte(codepoint, 1) & 0b11110000) == 0b11100000) {
		return 3;
	} else if ((byte(codepoint, 2) & 0b11100000) == 0b11000000) {
		return 2;
	} else if (((byte(codepoint, 3) & 0b10000000) == 0b00000000)) {
		return 1;
	} else {
		throw std::runtime_error("invalid codepoint");
	}
}

inline std::string to_bytestring(Codepoint codepoint) {
	auto bytes = size(codepoint);
	if (bytes == 1) {
		std::string s {};
		s += (char)(std::uint8_t)(codepoint >> (8 * 0));
		return s;
	} else if (bytes == 2) {
		std::string s {};
		s += (char)(std::uint8_t)(codepoint >> (8 * 1));
		s += (char)(std::uint8_t)(codepoint >> (8 * 0));
		return s;
	} else if (bytes == 3) {
		std::string s {};
		s += (char)(std::uint8_t)(codepoint >> (8 * 2));
		s += (char)(std::uint8_t)(codepoint >> (8 * 1));
		s += (char)(std::uint8_t)(codepoint >> (8 * 0));
		return s;
	} else if (bytes == 4) {
		std::string s {};
		s += (char)(std::uint8_t)(codepoint >> (8 * 3));
		s += (char)(std::uint8_t)(codepoint >> (8 * 2));
		s += (char)(std::uint8_t)(codepoint >> (8 * 1));
		s += (char)(std::uint8_t)(codepoint >> (8 * 0));
		return s;
	} else {
		throw std::runtime_error("invalid codepoint");
	}
}

inline char to_ascii(Codepoint codepoint) {
	if (size(codepoint) != 1)
		throw std::runtime_error(
			"could not convert utf8 codepoint to ascii, multibyte codepoint"
		);
	return (char)byte(codepoint, 3);
}

inline bool is_digit(Codepoint codepoint) {
	return size(codepoint) == 1 and to_ascii(codepoint) >= '0'
	   and to_ascii(codepoint) <= '9';
}

inline bool is_ascii_alpha(Codepoint codepoint) {
	if (size(codepoint) != 1) return false;
	const auto c = to_ascii(codepoint);
	return ('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z');
}

#endif
