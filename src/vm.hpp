// Bytecode Virtual Machine

#ifndef VM_HPP
#define VM_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <istream>
#include <stack>
#include <stdexcept>
#include <utility>
#include <variant>

#include "lir.hpp"

namespace lir {

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

 private:
	std::variant<Integer, Pointer, std::monostate> data;
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

struct VM {
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
