#ifndef TYPE_HPP
#define TYPE_HPP

#include <cassert>
#include <memory>
#include <typeinfo>
#include <vector>

struct Type;

using TYPE = std::shared_ptr<Type>;

struct Type {
	virtual ~Type() {}

	virtual bool operator==(Type* other) {
		return (typeid(*this) == typeid(*other));
	}

	virtual size_t size_of() = 0;

 protected:
	Type() {}
};

enum Sign { SIGNED, UNSIGNED };

struct Integer : Type {
	Integer(int bit_count, Sign sign) : bit_count(bit_count), sign(sign) {}
	int bit_count;
	Sign sign;

	bool operator==(Type* other) override {
		return Type::operator==(other)
		   and this->bit_count == ((Integer*)other)->bit_count
		   and this->sign == ((Integer*)other)->sign;
	}

	size_t size_of() override { return 1; }
};

using INTEGER = std::shared_ptr<Integer>;

struct Nil : Type {
	size_t size_of() override { return 0; }
};

struct Bool : Type {
	size_t size_of() override { return 1; }
};

struct Void : Type {
	size_t size_of() override { return 0; }
};

struct Function : Type {
	Function(std::vector<TYPE> inputs, TYPE output)
	: inputs(inputs), output(output) {}
	std::vector<TYPE> inputs;
	TYPE output;

	// label to function?
	size_t size_of() override { return 1; }
};

using FUNCTION = std::shared_ptr<Function>;

struct TypeVariable : Type {
	TypeVariable(std::size_t name) : unbound_name(name) {}

	bool operator==(Type*) override { return true; }
	size_t size_of() override { assert(false); }

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
};

using ARRAY = std::shared_ptr<Array>;

#endif
