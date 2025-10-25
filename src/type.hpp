#ifndef TYPE_HPP
#define TYPE_HPP

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <vector>

#include "utils.hpp"

struct Type;

using TYPE = std::shared_ptr<Type>;

struct Type {
	virtual ~Type() {}
	virtual size_t size_of() = 0;
	virtual std::ostream& print(std::ostream&) const = 0;

 protected:
	Type() {}
};

std::ostream& operator<<(std::ostream& st, const Type& t);

enum Sign { SIGNED, UNSIGNED };

struct Integer : Type {
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

struct Nil : Type {
	size_t size_of() override { return 0; }
	std::ostream& print(std::ostream& st) const override {
		st << "Nil";
		return st;
	}
};

struct Bool : Type {
	size_t size_of() override { return 1; }
	std::ostream& print(std::ostream& st) const override {
		st << "Bool";
		return st;
	}
};

struct Void : Type {
	size_t size_of() override { return 0; }
	std::ostream& print(std::ostream& st) const override {
		st << "Void";
		return st;
	}
};

struct Function : Type {
	Function(std::vector<TYPE> inputs, TYPE output)
	: inputs(inputs), output(output) {}
	std::vector<TYPE> inputs;
	TYPE output;

	// label to function?
	size_t size_of() override { return 1; }
	std::ostream& print(std::ostream& st) const override {
		st << "(" << SeparatedPrinter(inputs, ", ") << ") -> " << *output;
		return st;
	}
};

using FUNCTION = std::shared_ptr<Function>;

struct TypeVariable : Type {
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

	void bind_to(TYPE t) {
		bound_type = t;
		is_bound = true;
	}

	std::size_t unbound_name {};
	TYPE bound_type {nullptr};
	bool is_bound {false};
};

using TYPE_VARIABLE = std::shared_ptr<TypeVariable>;

// Array of Int 64, really
struct Array : Type {
	Array(TYPE item_type) : item_type(item_type) {}
	TYPE item_type;

	// pointer to beginning of allocated region
	size_t size_of() override { return 1; }
	std::ostream& print(std::ostream& st) const override {
		st << "Array<" << *item_type << ">";
		return st;
	}
};

using ARRAY = std::shared_ptr<Array>;

struct Ref : Type {
	Ref(TYPE ref_type) : ref_type(ref_type) {}
	size_t size_of() override { return ref_type->size_of(); }
	std::ostream& print(std::ostream& st) const override {
		st << "&" << *ref_type;
		return st;
	}

	TYPE ref_type;
};

using REF = std::shared_ptr<Ref>;

// Type of all types
struct Toat : Type {
	size_t size_of() override { assert(false); }
	std::ostream& print(std::ostream&) const override {
		throw std::domain_error("can't print toat");
	}
};

using TOAT = std::shared_ptr<Toat>;

struct General : Type {
	General(std::vector<TYPE> vars, TYPE body) : vars(vars), body(body) {}
	std::vector<TYPE> vars;
	TYPE body;

	size_t size_of() override { assert(false); }
	std::ostream& print(std::ostream&) const override {
		throw std::domain_error("can't print general type");
	}
};

using GENERAL = std::shared_ptr<General>;

#endif
