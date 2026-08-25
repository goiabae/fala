#ifndef INDEX_HPP
#define INDEX_HPP

#include <functional>
#include <type_traits>

template<typename Of>
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

template<typename Of>
bool operator==(const Index<Of>& a, const Index<Of>& b) {
	return a.index == b.index;
}

template<typename Of>
bool operator<(const Index<Of>& a, const Index<Of>& b) {
	return a.index < b.index;
}

namespace std {

template<typename Of>
struct hash<Index<Of>> {
	size_t operator()(const Index<Of>& p) const {
		return hash<typename Index<Of>::index_type> {}(p.index);
	}
};

} // namespace std

class ___Marker {};
static_assert(std::is_default_constructible<Index<___Marker>>());

#endif
