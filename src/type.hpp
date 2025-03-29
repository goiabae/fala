#ifndef TYPE_HPP
#define TYPE_HPP

#include <cassert>
#include <typeinfo>
#include <vector>

struct Type {
	virtual ~Type() {}

	virtual bool operator==(Type* other) {
		return (typeid(*this) == typeid(*other));
	}

	virtual size_t size_of() = 0;

 protected:
	Type() {}
};

bool equiv(Type* x, Type* y);

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
	Function(std::vector<Type*> inputs, Type* output)
	: inputs(inputs), output(output) {}
	std::vector<Type*> inputs;
	Type* output;

	bool operator==(Type* other) override {
		if (!Type::operator==(other)) return false;
		Function* other_func = (Function*)other;
		if (inputs.size() != other_func->inputs.size()) return false;

		for (std::size_t i = 0; i < inputs.size(); i++)
			if (!equiv(inputs[i], other_func->inputs[i])) return false;

		if (!equiv(output, other_func->output)) return false;

		return true;
	}

	// label to function?
	size_t size_of() override { return 1; }
};

struct TypeVariable : Type {
	TypeVariable(std::size_t name) : name(name) {}
	std::size_t name;

	bool operator==(Type*) override { return true; }

	size_t size_of() override { assert(false); }
};

// Array of Int 64, really
struct Array : Type {
	Array(Type* item_type) : item_type(item_type) {}
	Type* item_type;

	bool operator==(Type* other) override {
		return Type::operator==(other)
		   and (*this->item_type) == ((Array*)other)->item_type;
	}

	// pointer to beginning of allocated region
	size_t size_of() override { return 1; }
};

#endif
