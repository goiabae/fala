#include <gtest/gtest.h>

#include "codepoint.hpp"
#include "codepoint_decoder.hpp"

static std::string decode_and_reencode(std::string s) {
	CodepointDecoder decoder {};
	std::vector<Codepoint> codepoints {};
	for (auto c : s) {
		if (auto codepoint = decoder.push((std::uint8_t)c)) {
			codepoints.push_back(*codepoint);
		}
	}
	std::string encoded {};
	for (auto codepoint : codepoints) {
		encoded += to_bytestring(codepoint);
	}
	return encoded;
}

TEST(CodepointDecoder, just_ascii_characters) {
	std::string s {"the quick brown fox jumped over the lazy dog"};
	std::string got = decode_and_reencode(s);
	EXPECT_STREQ(s.c_str(), got.c_str());
}

TEST(CodepointDecoder, latin_with_diacritics) {
	std::string s {"aáàâãeéêiíoóôuúç"};
	std::string got = decode_and_reencode(s);
	EXPECT_STREQ(s.c_str(), got.c_str());
}

TEST(CodepointDecoder, cast_single_byte_codepoint_to_byte) {
	std::string s {"(a"};
	CodepointDecoder decoder {};
	std::vector<Codepoint> codepoints {};
	for (auto c : s) {
		if (auto p = decoder.push((std::uint8_t)c)) {
			codepoints.push_back(*p);
		}
	}
	EXPECT_EQ(codepoints.at(0), '(');
	EXPECT_EQ(codepoints.at(1), 'a');
}
