#include "type.hpp"

#include <variant>

#include "utils.hpp"

std::ostream& operator<<(std::ostream& st, Mode::Concrete cm) {
	switch (cm) {
		case Mode::Concrete::VAL: st << "val"; break;
		case Mode::Concrete::VAR: st << "var"; break;
		case Mode::Concrete::OUT: st << "out"; break;
	}
	return st;
}

std::ostream& operator<<(std::ostream& st, const Datatype& t) {
	t.print(st);
	return st;
}

std::ostream& operator<<(std::ostream& st, const Mode& m) {
	if (std::holds_alternative<Mode::Concrete>(m.data)) {
		auto cm = std::get<Mode::Concrete>(m.data);
		st << cm;
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
	} else if (auto t2 = std::dynamic_pointer_cast<RowSet>(t)) {
		throw std::runtime_error("row set is not a concrete record");
	} else if (auto t2 = std::dynamic_pointer_cast<Record>(t)) {
		return t2;
	} else {
		assert(false && "no more variants of type!!!");
	}
}

std::optional<Mode::Concrete> Mode::to_concrete() {
	if (std::holds_alternative<Mode::Concrete>(data))
		return std::get<Mode::Concrete>(data);
	else {
		auto v = std::get<Variable<Mode>>(data);
		if (not v.is_bound())
			throw std::runtime_error("mode variable was not bound");
		else
			return v.get_bound()->to_concrete();
	}
}

std::ostream& Function::print(std::ostream& st) const {
	st << "(" << SeparatedPrinter(inputs, ", ") << ") -> " << *output;
	return st;
}

size_t Function::size_of() { return 1; }

size_t Void::size_of() { return 0; }

std::ostream& Void::print(std::ostream& st) const {
	st << "Void";
	return st;
}

size_t TypeVariable::size_of() {
	if (is_bound)
		return bound_type->size_of();
	else
		assert(false);
}

std::ostream& TypeVariable::print(std::ostream& st) const {
	if (is_bound) {
		st << "(" << "t" << "vt->unbound_name" << " := " << *bound_type << ")";
	} else {
		st << "'t" << unbound_name;
	}
	return st;
}

size_t Array::size_of() { return 1; }

std::ostream& Array::print(std::ostream& st) const {
	st << "Array<" << *item_type << ">";
	return st;
}

std::ostream& Integer::print(std::ostream& st) const {
	if (sign == Sign::SIGNED)
		st << "Int<" << bit_count << ">";
	else
		st << "UInt<" << bit_count << ">";
	return st;
}

size_t Integer::size_of() { return 1; }

size_t Toat::size_of() { assert(false); }

std::ostream& Toat::print(std::ostream&) const {
	throw std::domain_error("can't print toat");
}

size_t General::size_of() { assert(false); }

std::ostream& General::print(std::ostream&) const {
	throw std::domain_error("can't print general type");
}

size_t Record::size_of() { assert(false); }

std::ostream& Record::print(std::ostream& st) const {
	st << '{';
	for (auto i = 0ul; i < fields.size(); i++) {
		st << fields.at(i).first << ':';
		fields.at(i).second->print(st);
		if (i != (fields.size() - 1)) {
			st << ';';
		}
	}
	st << '}';
	return st;
}

size_t RowSet::size_of() { assert(false); }

std::ostream& RowSet::print(std::ostream& st) const {
	st << '{';
	for (const auto& [n, d] : rows) {
		st << n << ':';
		d->print(st);
		st << ';';
	}
	st << "...";
	st << '}';
	return st;
}

size_t Nil::size_of() { return 0; }

std::ostream& Nil::print(std::ostream& st) const {
	st << "Nil";
	return st;
}

void TypeVariable::bind_to(std::shared_ptr<Datatype> t) {
	bound_type = t;
	is_bound = true;
}
