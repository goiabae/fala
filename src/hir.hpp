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

struct Register {
	size_t id;
	std::string name;
	bool is_mutable;
};

class Label {
 public:
	std::string name;
};

class Block {
 public:
	std::shared_ptr<Code> body_code;
	std::vector<Label> child_blocks;
};

class Function {
 public:
	std::map<std::string, Block> blocks;
	std::string entry_block;
	std::vector<Register> parameter_registers;
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
		LABEL,
	};

	Integer integer;
	String string;
	Register registuhr;
	Function function;
	Boolean boolean;
	Character character;
	Block block;
	Nil nil;
	Label label;

	Kind kind;

	Operand(Integer i) : integer(i), kind(Kind::INTEGER) {}
	Operand(String s) : string(s), kind(Kind::STRING) {}
	Operand(Register r) : registuhr(r), kind(Kind::REGISTER) {}
	Operand(Function f) : function(f), kind(Kind::FUNCTION) {}
	Operand(Boolean b) : boolean(b), kind(Kind::BOOLEAN) {}
	Operand(Character c) : character(c), kind(Kind::CHARACTER) {}
	Operand(Block b) : block(b), kind(Kind::BLOCK) {}
	Operand(Nil n) : nil(n), kind(Kind::NIL) {}
	Operand(Label l) : label(l), kind(Kind::LABEL) {}

	Operand() : kind(Kind::INVALID) {}
};

enum class Opcode {

	COPY,

	// I/O
	PRINT,
	READ,

	// Unary and binary operators
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

	// Escape hatch into the compiler
	BUILTIN,

	// Control flow
	LOOP,
	IF_TRUE,
	IF_FALSE,
	BREAK,
	CONTINUE,
	CALL,
	RET,

	// Receives a register containing an aggregate value and a sequence of indexes
	// (registers or numeric literals). Sets or gets the element of the aggregate
	// at the position indicated by the indexes.
	SET_ELEMENT,
	GET_ELEMENT,

	// Given a reference, copy the value contained in the register pointed to by
	// the refence into another register
	LOAD,

	// Given a reference and a value, store the value into the register pointed to
	// by the reference
	STORE,

	ALLOC,
	CLONE,
	ALIAS,
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
	void builtin(hir::Register, hir::String);
	void if_false(hir::Register, hir::Block);
	void if_true(hir::Register, hir::Block);
	void loop(hir::Label next, hir::Label stop, hir::Block);
	void brake(hir::Label where_to);
	void equals(hir::Register, hir::Operand, hir::Operand);
	void inc(hir::Register);
	void set_element(hir::Register, std::vector<hir::Operand>, hir::Operand);
	void get_element(hir::Register, hir::Register, std::vector<hir::Operand>);
	void get_element(hir::Register, hir::Register, hir::Operand);
	void alloc(hir::Register, hir::Register);
	void clone(hir::Register, hir::Register);
	void alias(hir::Register, hir::Register);
	void load(hir::Register, hir::Register);
	void store(hir::Register, hir::Register);
	void add(hir::Register, hir::Operand, hir::Operand);
};

Code operator+(Code pre, Code post);

void print_code(FILE* fd, const Code& code, const StringPool& pool, int spaces);

} // namespace hir

#endif
