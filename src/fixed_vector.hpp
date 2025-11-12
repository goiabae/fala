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

 public:
	FixedVector() : m_data {nullptr}, m_size {0} {}

	FixedVector(std::initializer_list<T> init) {
		m_data = std::make_unique<T[]>(init.size());
		m_size = init.size();
		std::ranges::copy(init, m_data.get());
	}

	FixedVector(std::size_t count, const T& value) {
		m_data = std::make_unique<T[]>(count);
		for (auto i = 0ul; i < count; i++) m_data.get()[i] = value;
		m_size = count;
	}

	FixedVector(const FixedVector<T>& other) {
		m_data = std::make_unique<T[]>(other.size());
		m_size = other.size();
		std::ranges::copy(other, m_data.get());
	}

	FixedVector(FixedVector<T>&& other)
	: m_data(std::move(other.m_data)), m_size(std::move(other.m_size)) {
		other.m_size = 0;
	}

	FixedVector<T>& operator=(const FixedVector<T>& other) {
		if (other.size() != m_size)
			throw std::runtime_error("assignment to vector of different size");
		std::ranges::copy(other, std::begin(*m_data));
		return *this;
	}

	FixedVector<T>& operator=(FixedVector<T>&& other) {
		if (other.size() != m_size)
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
};

static_assert(std::is_default_constructible<FixedVector<int>>());
static_assert(std::is_copy_constructible<FixedVector<int>>());
static_assert(std::is_copy_assignable<FixedVector<int>>());
static_assert(std::is_move_constructible<FixedVector<int>>());
static_assert(std::is_move_assignable<FixedVector<int>>());
static_assert(std::ranges::range<FixedVector<int>>);

#endif
