#ifndef TYPE_HPP
#define TYPE_HPP

#include <typeinfo>
#include <vector>

struct Type {
	virtual ~Type() {}

	virtual bool operator==(Type* other) {
		return (typeid(*this) == typeid(*other));
	}

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
};

struct Nil : Type {};
struct Bool : Type {};
struct Void : Type {};

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
			if (!((*inputs[i]) == other_func->inputs[i])) return false;

		if (!((*output) == other_func->output)) return false;

		return true;
	}
};

struct TypeVariable : Type {
	TypeVariable(std::size_t name) : name(name) {}
	std::size_t name;

	bool operator==(Type*) override { return true; }
};

struct Array : Type {
	Array(Type* item_type) : item_type(item_type) {}
	Type* item_type;

	bool operator==(Type* other) override {
		return Type::operator==(other)
		   and (*this->item_type) == ((Array*)other)->item_type;
	}
};

#endif
