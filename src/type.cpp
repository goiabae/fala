#include "type.hpp"

#include <variant>

std::ostream& operator<<(std::ostream& st, const Datatype& t) {
	t.print(st);
	return st;
}

std::ostream& operator<<(std::ostream& st, const Mode& m) {
	if (std::holds_alternative<ConcreteMode>(m.data)) {
		auto cm = std::get<ConcreteMode>(m.data);
		switch (cm) {
			case ConcreteMode::VAL: st << "val"; break;
			case ConcreteMode::VAR: st << "var"; break;
			case ConcreteMode::OUT: st << "out"; break;
		}
	} else {
		auto mv = std::get<Variable<Mode>>(m.data);
		if (mv.is_bound()) {
			auto bv = mv.get_bound();
			st << *bv;
		} else {
			st << 'm' << mv.get_index();
		}
	}
	return st;
}

std::ostream& operator<<(std::ostream& st, const Type& t) {
	st << *t.mode << ' ' << *t.datatype;
	return st;
}

std::shared_ptr<Datatype> get_datatype(std::shared_ptr<Type> t1) {
	auto t = t1->datatype;
	if (auto t2 = std::dynamic_pointer_cast<Integer>(t)) {
		return t;
	} else if (auto t2 = std::dynamic_pointer_cast<Nil>(t)) {
		return t;
	} else if (auto t2 = std::dynamic_pointer_cast<Void>(t)) {
		return t;
	} else if (auto t2 = std::dynamic_pointer_cast<Array>(t)) {
		return t;
	} else if (auto t2 = std::dynamic_pointer_cast<Function>(t)) {
		return t;
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

std::optional<ConcreteMode> Mode::to_concrete() {
	if (std::holds_alternative<ConcreteMode>(data))
		return std::get<ConcreteMode>(data);
	else {
		auto v = std::get<Variable<Mode>>(data);
		if (not v.is_bound())
			throw std::runtime_error("mode variable was not bound");
		else
			return v.get_bound()->to_concrete();
	}
}
