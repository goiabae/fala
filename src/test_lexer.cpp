#include <gtest/gtest.h>

#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "string_reader.hpp"

using tk = yy::parser::token_type;

namespace {

std::vector<int> collect_tokens(std::string source) {
	Reader* reader = new StringReader(source);
	Lexer lexer {reader};
	Location loc;
	union TokenValue value;
	lexer.loc = &loc;
	lexer.value = &value;
	std::vector<int> tokens {};
	while (true) {
		auto tok = lexer.lex();
		if (tok == tk::YYEOF) break;
		tokens.push_back(tok);
	}
	return tokens;
}

} // namespace

TEST(LexerTest, empty_string) {
	std::string source = "";
	std::vector<int> actual = collect_tokens(source);
	std::vector<int> expected {};
	EXPECT_EQ(actual, expected);
}

TEST(LexerTest, some_tokens) {
	std::string source = "let var x = 3 in x";
	std::vector<int> actual = collect_tokens(source);
	std::vector<int> expected {
		tk::LET,
		tk::VAR,
		tk::ID,
		tk::EQ,
		tk::NUMBER,
		tk::IN,
		tk::ID,
	};
	EXPECT_EQ(actual, expected);
}
