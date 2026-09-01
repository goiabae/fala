#include "type.hpp"

#include <memory>
#include <optional>
#include <ostream>
#include <ranges>
#include <sstream>
#include <variant>

#include "utils.hpp"

using M = std::shared_ptr<Mode>;
using D = std::shared_ptr<Datatype>;
using T = std::shared_ptr<Type>;
using E = std::shared_ptr<EffectSet>;

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

std::shared_ptr<Datatype> get_datatype(std::shared_ptr<Datatype> t) {
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
	} else if (auto t2 = std::dynamic_pointer_cast<TypeVariable>(t)) {
		if (not t2->is_bound)
			throw std::runtime_error("type variable was not bound");
		return t2->bound_type;
	} else if (auto t2 = std::dynamic_pointer_cast<RowSet>(t)) {
		if (t2->bound_record->has_value())
			return get_datatype(t2->bound_record->value());
		throw std::runtime_error("row set is not a concrete record");
	} else if (auto t2 = std::dynamic_pointer_cast<Record>(t)) {
		return t2;
	} else if (auto t2 = std::dynamic_pointer_cast<Closure>(t)) {
		return t2;
	} else if (auto t2 = std::dynamic_pointer_cast<Union>(t)) {
		return t2;
	} else {
		assert(false && "no more variants of type!!!");
	}
}

std::shared_ptr<Datatype> get_datatype(std::shared_ptr<Type> t1) {
	auto t = t1->datatype;
	return get_datatype(t);
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

std::ostream& operator<<(std::ostream& st, const Effect& eff) {
	return st << eff.name;
}

std::ostream& operator<<(std::ostream& st, const EffectSet& set) {
	for (auto eff : set.concrete) {
		st << " !" << *eff;
	}
	if (set.variable != nullptr) {
		if (set.variable->is_bound()) {
			st << *set.variable->get_bound();
		} else {
			st << " !e" << set.variable->get_index();
		}
	}
	return st;
}

std::ostream& Function::print(std::ostream& st) const {
	st << "(" << SeparatedPrinter(inputs, ", ") << ") -> " << *output;
	st << *effects;
	return st;
}

size_t Function::size_of() { assert(false && "not implemented yet"); }

std::ostream& Closure::print(std::ostream& st) const {
	st << '[';
	std::vector<std::string> as {};
	for (const auto& [k, v] : captures) {
		std::ostringstream ss {};
		ss << *v;
		as.push_back(ss.str());
	}
	st << SeparatedPrinter(as, ", ");
	st << ']';
	st << "(" << SeparatedPrinter(inputs, ", ") << ") -> " << *output;
	st << *effects;
	return st;
}

size_t Closure::size_of() { assert(false && "not implemented yet"); }

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
		assert(not is_generalized);
		// st << "(" << "t" << name << " := " << *bound_type << ")";
		st << *bound_type;
	} else if (is_generalized) {
		st << "'g" << name;
	} else {
		st << "'t" << name;
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
	if (bound_record->has_value()) return bound_record->value()->print(st);
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

size_t Union::size_of() { assert(false); }

std::ostream& Union::print(std::ostream& st) const {
	st << "{";
	for (const auto& [i, ctor] : std::ranges::views::enumerate(constructors)) {
		if (i != 0) st << "|";
		st << ' ' << ctor.name << '(';
		for (const auto& [j, argument] :
		     std::ranges::views::enumerate(ctor.arguments)) {
			st << *argument;
			if (static_cast<std::size_t>(j) != (ctor.arguments.size() - 1))
				st << ", ";
		}
		st << ')';
	}
	st << " }";
	return st;
}

void TypeVariable::bind_to(std::shared_ptr<Datatype> t) {
	bound_type = t;
	is_bound = true;
}

D make_row_set(
	std::map<std::string, std::shared_ptr<Datatype>> x,
	std::shared_ptr<std::optional<std::shared_ptr<Datatype>>> v
) {
	RowSet rs;
	rs.rows = x;
	rs.bound_record = v;
	return std::make_shared<RowSet>(rs);
}

D make_record(std::vector<std::pair<std::string, D>> field_tys) {
	Record r;
	r.fields = field_tys;
	return std::make_shared<Record>(r);
}

D make_bool() { return make_integer(1, Integer::Sign::UNSIGNED); }

M make_modevar(std::size_t index) {
	return std::make_shared<Mode>(Mode {.data = Variable<Mode>(index)});
}

D make_datavar(std::size_t index) {
	return std::make_shared<TypeVariable>(index, false);
}

D make_integer(int bit_count, Integer::Sign sign) {
	Integer i {};
	i.bit_count = bit_count;
	i.sign = sign;
	return std::make_shared<Integer>(i);
}

D make_array(D item_type) {
	Array a {};
	a.item_type = item_type;
	return std::make_shared<Array>(a);
}

D make_function(std::vector<T> inputs, D output, E effects) {
	auto x = std::make_shared<Function>();
	x->inputs = inputs;
	x->output = output;
	x->effects = effects;
	return x;
}

D make_closure(
	std::vector<T> inputs, D output, E effects, std::map<Guid, D> captures
) {
	auto x = std::make_shared<Closure>();
	x->inputs = inputs;
	x->output = output;
	x->effects = effects;
	x->captures = captures;
	return x;
}

T make_type(M mode, D datatype) {
	return std::make_shared<Type>(mode, datatype);
}

std::shared_ptr<DataScheme> make_scheme(
	std::vector<std::shared_ptr<TypeVariable>> quantified, D body
) {
	return std::make_shared<DataScheme>(quantified, body);
}

std::shared_ptr<PolyType> make_polytype(
	M mode, std::shared_ptr<DataScheme> scheme
) {
	return std::make_shared<PolyType>(mode, scheme);
}

std::ostream& operator<<(std::ostream& st, const PolyType& p) {
	return st << *p.mode << ' ' << *p.scheme;
}

std::ostream& operator<<(std::ostream& st, const DataScheme& s) {
	st << "∀";
	for (auto x : s.quantified) {
		st << *x << ' ';
	}
	st << '.';
	st << ' ';
	st << *s.body;
	return st;
}

std::shared_ptr<Function> erase_function_captures(D data) {
	if (auto x = std::dynamic_pointer_cast<Function>(data)) {
		return x;
	} else if (auto x = std::dynamic_pointer_cast<Closure>(data)) {
		return std::dynamic_pointer_cast<Function>(
			make_function(x->inputs, x->output, x->effects)
		);
	} else {
		throw std::runtime_error("not a function");
	}
}

E make_effsetvar(std::size_t index) {
	return std::make_shared<EffectSet>(index);
}

T as_monotype(std::shared_ptr<PolyType> p) {
	if (p->scheme->quantified.size() != 0)
		throw std::runtime_error(
			"can't cast polytype with quantified arguments to a monotype"
		);
	return make_type(p->mode, p->scheme->body);
}
