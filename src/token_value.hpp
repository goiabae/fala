#ifndef TOKEN_VALUE_HPP
#define TOKEN_VALUE_HPP

#include "ast.hpp"

union TokenValue {
	int64_t num;
	char* str;
	char character;
	NodeIndex node;
};

#endif
