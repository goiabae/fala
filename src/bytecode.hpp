#ifndef BYTECODE_HPP
#define BYTECODE_HPP

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "ast.h"

namespace bytecode {

using Number = int;

enum class Opcode {
	PRINTF,
	PRINTV,
	READ,
	MOV,
	ADD,
	SUB,
	MUL,
	DIV,
	MOD,
	NOT,
	OR,
	AND,
	EQ,
	DIFF,
	LESS,
	LESS_EQ,
	GREATER,
	GREATER_EQ,
	LOAD,
	STORE,
	JMP,
	JMP_FALSE,
	JMP_TRUE,
};

struct Register {
	enum {
		VAL_NUM,  // register contains a plain number
		VAL_ADDR, // register contains an address of a r-register
	} type;
	size_t index;
	Register(size_t idx) : index {idx} {}
	Register& as_num() { return (type = VAL_NUM), *this; }
	Register& as_addr() { return (type = VAL_ADDR), *this; }
	bool has_num() const { return type == VAL_NUM; }
	bool has_addr() const { return type == VAL_ADDR; }
};

struct Funktion {
	size_t argc;
	Node* args;
	Node root;
};

struct Label {
	size_t id;
};

struct Operand {
	enum class Type {
		NIL, // no operand
		TMP, // temporary register
		REG, // variables register
		LAB, // label
		NUM, // immediate number
		FUN,
	};

	Type type;

	union {
		Register reg;
		Label lab;
		Number num;
		Funktion fun;
	};

	Operand() : type {Type::NIL}, num {0} {}
	Operand(Register reg) : type(Type::NIL), reg {reg} {}
	Operand(Label lab) : type(Type::LAB), lab {lab} {}
	constexpr Operand(Number num) : type(Type::NUM), num {num} {}
	Operand(Funktion fun) : type(Type::FUN), fun {fun} {}

	bool is_register() const { return type == Type::REG || type == Type::TMP; }

	Operand as_reg() { return (type = Type::REG), *this; }
	Operand as_temp() { return (type = Type::TMP), *this; }

	void deinit() { return; }
};

struct Instruction {
	Opcode opcode;
	Operand operands[3];
	std::string comment;
};

struct Chunk {
	Chunk& emit(Opcode, Operand fst = {}, Operand snd = {}, Operand trd = {});
	Chunk& with_comment(std::string comment);
	void add_label(Operand label);

	std::vector<Instruction> m_vec;
	std::map<size_t, size_t> label_indexes;
};

// return amount of operands of each opcode
size_t opcode_opnd_count(Opcode op);

void print_chunk(FILE*, const Chunk&);
int print_inst(FILE*, const Instruction& inst);

} // namespace bytecode

#endif
