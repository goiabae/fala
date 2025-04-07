#ifndef HIR_HPP
#define HIR_HPP

#include <cstdio>
#include <map>
#include <memory>
#include <vector>

#include "str_pool.h"

namespace hir {

class Code;

class Integer {
 public:
	int integer;
};

class String {
 public:
	StrID str_id;
};

class Register {
	bool m_contains_value;

 public:
	size_t id;
	bool contains_value();
	bool contains_pointer();

	Register() : m_contains_value(false), id(0) {}
	Register(size_t id) : m_contains_value(false), id(id) {}
	Register(size_t id, bool contains_value)
	: m_contains_value(contains_value), id(id) {}
};

class Block {
 public:
	std::shared_ptr<Code> body_code;
};

class Function {
 public:
	std::vector<Register> parameter_registers;
	Block body_block;
	bool is_builtin;
	StrID builtin_name;
};

class Boolean {
 public:
	bool boolean;
};

class Character {
 public:
	char character;
};

class Nil {};

class Operand {
 public:
	enum class Kind {
		INVALID, // Represents uninitialized/invalid operands

		INTEGER,
		STRING,
		REGISTER,
		FUNCTION,
		BOOLEAN,
		CHARACTER,
		BLOCK,
		NIL,
	};

	Integer integer;
	String string;
	Register registuhr;
	Function function;
	Boolean boolean;
	Character character;
	Block block;
	Nil nil;

	Kind kind;

	Operand(Integer i) : integer(i), kind(Kind::INTEGER) {}
	Operand(String s) : string(s), kind(Kind::STRING) {}
	Operand(Register r) : registuhr(r), kind(Kind::REGISTER) {}
	Operand(Function f) : function(f), kind(Kind::FUNCTION) {}
	Operand(Boolean b) : boolean(b), kind(Kind::BOOLEAN) {}
	Operand(Character c) : character(c), kind(Kind::CHARACTER) {}
	Operand(Block b) : block(b), kind(Kind::BLOCK) {}
	Operand(Nil n) : nil(n), kind(Kind::NIL) {}

	Operand() : kind(Kind::INVALID) {}
};

enum class Opcode {
	PRINT,
	READ,
	COPY,
	ADD,
	SUB,
	MUL,
	DIV,
	MOD,
	NOT,
	OR,
	AND,
	EQ,
	NEQ,
	LESS,
	LESS_EQ,
	GREATER,
	GREATER_EQ,
	ELEMENT_PTR, // returns pointer to the nth item of some size from base
	CALL,
	BUILTIN, // move builtin function with name $2 to $1 register
	RET,
	REFTO,
	ALLOC,
	LOAD,
	STORE,
	LOOP,
	IF_TRUE,
	IF_FALSE,
	BREAK,
	CONTINUE,
};

class Instruction {
 public:
	Opcode opcode;
	std::vector<Operand> operands;
};

class Code {
 public:
	std::vector<Instruction> instructions;

	void copy(hir::Register, hir::Operand);
	void call(hir::Register, hir::Register, std::vector<hir::Operand>);
	void builtin(hir::Register, hir::Operand);
	void if_false(hir::Register, hir::Block);
	void if_true(hir::Register, hir::Block);
	void loop(hir::Block);
	void brake();
	void equals(hir::Register, hir::Operand, hir::Operand);
	void inc(hir::Register);
	void store(hir::Register, hir::Register);
	void load(hir::Register, hir::Register);
	void element_ptr(hir::Register, hir::Register, hir::Operand, hir::Operand);
	void ref_to(hir::Register, hir::Register);
};

Code operator+(Code pre, Code post);

void print_code(FILE* fd, const Code& code, const StringPool& pool, int spaces);
} // namespace hir

#endif
