#ifndef INDEX_HPP
#define INDEX_HPP

#include <type_traits>

template<int>
struct Index {
	using index_type = unsigned int;

	index_type index;

	Index& operator++() {
		index++;
		return *this;
	}

	explicit operator index_type() { return index; }

	Index operator++(int) {
		Index old = *this;
		operator++();
		return old;
	}
};

template<int N>
bool operator==(const Index<N>& a, const Index<N>& b) {
	return a.index == b.index;
}

static_assert(std::is_default_constructible<Index<0>>());

#endif
