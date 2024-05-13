#ifndef FALA_COMPILER_HPP
#define FALA_COMPILER_HPP

#include <stddef.h>
#include <stdio.h>

#include "ast.h"
#include "env.h"

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
		VAL_NUM,
		VAL_ADDR,
	} type;
	size_t index;
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
		void* nil;
		Register reg;
		size_t lab;
		String str;
		Number num;
		Funktion fun;

		Value(void*) : nil {nullptr} {}
		Value(Register reg) : reg {reg} {}
		Value(size_t lab) : lab {lab} {}
		Value(String str) : str {str} {}
		Value(Number num) : num {num} {}
		Value(Funktion fun) : fun {fun} {}
		Value() {};
	};

	Type type;
	Value value;
};

typedef struct Instruction {
	InstructionOp opcode;
	Operand operands[3];
} Instruction;

typedef struct Chunk {
	size_t len;
	size_t cap;
	Instruction* insts;
} Chunk;

typedef struct VariableStack {
	size_t len;
	size_t cap;
	Operand* opnds;
} VariableStack;

struct Compiler {
	size_t label_count;
	size_t tmp_count;
	size_t reg_count;
	Environment env;
	VariableStack vars;

	bool in_loop;
	Operand cnt_lab; // jump to on continue
	Operand brk_lab; // jump to on break

	// MOVs to backpatch the operands
	size_t back_patch_stack_len;
	size_t* back_patch_stack;
	size_t back_patch_len;
	size_t* back_patch;

	Compiler() {};
};

void chunk_deinit(Chunk*);
void print_chunk(FILE*, Chunk);

Compiler compiler_init();
void compiler_deinit(Compiler* comp);

Chunk compile_ast(Compiler* comp, AST ast, SymbolTable* syms);

#endif
