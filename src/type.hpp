#ifndef TYPE_HPP
#define TYPE_HPP

#include <cassert>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "utils.hpp"
#include "variable.hpp"

struct Type;
struct Datatype;

std::ostream& operator<<(std::ostream& st, const Type& t);
std::ostream& operator<<(std::ostream& st, const Datatype& t);

using DATATYPE = std::shared_ptr<Datatype>;

struct Datatype {
	virtual ~Datatype() {}
	virtual size_t size_of() = 0;
	virtual std::ostream& print(std::ostream&) const = 0;

 protected:
	Datatype() {}
};

enum Sign { SIGNED, UNSIGNED };

struct Integer : Datatype {
	Integer(int bit_count, Sign sign) : bit_count(bit_count), sign(sign) {}
	int bit_count;
	Sign sign;

	size_t size_of() override { return 1; }

	std::ostream& print(std::ostream& st) const override {
		if (sign == SIGNED)
			st << "Int<" << bit_count << ">";
		else
			st << "UInt<" << bit_count << ">";
		return st;
	}
};

using INTEGER = std::shared_ptr<Integer>;

struct Nil : Datatype {
	size_t size_of() override { return 0; }
	std::ostream& print(std::ostream& st) const override {
		st << "Nil";
		return st;
	}
};

struct Void : Datatype {
	size_t size_of() override { return 0; }
	std::ostream& print(std::ostream& st) const override {
		st << "Void";
		return st;
	}
};

struct Function : Datatype {
	Function(std::vector<std::shared_ptr<Type>> inputs, DATATYPE output)
	: inputs(inputs), output(output) {}
	std::vector<std::shared_ptr<Type>> inputs;
	DATATYPE output;

	// label to function?
	size_t size_of() override { return 1; }
	std::ostream& print(std::ostream& st) const override {
		st << "(" << SeparatedPrinter(inputs, ", ") << ") -> " << *output;
		return st;
	}
};

using FUNCTION = std::shared_ptr<Function>;

struct TypeVariable : Datatype {
	TypeVariable(std::size_t name) : unbound_name(name) {}

	size_t size_of() override {
		if (is_bound)
			return bound_type->size_of();
		else
			assert(false);
	}

	std::ostream& print(std::ostream& st) const override {
		if (is_bound) {
			st << "(" << "t" << "vt->unbound_name" << " := " << *bound_type << ")";
		} else {
			st << "'t" << unbound_name;
		}
		return st;
	}

	void bind_to(DATATYPE t) {
		bound_type = t;
		is_bound = true;
	}

	std::size_t unbound_name {};
	DATATYPE bound_type {nullptr};
	bool is_bound {false};
};

using TYPE_VARIABLE = std::shared_ptr<TypeVariable>;

// Array of Int 64, really
struct Array : Datatype {
	Array(DATATYPE item_type) : item_type(item_type) {}
	DATATYPE item_type;

	// pointer to beginning of allocated region
	size_t size_of() override { return 1; }
	std::ostream& print(std::ostream& st) const override {
		st << "Array<" << *item_type << ">";
		return st;
	}
};

using ARRAY = std::shared_ptr<Array>;

// Type of all types
struct Toat : Datatype {
	size_t size_of() override { assert(false); }
	std::ostream& print(std::ostream&) const override {
		throw std::domain_error("can't print toat");
	}
};

using TOAT = std::shared_ptr<Toat>;

struct General : Datatype {
	General(std::vector<DATATYPE> vars, DATATYPE body) : vars(vars), body(body) {}
	std::vector<DATATYPE> vars;
	DATATYPE body;

	size_t size_of() override { assert(false); }
	std::ostream& print(std::ostream&) const override {
		throw std::domain_error("can't print general type");
	}
};

using GENERAL = std::shared_ptr<General>;

enum class ConcreteMode {
	VAL,
	VAR,
	OUT,
};

struct Mode {
	std::variant<ConcreteMode, Variable<Mode>> data;
	std::optional<ConcreteMode> to_concrete();
};

struct Type {
	std::shared_ptr<Mode> mode;
	std::shared_ptr<Datatype> datatype;
};

std::shared_ptr<Datatype> get_datatype(std::shared_ptr<Type>);

#endif
