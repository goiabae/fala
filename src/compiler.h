#ifndef FALA_COMPILER_H
#define FALA_COMPILER_H

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

typedef struct Operand {
	enum {
		OPND_NIL, // no operand

		OPND_TMP, // temporary register
		OPND_REG, // variables register
		OPND_LAB, // label
		OPND_STR, // immediate string
		OPND_NUM, // immediate number
	} type;
	union {
		void* nil;
		size_t index; // temporaries, registers and labels
		String str;
		Number num;
	};
} Operand;

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

typedef struct Compiler {
	size_t label_count;
	size_t tmp_count;
	size_t reg_count;
	Environment env;
	VariableStack vars;
} Compiler;

void chunk_deinit(Chunk*);
void print_chunk(FILE*, Chunk);

Compiler compiler_init();
void compiler_deinit(Compiler* comp);

Chunk compile_ast(Compiler* comp, AST ast, SymbolTable* syms);

#endif
