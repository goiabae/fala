#ifndef FIXED_VECTOR_HPP
#define FIXED_VECTOR_HPP

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <type_traits>

template<typename T>
class FixedVector {
#if 0
 public:
	FixedVector();
	FixedVector(std::initializer_list<T> init);
	FixedVector(std::size_t count, const T& value);
	FixedVector(std::size_t count);

	FixedVector(const FixedVector<T>& other);
	FixedVector(FixedVector<T>&& other);

	FixedVector<T>& operator=(const FixedVector<T>& other);
	FixedVector<T>& operator=(FixedVector<T>&& other);

	T& operator[](std::size_t i);
	const T& operator[](std::size_t i) const;

	using iterator = T*;
	using const_iterator = T const*;

	iterator begin();
	const_iterator begin() const;
	const_iterator cbegin() const noexcept;

	iterator end();
	const_iterator end() const;
	const_iterator cend() const noexcept;

	std::size_t size() const noexcept;
#endif

	/// IMPLEMENTATION

 private:
	std::unique_ptr<T[]> m_data {};
	std::size_t m_size {};
	std::size_t m_cap {};

 public:
	FixedVector() : m_data {nullptr}, m_size {0}, m_cap {0} {}

	FixedVector(std::initializer_list<T> init) {
		m_data = std::make_unique<T[]>(init.size());
		m_size = init.size();
		m_cap = init.size();
		std::ranges::copy(init, m_data.get());
	}

	FixedVector(std::size_t count, const T& value) {
		m_data = std::make_unique<T[]>(count);
		for (auto i = 0ul; i < count; i++) m_data.get()[i] = value;
		m_size = count;
		m_cap = count;
	}

	FixedVector(std::size_t count) {
		m_data = std::make_unique<T[]>(count);
		for (auto i = 0ul; i < count; i++) m_data.get()[i] = {};
		m_size = 0;
		m_cap = count;
	}

	FixedVector(const FixedVector<T>& other) {
		m_data = std::make_unique<T[]>(other.size());
		m_size = other.size();
		m_cap = other.m_cap;
		std::ranges::copy(other, m_data.get());
	}

	FixedVector(FixedVector<T>&& other)
	: m_data(std::move(other.m_data)),
		m_size(std::move(other.m_size)),
		m_cap(std::move(other.m_cap)) {
		other.m_size = 0;
		other.m_cap = 0;
	}

	FixedVector<T>& operator=(const FixedVector<T>& other) {
		if (other.m_cap != m_cap)
			throw std::runtime_error("assignment to vector of different size");
		std::ranges::copy(other.begin(), other.end(), m_data.get());
		m_size = other.m_size;
		return *this;
	}

	FixedVector<T>& operator=(FixedVector<T>&& other) {
		if (other.m_cap != m_cap)
			throw std::runtime_error("assignment to vector of different size");
		m_data = std::move(other.m_data);
		other.m_size = 0;
		return *this;
	}

	T& operator[](std::size_t i) {
		if (i > (size() - 1)) std::runtime_error("index out of bounds");
		return m_data.get()[i];
	}

	const T& operator[](std::size_t i) const {
		return const_cast<FixedVector<T>&&>(*this)[i];
	}

	T& at(std::size_t i) {
		if (i > (size() - 1)) std::runtime_error("index out of bounds");
		return m_data.get()[i];
	}

	const T& at(std::size_t i) const {
		return const_cast<FixedVector<T>&&>(*this)[i];
	}

	using iterator = T*;
	using const_iterator = T const*;

	iterator begin() { return m_data.get(); }

	const_iterator begin() const {
		return const_cast<FixedVector<T>&&>(*this).begin();
	}

	const_iterator cbegin() const noexcept {
		return const_cast<FixedVector<T>&&>(*this).begin();
	}

	iterator end() { return &(m_data.get())[m_size]; }

	const_iterator end() const {
		return const_cast<FixedVector<T>&&>(*this).end();
	}

	const_iterator cend() const noexcept {
		return const_cast<FixedVector<T>&&>(*this).end();
	}

	std::size_t size() const noexcept { return m_size; }

	std::size_t capacity() const noexcept { return m_cap; }

	void push(const T& value) {
		if (m_size == m_cap)
			throw std::runtime_error("vector full. cant push element");
		m_data[m_size++] = value;
	}

	T pop() {
		if (m_size == 0) throw std::runtime_error("vector empty. cant pop element");
		auto v = m_data[m_size - 1];
		m_size--;
		return v;
	}

	T back() {
		if (m_size == 0) throw std::runtime_error("vector empty. cant get back");
		return m_data[m_size - 1];
	}
};

static_assert(std::is_default_constructible<FixedVector<int>>());
static_assert(std::is_copy_constructible<FixedVector<int>>());
static_assert(std::is_copy_assignable<FixedVector<int>>());
static_assert(std::is_move_constructible<FixedVector<int>>());
static_assert(std::is_move_assignable<FixedVector<int>>());
static_assert(std::ranges::range<FixedVector<int>>);

#endif
