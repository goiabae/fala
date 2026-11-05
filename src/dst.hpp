#ifndef DST_HPP
#define DST_HPP

#include <concepts>
#include <map>
#include <type_traits>

namespace dst {

template<class B>
concept boolean_testable = std::convertible_to<B, bool>;

template<typename T, typename U>
concept strict_preorderable = requires(
	const std::remove_reference_t<T>& t, const std::remove_reference_t<U>& u
) {
	{ t < u } -> boolean_testable;
};

template<
	class Key, class T, class Compare = std::less<Key>,
	class Allocator = std::allocator<std::pair<const Key, T>>>
	requires(strict_preorderable<Key, Key>)
using map = std::map<Key, T, Compare, Allocator>;

} // namespace dst

#endif
