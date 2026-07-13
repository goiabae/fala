#ifndef VARIABLE_HPP
#define VARIABLE_HPP

#include <cstddef>
#include <memory>

template<typename T>
struct Variable {
 public:
	Variable(std::size_t index) : m_index(index), m_bound_value(nullptr) {}

 private:
	std::size_t m_index;
	std::shared_ptr<T> m_bound_value;

 public:
	void bind_to(const std::shared_ptr<T>& value) { m_bound_value = value; }
	bool is_bound() const { return m_bound_value != nullptr; }
	std::shared_ptr<T> get_bound() const { return m_bound_value; }
	std::size_t get_index() const { return m_index; }
};

#endif
