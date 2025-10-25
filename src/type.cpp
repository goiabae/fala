#include "type.hpp"

std::ostream& operator<<(std::ostream& st, const Type& t) {
	t.print(st);
	return st;
}
