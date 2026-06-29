#include "type.hpp"

std::ostream& operator<<(std::ostream& st, const Type& t) {
	t.print(st);
	return st;
}

TYPE get_datatype(TYPE t) {
	if (auto t2 = std::dynamic_pointer_cast<Integer>(t)) {
		return t;
	} else if (auto t2 = std::dynamic_pointer_cast<Nil>(t)) {
		return t;
	} else if (auto t2 = std::dynamic_pointer_cast<Bool>(t)) {
		return t;
	} else if (auto t2 = std::dynamic_pointer_cast<Void>(t)) {
		return t;
	} else if (auto t2 = std::dynamic_pointer_cast<Array>(t)) {
		return t;
	} else if (auto t2 = std::dynamic_pointer_cast<Function>(t)) {
		return t;
	} else if (auto t2 = std::dynamic_pointer_cast<Ref>(t)) {
		return t2->ref_type;
	} else if (auto t2 = std::dynamic_pointer_cast<Toat>(t)) {
		throw std::runtime_error("The type of all types is not a data type");
	} else if (auto t2 = std::dynamic_pointer_cast<General>(t)) {
		throw std::runtime_error("Generalized type is not a concrete data type");
	} else if (auto t2 = std::dynamic_pointer_cast<TypeVariable>(t)) {
		if (not t2->is_bound)
			throw std::runtime_error("type variable was not bound");
		return t2->bound_type;
	} else {
		assert(false && "no more variants of type!!!");
	}
}
