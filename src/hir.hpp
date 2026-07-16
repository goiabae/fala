#ifndef HIR_HPP
#define HIR_HPP

#include <cstddef>
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

struct Operand {
	enum class Kind {
		INVALID, // Represents uninitialized/invalid operands
		REGISTER,
		BLOCK,
		LABEL,
	};

	Register registuhr;
	Block block;
	Label label;
	Kind kind;

	Operand(Register r) : registuhr(r), kind(Kind::REGISTER) {}
	Operand(Block b) : block(b), kind(Kind::BLOCK) {}
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

	GET_GLOBAL,

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
	void if_false(hir::Register, hir::Block);
	void if_true(hir::Register, hir::Block);
	void loop(hir::Label next, hir::Label stop, hir::Block);
	void brake(hir::Label where_to);
	void equals(hir::Register, hir::Operand, hir::Operand);
	void set_element(hir::Register, std::vector<hir::Operand>, hir::Operand);
	void get_element(hir::Register, hir::Register, std::vector<hir::Operand>);
	void get_element(hir::Register, hir::Register, hir::Operand);
	void alloc(hir::Register, hir::Register);
	void clone(hir::Register, hir::Register);
	void alias(hir::Register, hir::Register);
	void load(hir::Register, hir::Register);
	void store(hir::Register, hir::Register);
	void add(hir::Register, hir::Operand, hir::Operand);
	void get_global(hir::Register, hir::Label);
};

using Constant = std::variant<
	hir::Integer, hir::String, hir::Character, hir::Nil, hir::Boolean,
	hir::Function>;

struct StaticAllocation {
	std::size_t size;
};

struct BuiltinLoad {
	std::string name;
};

using StaticInitialization =
	std::variant<Constant, StaticAllocation, BuiltinLoad>;

struct Module {
	std::map<std::string, StaticInitialization> static_symbols;

 public:
	void load_builtin(hir::Label name);
	hir::Label register_constant(hir::Label name, const Constant& constant);
};

Code operator+(Code pre, Code post);

void print_code(FILE* fd, const Code& code, const StringPool& pool, int spaces);
void print_module(
	FILE* fd, const Module& mod, const StringPool& pool, int spaces
);

} // namespace hir

#endif
