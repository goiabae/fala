// Bytecode Virtual Machine

#ifndef VM_HPP
#define VM_HPP

#include <array>
#include <istream>
#include <stack>

#include "lir.hpp"

namespace lir {

struct VM {
	class Value : public std::variant<int64_t, Value*> {
	 public:
		Value() : std::variant<int64_t, Value*>(0) {}
		Value(int64_t integer) : std::variant<int64_t, Value*>(integer) {}
		Value(Value* pointer) : std::variant<int64_t, Value*>(pointer) {}
	};

	VM(std::istream& input, std::ostream& output)
	: input {input}, output {output} {}

	std::istream& input;
	std::ostream& output;

	bool should_print_result {true};

	std::array<Value, 2048> cells {};
	std::stack<Value> stack {};

	void run(const Chunk&);
};

} // namespace lir

#endif
