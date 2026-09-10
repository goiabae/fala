// Bytecode Virtual Machine

#ifndef VM_HPP
#define VM_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <istream>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <variant>

#include "fixed_vector.hpp"
#include "lir/lir.hpp"
#include "options.hpp"

namespace lir {

struct Continuation;

class Value {
 public:
	class Integer {
	 public:
		Integer(int64_t integer) : m_data(integer) {}

		operator int64_t const&() const { return m_data; }
		operator int64_t&() { return m_data; }

	 private:
		int64_t m_data;
	};

	class Pointer {
	 public:
		Pointer(Value* pointer, std::size_t size)
		: m_pointer(pointer), m_size(size) {}

		Pointer operator+(std::size_t offset) {
			if (offset >= m_size)
				throw std::runtime_error(
					std::format(
						"out of bounds pointer access (size={}, offset={})", m_size, offset
					)
				);
			return Pointer(&m_pointer[offset], m_size - offset);
		}
		Pointer operator[](std::size_t offset) { return *this + offset; }
		Pointer operator&() { return *this; }
		Value& operator*() { return *m_pointer; }
		std::size_t size() const { return m_size; }
		void* address() const { return m_pointer; }

		friend Pointer clone_pointer(Pointer);

	 private:
		Value* m_pointer;
		std::size_t m_size;
	};

 public:
	Value(int64_t integer) : data(Integer(integer)) {}
	Value(Value* pointer, std::size_t size) : data(Pointer(pointer, size)) {}
	Value(const Pointer& pointer) : data(pointer) {}
	Value(const Integer& integer) : data(integer) {}
	Value(const std::shared_ptr<Continuation>& continuation)
	: data(continuation) {}
	Value() : data(std::monostate {}) {}

 public:
	bool is_integer() const { return std::holds_alternative<Integer>(data); }
	bool is_pointer() const { return std::holds_alternative<Pointer>(data); }
	bool is_undefined() const {
		return std::holds_alternative<std::monostate>(data);
	}

	Integer const& as_integer() const {
		if (is_pointer())
			throw std::runtime_error("expected integer, but was pointer");
		if (is_undefined())
			throw std::runtime_error("expected integer, but was monostate");
		return std::get<Integer>(data);
	}

	Integer& as_integer() {
		return const_cast<Integer&>(std::as_const(*this).as_integer());
	}

	Pointer const& as_pointer() const {
		if (is_integer())
			throw std::runtime_error("expected pointer, but was integer");
		if (is_undefined())
			throw std::runtime_error("expected pointer, but was monostate");
		return std::get<Pointer>(data);
	}

	Pointer& as_pointer() {
		return const_cast<Pointer&>(std::as_const(*this).as_pointer());
	}

	std::shared_ptr<Continuation> as_continuation() const {
		return std::get<std::shared_ptr<Continuation>>(data);
	}

 private:
	std::variant<Integer, Pointer, std::shared_ptr<Continuation>, std::monostate>
		data;
};

inline bool operator==(const Value::Integer& x, int y) {
	return (int64_t)x == y;
}

inline bool operator!=(const Value::Integer& x, const Value::Integer& y) {
	return (int64_t)x != (int64_t)y;
}

inline bool operator==(const Value::Integer& x, const Value::Integer& y) {
	return (int64_t)x == (int64_t)y;
}

static constexpr const auto register_count = 2048;
static constexpr const auto stack_size = 4 * 1024;

struct Continuation {
	std::array<Value, register_count> cells;
	FixedVector<Value> stack;
	std::vector<std::size_t> callstack;
	std::vector<std::pair<std::size_t, std::shared_ptr<Continuation>>>
		prompt_tags;
	std::size_t program_counter;
	std::size_t return_address;
};

struct VM {
	VM(std::istream& input, std::ostream& output, const Options& opts)
	: input {input}, output {output}, opts {opts}, stack(stack_size) {}

	std::istream& input;
	std::ostream& output;
	const Options& opts;

	bool should_print_result {true};

	void install_watch_point(lir::Register);
	std::array<Value, register_count> cells {};
	FixedVector<Value> stack;
	std::vector<std::size_t> callstack {};
	std::vector<std::pair<std::size_t, std::shared_ptr<Continuation>>>
		prompt_tags {};
	size_t program_counter {0};
	std::size_t return_address {0};
	std::string serialize(Value);

	void run(const Chunk&);
	std::size_t address_of_label(const Chunk&, lir::Label);
	Value& deref(lir::Operand);
	Value fetch(lir::Operand);
	Continuation save_continuation();
	void restore_continuation(const Continuation&);
	std::optional<Continuation> find_handler(std::size_t tag);
	void write(lir::Register, Value);
	std::unordered_set<std::size_t> watching_registers;

 private:
	void print_debug_stack();
	void print_debug_callstack(const Chunk&);
	void print_continuation(const Chunk&, const Continuation&);
};

} // namespace lir

#endif
