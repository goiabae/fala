#include <gtest/gtest.h>

#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "string_reader.hpp"

using tk = yy::parser::token_type;

namespace {

std::vector<int> collect_tokens(std::string source) {
	Reader* reader = new StringReader(source);
	Lexer lexer {reader, true};
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
	std::vector<int> expected {tk::START_REPL};
	EXPECT_EQ(actual, expected);
}

TEST(LexerTest, some_tokens) {
	const std::string source =
		"let var x = 3 in let fun f (out y) = y = 4 in f x";
	const std::vector<int> actual = collect_tokens(source);
	const std::vector<int> expected {
		tk::START_REPL, tk::LET, tk::VAR,    tk::ID,          tk::EQ,
		tk::NUMBER,     tk::IN,  tk::LET,    tk::FUN,         tk::ID,
		tk::PAREN_OPEN, tk::OUT, tk::ID,     tk::PAREN_CLOSE, tk::EQ,
		tk::ID,         tk::EQ,  tk::NUMBER, tk::IN,          tk::ID,
		tk::ID,
	};
	EXPECT_EQ(actual, expected);
}
