// Bytecode Virtual Machine

#ifndef VM_HPP
#define VM_HPP

#include <array>
#include <stack>

#include "lir.hpp"

namespace lir {

struct VM {
	bool should_print_result;

	std::array<int64_t, 2048> cells {};
	std::stack<int64_t> stack {};

	void run(const Chunk&);
};

} // namespace lir

#endif
