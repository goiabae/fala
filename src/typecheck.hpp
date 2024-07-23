#ifndef FALA_TYPECHECK_HPP
#define FALA_TYPECHECK_HPP

#include <utility>
#include <vector>

#include "ast.h"
#include "str_pool.h"

struct Type {
	virtual ~Type() {};
};

struct Concrete : Type {
	Concrete(size_t id) : id {id} {}
	size_t id;
};

struct Record : Type {
	std::vector<std::pair<StrID, Type*>> fields;
};

struct Slice : Type {
	Slice(Type* element) : element {element} {}
	Type* element;
};

struct TypeVar : Type {
	TypeVar(size_t id) : id {id} {}
	size_t id;
};

struct Poly : Type {
	TypeVar* var;
	Type* typ;
};

// nominal types in a structural type system
struct Unique : Type {
	size_t id;
	Type* type;
};

struct Typechecker {
	size_t next_var_id {0};
	size_t next_unique_id {0};
	size_t next_concrete_id {0};

	Typechecker() {
		ts.push_back(new Concrete {next_concrete_id++});
		ts.push_back(new Concrete {next_concrete_id++});
		ts.push_back(new Concrete {next_concrete_id++});
		ts.push_back(new Concrete {next_concrete_id++});
	}

	~Typechecker() {
		for (Type* t : ts) delete t;
	}

	Type* make_var() {
		auto t = new TypeVar {next_var_id++};
		ts.push_back(t);
		return t;
	}
	Type* make_unique(Type*);
	Type* make_slice(Type* t) {
		auto u = new Slice {t};
		ts.push_back(u);
		return u;
	}
	Type* make_concrete();

	Type* get_numeric() { return ts[0]; }
	Type* get_character() { return ts[1]; };
	Type* get_nil() { return ts[2]; }
	Type* get_bool() { return ts[3]; }

	std::vector<Type*> ts;
};

bool typecheck(const AST& ast);

#endif
