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

#endif
