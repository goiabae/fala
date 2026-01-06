#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include <memory>
#include <string>
#include <vector>

template<typename R, typename F, typename S>
void print_with_sep(R&& r, F f, S sep) {
	for (auto it = r.begin(); it != r.end(); it++) {
		f(*it);
		if (std::next(it) != r.end()) sep();
	}
}

template<typename T>
struct SeparatedPrinter {
	std::vector<T> t;
	std::string sep;
};

template<typename T>
std::ostream& operator<<(std::ostream& st, SeparatedPrinter<T>&& sp) {
	for (auto it = sp.t.begin(); it != sp.t.end(); it++) {
		st << *it;
		if (std::next(it) != sp.t.end()) st << sp.sep;
	}
	return st;
}

template<typename T>
std::ostream& operator<<(
	std::ostream& st, SeparatedPrinter<std::shared_ptr<T>>&& sp
) {
	for (auto it = sp.t.begin(); it != sp.t.end(); it++) {
		st << **it;
		if (std::next(it) != sp.t.end()) st << sp.sep;
	}
	return st;
}

template<class... Ts>
struct overloaded : Ts... {
	using Ts::operator()...;
};

#define PARENS ()

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define EACH(M, ...) __VA_OPT__(EXPAND(EACH_AUX(M, __VA_ARGS__)))
#define EACH_AUX(M, X, ...) \
	M(X) __VA_OPT__(, ) __VA_OPT__(EACH_AGAIN PARENS(M, __VA_ARGS__))
#define EACH_AGAIN() EACH_AUX

#define ADD_REF(X) [&] X

#define MATCH(X, ...) std::visit(overloaded {EACH(ADD_REF, __VA_ARGS__)}, X)
#define TRY_MATCH(X, ...) \
	MATCH(X, __VA_ARGS__, (const auto& _) { assert(false); })

#endif
