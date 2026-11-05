#ifndef COUNTER_HPP
#define COUNTER_HPP

#include <cstddef>

template<class Wrapper>
class Counter {
 public:
	template<typename... Args>
	Wrapper next(Args... args) {
		return Wrapper {m_next_index++, args...};
	}

	std::size_t count() const { return m_next_index; }

 private:
	std::size_t m_next_index;
};

#endif
