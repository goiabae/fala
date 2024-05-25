#ifndef FALA_COMPILER_HPP
#define FALA_COMPILER_HPP

#include <stddef.h>
#include <stdio.h>

#include <utility>
#include <vector>

#include "ast.h"
#include "env.h"

using std::pair;
using std::vector;

typedef enum InstructionOp {
	OP_PRINTF,
	OP_PRINTV,
	OP_READ,
	OP_MOV,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_NOT,
	OP_OR,
	OP_AND,
	OP_EQ,
	OP_DIFF,
	OP_LESS,
	OP_LESS_EQ,
	OP_GREATER,
	OP_GREATER_EQ,
	OP_LOAD,
	OP_STORE,
	OP_LABEL,
	OP_JMP,
	OP_JMP_FALSE,
	OP_JMP_TRUE,
} InstructionOp;

typedef struct Register {
	enum {
		VAL_NUM,  // register contains a plain number
		VAL_ADDR, // register contains an address of a r-register
	} type;
	size_t index;
	Register(size_t idx) : index {idx} {}
	Register& as_num() { return (type = VAL_NUM), *this; }
	Register& as_addr() { return (type = VAL_ADDR), *this; }
} Register;

typedef struct Funktion {
	size_t argc;
	Node* args;
	Node root;
} Funktion;

struct Operand {
	enum Type {
		OPND_NIL, // no operand
		OPND_TMP, // temporary register
		OPND_REG, // variables register
		OPND_LAB, // label
		OPND_STR, // immediate string
		OPND_NUM, // immediate number
		OPND_FUN,
	};

	union Value {
		Register reg;
		size_t lab;
		String str;
		Number num;
		Funktion fun;

		Value(Register reg) : reg {reg} {}
		Value(size_t lab) : lab {lab} {}
		Value(String str) : str {str} {}
		Value(Number num) : num {num} {}
		Value(Funktion fun) : fun {fun} {}
		Value() : num {0} {}
	};

	Type type;
	Value value;

	Operand() : type {Type::OPND_NIL}, value {} {}
	Operand(Type type, Value value) : type {type}, value {value} {}
};

typedef struct Instruction {
	InstructionOp opcode;
	Operand operands[3];
} Instruction;

using Chunk = std::vector<Instruction>;

template<typename T>
using Env = vector<vector<pair<size_t, T>>>;

struct Compiler {
	size_t label_count {0};
	size_t tmp_count {0};
	size_t reg_count {0};

	Env<Operand> env;

	bool in_loop {false};
	Operand cnt_lab; // jump to on continue
	Operand brk_lab; // jump to on break

	// MOVs to backpatch the operands
	size_t back_patch_stack_len {0};
	size_t* back_patch_stack;
	size_t back_patch_len {0};
	size_t* back_patch;

	Compiler();
	~Compiler();

	Chunk compile(AST ast, const SymbolTable& syms);
	Operand compile(Node node, const SymbolTable* syms, Chunk* chunk);
	Operand get_temporary();
	Operand get_register();
	Operand get_label();
};

void print_chunk(FILE*, const Chunk&);

#endif
