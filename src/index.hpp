#ifndef INDEX_HPP
#define INDEX_HPP

#include <functional>
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

template<int N>
bool operator<(const Index<N>& a, const Index<N>& b) {
	return a.index < b.index;
}

namespace std {

template<int N>
struct hash<Index<N>> {
	size_t operator()(const Index<N>& p) const {
		return hash<typename Index<N>::index_type> {}(p.index);
	}
};

} // namespace std

static_assert(std::is_default_constructible<Index<0>>());

#endif
