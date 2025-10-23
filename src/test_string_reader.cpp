#include <gtest/gtest.h>

#include <cstring>
#include <memory>

#include "string_reader.hpp"

TEST(StringReaderTest, empty_string) {
	auto reader = std::make_shared<StringReader>("");
	EXPECT_EQ(reader->at_eof(), true);
	char buf[50];
	size_t read = reader->read_at_most(buf, 50);
	EXPECT_EQ(read, 0);
}

TEST(StringReaderTest, sample_string) {
	std::string source =
		"let val x = 3 in do\n"
		"\twrite_int x\n"
		"\twrite_str \"\\n\"\n"
		"end\n";
	EXPECT_EQ(source.size(), 53);
	auto reader = std::make_shared<StringReader>(source);
	char buf[51];
	memset(buf, '\0', 51);
	size_t read = reader->read_at_most(buf, 50);
	EXPECT_EQ(read, 50);
	EXPECT_STREQ(
		buf, "let val x = 3 in do\n\twrite_int x\n\twrite_str \"\\n\"\ne"
	);
	memset(buf, '\0', 51);
	read = reader->read_at_most(buf, 50);
	EXPECT_EQ(read, 3);
	EXPECT_STREQ(buf, "nd\n");
	EXPECT_EQ(reader->at_eof(), true);
}
