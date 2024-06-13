#ifndef FALA_COMPILER_HPP
#define FALA_COMPILER_HPP

#include <stddef.h>
#include <stdio.h>

#include <string>
#include <utility>
#include <vector>

#include "ast.h"
#include "env.hpp"
#include "str_pool.h"

using std::pair;
using std::string;
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
	bool has_num() const { return type == VAL_NUM; }
	bool has_addr() const { return type == VAL_ADDR; }
} Register;

typedef struct Funktion {
	size_t argc;
	Node* args;
	Node root;
} Funktion;

struct Label {
	size_t id;
};

struct Instruction;

struct Operand {
	enum class Type {
		NIL, // no operand
		TMP, // temporary register
		REG, // variables register
		LAB, // label
		STR, // immediate string
		NUM, // immediate number
		FUN,
	};

	Type type;

	union {
		Register reg;
		Label lab;
		const char* str;
		Number num;
		Funktion fun;
	};

	Operand() : type {Type::NIL}, num {0} {}
	Operand(Register reg) : type(Type::NIL), reg {reg} {}
	Operand(Label lab) : type(Type::LAB), lab {lab} {}
	Operand(const char* str) : type(Type::STR), str {str} {}
	constexpr Operand(Number num) : type(Type::NUM), num {num} {}
	Operand(Funktion fun) : type(Type::FUN), fun {fun} {}

	Operand to_rvalue(std::vector<Instruction>* chunk);
	bool is_register() const { return type == Type::REG || type == Type::TMP; }

	Operand as_reg() { return (type = Type::REG), *this; }
	Operand as_temp() { return (type = Type::TMP), *this; }

	void deinit() { return; }
};

typedef struct Instruction {
	InstructionOp opcode;
	Operand operands[3];
	string comment;
} Instruction;

using Chunk = std::vector<Instruction>;

struct Compiler {
	Compiler();
	~Compiler();

	Chunk compile(AST ast, const StringPool& pool);

 private:
	Operand compile(Node node, const StringPool& pool, Chunk* chunk);

	// these are monotonically increasing as the compiler goes on
	size_t label_count {0};
	size_t tmp_count {0};
	size_t reg_count {0};

	Operand make_temporary();
	Operand make_register();
	Operand make_label();

	Operand to_rvalue(Chunk* chunk, Operand opnd);

	bool in_loop {false};
	Operand cnt_lab; // jump to on continue
	Operand brk_lab; // jump to on break

	// these are used to backpatch the destination of MOVs generated when
	// compiling BREAK and CONTINUE nodes
	size_t back_patch_stack_len {0};
	size_t* back_patch_stack;
	size_t back_patch_len {0};
	size_t* back_patch;

	void push_to_back_patch(size_t idx);
	void back_patch_jumps(Chunk* chunk, Operand dest);

	Env<Operand> env;

	Operand builtin_read(Chunk* chunk, size_t argc, Operand args[]);
	Operand builtin_write(Chunk* chunk, size_t argc, Operand args[]);
	Operand builtin_array(Chunk* chunk, size_t argc, Operand args[]);
};

void print_chunk(FILE*, const Chunk&);

#endif
