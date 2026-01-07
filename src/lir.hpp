#ifndef LIR_HPP
#define LIR_HPP

#include <cstddef>
#include <map>
#include <memory>
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
	SHIFTA,
};

class Type;

class Pointer {
 public:
	std::shared_ptr<Type> pointed_type;
	bool is_many_pointer;
	Pointer(std::shared_ptr<Type> pointed_type, bool is_many_pointer)
	: pointed_type {pointed_type}, is_many_pointer {is_many_pointer} {}
};

class Integer {};

class Type : public std::variant<Pointer, Integer> {
 public:
	Type(const Integer& i) : std::variant<Pointer, Integer> {i} {}
	Type(const Pointer& p) : std::variant<Pointer, Integer> {p} {}
	static Type make_integer();
	static Type make_integer_array();
};

struct Register {
	explicit Register(size_t idx, Type type) : index {idx}, type {type} {}
	size_t index;
	bool is_lvalue_pointer {false};
	Type type;
};

struct Function {
	size_t argc;
	Node* args;
	Node root;
};

struct Label {
	size_t id;
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
	};

	Type type;
	std::variant<Register, Label, Immediate, Function> data;

	Operand() : type {Type::NOTHING}, data {Immediate {0}} {}
	explicit Operand(Register reg) : type(Type::REGISTER), data {reg} {}
	explicit Operand(Label lab) : type(Type::LABEL), data {lab} {}
	explicit Operand(Function fun) : type(Type::FUN), data {fun} {}

	Label& as_label() { return std::get<Label>(data); }
	Register& as_register() { return std::get<Register>(data); }
	Immediate& as_immediate() { return std::get<Immediate>(data); }

	const Label& as_label() const { return std::get<Label>(data); }
	const Register& as_register() const { return std::get<Register>(data); }
	const Immediate& as_immediate() const { return std::get<Immediate>(data); }

	static auto make_immediate_integer(int integer) -> Operand;
};

struct Instruction {
	static constexpr auto max_operands = 3;
	Opcode opcode;
	Operand operands[max_operands];
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
