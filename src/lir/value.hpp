#ifndef LIR_VALUE_HPP
#define LIR_VALUE_HPP

#include <cstdint>
#include <format>
#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include "fixed_vector.hpp"

namespace lir::value {

class Integer {
 public:
	Integer(int64_t integer) : m_data(integer) {}

	operator int64_t const&() const { return m_data; }
	operator int64_t&() { return m_data; }

 private:
	int64_t m_data;
};

template<typename Value>
class PointerImpl;

template<typename Value>
PointerImpl<Value> clone_pointer(PointerImpl<Value>);

template<typename Value>
class PointerImpl {
 public:
	PointerImpl(Value* pointer, std::size_t size)
	: m_pointer(pointer), m_size(size) {}

	PointerImpl operator+(std::size_t offset) {
		if (offset >= m_size)
			throw std::runtime_error(
				std::format(
					"out of bounds pointer access (size={}, offset={})", m_size, offset
				)
			);
		return PointerImpl(&m_pointer[offset], m_size - offset);
	}
	PointerImpl operator[](std::size_t offset) { return *this + offset; }
	PointerImpl operator&() { return *this; }
	Value& operator*() { return *m_pointer; }
	std::size_t size() const { return m_size; }
	void* address() const { return m_pointer; }

	friend PointerImpl clone_pointer<Value>(PointerImpl);

 private:
	Value* m_pointer;
	std::size_t m_size;
};

inline bool operator==(const Integer& x, int y) { return (int64_t)x == y; }

inline bool operator!=(const Integer& x, const Integer& y) {
	return (int64_t)x != (int64_t)y;
}

inline bool operator==(const Integer& x, const Integer& y) {
	return (int64_t)x == (int64_t)y;
}

struct Prompt {
	std::size_t program_counter;
	std::size_t control_stack_height;
	std::size_t data_stack_height;
	std::size_t prompt_stack_height;
};

template<typename Value>
struct DelimitedContinuationImpl {
	std::vector<Value> data_stack_suffix;
	std::vector<std::size_t> control_stack_suffix;
	std::vector<std::pair<std::size_t, Prompt>> prompt_stack_suffix;
	std::size_t program_counter;
};

class Value {
	using Pointer = PointerImpl<Value>;
	using DelimitedContinuation = DelimitedContinuationImpl<Value>;

 public:
	Value(int64_t integer) : data(Integer(integer)) {}
	Value(Value* pointer, std::size_t size) : data(Pointer(pointer, size)) {}
	Value(const Pointer& pointer) : data(pointer) {}
	Value(const Integer& integer) : data(integer) {}
	Value(const std::shared_ptr<DelimitedContinuation>& continuation)
	: data(continuation) {}
	Value() : data(std::monostate {}) {}

 public:
	bool is_integer() const { return std::holds_alternative<Integer>(data); }
	bool is_pointer() const { return std::holds_alternative<Pointer>(data); }
	bool is_undefined() const {
		return std::holds_alternative<std::monostate>(data);
	}
	bool is_continuation() const {
		return std::holds_alternative<std::shared_ptr<DelimitedContinuation>>(data);
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

	std::shared_ptr<DelimitedContinuation> as_continuation() const {
		return std::get<std::shared_ptr<DelimitedContinuation>>(data);
	}

 private:
	std::variant<
		Integer, Pointer, std::shared_ptr<DelimitedContinuation>, std::monostate>
		data;
};

template<typename Value>
PointerImpl<Value> clone_pointer(PointerImpl<Value> ptr) {
	auto a = new Value[ptr.m_size];
	for (auto i = 0ul; i < ptr.m_size; i++) a[i] = clone(ptr.m_pointer[i]);
	PointerImpl<Value> ptr2(a, ptr.m_size);
	return ptr2;
}

inline Value clone(Value val) {
	if (val.is_undefined()) {
		return Value();
	} else if (val.is_integer()) {
		return value::Integer(val.as_integer());
	} else if (val.is_pointer()) {
		return clone_pointer(val.as_pointer());
	} else {
		std::unreachable();
	}
}

using Pointer = PointerImpl<Value>;
using DelimitedContinuation = DelimitedContinuationImpl<Value>;

} // namespace lir::value

#endif
