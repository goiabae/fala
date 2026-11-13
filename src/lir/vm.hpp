// Bytecode Virtual Machine

#ifndef VM_HPP
#define VM_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <istream>
#include <unordered_set>
#include <utility>

#include "fixed_vector.hpp"
#include "lir/lir.hpp"
#include "lir/value.hpp"
#include "options.hpp"

namespace lir {

static constexpr const auto register_count = 2048;
static constexpr const auto stack_size = 4 * 1024;

struct VM {
	VM(std::istream& input, std::ostream& output, const Options& opts)
	: input {input}, output {output}, opts {opts}, data_stack(stack_size) {}

 public:
	std::istream& input;
	std::ostream& output;
	const Options& opts;
	FixedVector<value::Value> data_stack;

 public:
	bool should_print_result {true};
	std::array<value::Value, register_count> cells {};
	std::vector<std::size_t> control_stack {};
	std::vector<std::pair<std::size_t, value::Prompt>> prompt_stack {};
	size_t program_counter {0};
	std::unordered_set<std::size_t> watching_registers;

 public:
	void install_watch_point(lir::Register);
	auto serialize(value::Value) -> std::string;
	void run(const Chunk&);
	auto address_of_label(const Chunk&, lir::Label) -> std::size_t;
	auto deref(lir::Operand) -> value::Value&;
	auto fetch(lir::Operand) -> value::Value;
	auto find_handler(std::size_t tag) -> std::optional<value::Prompt>;
	void write(lir::Register, value::Value);
	auto save_continuation(const value::Prompt&)
		-> std::shared_ptr<value::DelimitedContinuation>;
	void restore_continuation(const value::DelimitedContinuation&);
	void unwind_prompt(const value::Prompt&);

 private:
	void print_debug_stack();
	void print_debug_callstack(const Chunk&);
	void print_continuation(const Chunk&, const value::DelimitedContinuation&);
};

} // namespace lir

#endif
