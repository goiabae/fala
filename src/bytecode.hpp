#ifndef BYTECODE_HPP
#define BYTECODE_HPP

#include <vector>

namespace bytecode {
struct Code {};

struct Chunk {
	std::vector<Code> codes;
};
} // namespace bytecode

#endif
