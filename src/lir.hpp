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

	// pointer operations
	ALLOCA,
	LOADA,
	STOREA,
};

struct Register {
	enum {
		VAL_NUM,  // register contains a plain number
		VAL_ADDR, // register contains an address of a r-register
	} type;
	size_t index;
	explicit Register(size_t idx) : index {idx} {}
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

struct Immediate {
	Immediate(int number) : number {number} {}
	int number;
};

struct Operand {
	enum class Type {
		NOTHING,   // no operand
		REGISTER,  // variables register
		LABEL,     // label
		IMMEDIATE, // immediate number
		FUN,
		ARR, // array
	};

	enum class DataType {
		INVALID,
		INTEGER,
		POINTER,
	};

	DataType data_type;
	Type type;
	std::variant<Register, Label, Immediate, Function, Array> data;

	Operand()
	: data_type {DataType::INVALID}, type {Type::NOTHING}, data {Immediate {0}} {}
	explicit Operand(Register reg)
	: data_type {DataType::INVALID}, type(Type::REGISTER), data {reg} {}
	explicit Operand(Label lab)
	: data_type {DataType::INVALID}, type(Type::LABEL), data {lab} {}
	explicit Operand(Function fun)
	: data_type {DataType::INVALID}, type(Type::FUN), data {fun} {}
	explicit Operand(Array arr)
	: data_type {DataType::INVALID}, type(Type::ARR), data {arr} {}

	Label& as_label() { return std::get<Label>(data); }
	Register& as_register() { return std::get<Register>(data); }
	Immediate& as_immediate() { return std::get<Immediate>(data); }
	Array& as_array() { return std::get<Array>(data); }

	const Label& as_label() const { return std::get<Label>(data); }
	const Register& as_register() const { return std::get<Register>(data); }
	const Immediate& as_immediate() const { return std::get<Immediate>(data); }
	const Array& as_array() const { return std::get<Array>(data); }

	void deinit() { return; }

	static auto make_immediate_integer(int integer) -> Operand;
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

	void emit_store(Operand value, Operand offset, Operand base);
};

// return amount of operands of each opcode
size_t opcode_opnd_count(Opcode op);

void print_chunk(FILE*, const Chunk&);
int print_inst(FILE*, const Instruction& inst);

Chunk operator+(Chunk x, Chunk y);

const char* operand_type_repr(Operand::Type type);

} // namespace lir

#endif
