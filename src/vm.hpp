// Bytecode Virtual Machine

#ifndef VM_HPP
#define VM_HPP

#include <array>
#include <istream>
#include <stack>

#include "lir.hpp"

namespace lir {

struct VM {
	VM(std::istream& input, std::ostream& output)
	: input {input}, output {output} {}

	std::istream& input;
	std::ostream& output;

	bool should_print_result {true};

	std::array<int64_t, 2048> cells {};
	std::stack<int64_t> stack {};

	void run(const Chunk&);
};

} // namespace lir

#endif
