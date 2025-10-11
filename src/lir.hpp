#ifndef LIR_HPP
#define LIR_HPP

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ast.hpp"

namespace lir {

using Number = int;

enum class Opcode {
	PRINTF,
	PRINTV,
	PRINTC,
	READV,
	READC,
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
	PUSH,
	POP,
	CALL,
	RET,
	FUNC,
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

struct Function {
	size_t argc;
	Node* args;
	Node root;
};

struct Label {
	size_t id;
};

// simple wrapper around Register
struct Array {
	Register start_pointer_reg;
};

struct Operand {
	enum class Type {
		NIL, // no operand
		REG, // variables register
		LAB, // label
		NUM, // immediate number
		FUN,
		ARR, // array
	};

	Type type;
	std::variant<Register, Label, Number, Function, Array> data;

	Operand() : type {Type::NIL}, data {Number {0}} {}
	Operand(Register reg) : type(Type::REG), data {reg} {}
	Operand(Label lab) : type(Type::LAB), data {lab} {}
	constexpr Operand(Number num) : type(Type::NUM), data {num} {}
	Operand(Function fun) : type(Type::FUN), data {fun} {}
	Operand(Array arr) : type(Type::ARR), data {arr} {}

	Label& as_label() { return std::get<Label>(data); }
	Register& as_register() { return std::get<Register>(data); }
	Number& as_number() { return std::get<Number>(data); }
	Array& as_array() { return std::get<Array>(data); }

	const Label& as_label() const { return std::get<Label>(data); }
	const Register& as_register() const { return std::get<Register>(data); }
	const Number& as_number() const { return std::get<Number>(data); }
	const Array& as_array() const { return std::get<Array>(data); }

	void deinit() { return; }
};

#define INSTRUCTION_MAX_OPERANDS 3

struct Instruction {
	Opcode opcode;
	Operand operands[INSTRUCTION_MAX_OPERANDS];
	std::string comment;
};

struct Chunk {
	Chunk& emit(Opcode, Operand fst = {}, Operand snd = {}, Operand trd = {});
	Chunk& with_comment(std::string comment);
	void add_label(Operand label);

	std::vector<Instruction> m_vec;
	std::map<size_t, size_t> label_indexes;

	std::optional<Operand> result_opnd;
};

// return amount of operands of each opcode
size_t opcode_opnd_count(Opcode op);

void print_chunk(FILE*, const Chunk&);
int print_inst(FILE*, const Instruction& inst);

Chunk operator+(Chunk x, Chunk y);

const char* operand_type_repr(Operand::Type type);

} // namespace lir

#endif
