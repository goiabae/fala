#include "compiler.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

#define OPERAND_NIL() \
	(Operand) { .type = OPND_NIL, .nil = NULL }

static Chunk chunk_init();
static void chunk_append(Chunk* chunk, Instruction inst);

Compiler compiler_init() {
	Compiler comp;
	comp.label_count = 0;
	comp.var_count = 0;
	comp.tmp_count = 0;
	return comp;
}

void compiler_deinit(Compiler* comp) {
	(void)comp;
	return;
}

static Operand compile_node(
	Compiler* comp, Node node, SymbolTable* syms, Chunk* chunk
);

static Operand compile_builtin_write(Chunk* chunk, size_t argc, Operand* args);
static Operand compile_builtin_read(
	Chunk* chunk, size_t argc, Operand* args, size_t* temps
);

static void print_operand(FILE*, Operand);

static Chunk chunk_init() {
	Chunk chunk;
	chunk.cap = 1024;
	chunk.len = 0;
	chunk.insts = malloc(sizeof(Instruction) * chunk.cap);
	return chunk;
}

void chunk_deinit(Chunk* chunk) { free(chunk->insts); }

static void chunk_append(Chunk* chunk, Instruction inst) {
	assert(
		(chunk->len + 1 <= chunk->cap)
		&& "COMPILER_ERR: Max number of instructions exceeded"
	);
	chunk->insts[chunk->len++] = inst;
}

static void print_operand(FILE* fd, Operand opnd) {
	switch (opnd.type) {
		case OPND_NIL: fprintf(fd, "0"); break;
		case OPND_TMP: fprintf(fd, "%%t%zu", opnd.index); break;
		case OPND_REG: fprintf(fd, "%%r%zu", opnd.index); break;
		case OPND_LAB: fprintf(fd, "L%03zu", opnd.index); break;
		case OPND_STR: fprintf(fd, "\"%s\"", opnd.str); break;
		case OPND_NUM: fprintf(fd, "%d", opnd.num); break;
		case OPND_OFF: assert(false && "PRINT_ERR: offset not implemented"); break;
	}
}

static const char* opcode_repr[] = {
	[OP_PRINTF] = "printf",   [OP_PRINTV] = "printv",
	[OP_READ] = "read",       [OP_MOV] = "mov",
	[OP_ADD] = "add",         [OP_SUB] = "sub",
	[OP_MUL] = "mult",        [OP_DIV] = "div",
	[OP_MOD] = "mod",         [OP_NOT] = "not",
	[OP_OR] = "or",           [OP_AND] = "and",
	[OP_EQ] = "equal",        [OP_DIFF] = "diff",
	[OP_LESS] = "less",       [OP_LESS_EQ] = "lesseq",
	[OP_GREATER] = "greater", [OP_GREATER_EQ] = "greatereq",
	[OP_LOAD] = "load",       [OP_STORE] = "store",
	[OP_LABEL] = "label",     [OP_JMP] = "jump",
	[OP_JMP_FALSE] = "jf",    [OP_JMP_TRUE] = "jt",
};

static const int opcode_opnd_count[] = {
	[OP_PRINTF] = 1,  [OP_PRINTV] = 1,     [OP_READ] = 1,      [OP_MOV] = 2,
	[OP_ADD] = 3,     [OP_SUB] = 3,        [OP_MUL] = 3,       [OP_DIV] = 3,
	[OP_MOD] = 3,     [OP_NOT] = 2,        [OP_OR] = 3,        [OP_AND] = 3,
	[OP_EQ] = 3,      [OP_DIFF] = 3,       [OP_LESS] = 3,      [OP_LESS_EQ] = 3,
	[OP_GREATER] = 3, [OP_GREATER_EQ] = 3, [OP_LOAD] = 2,      [OP_STORE] = 2,
	[OP_LABEL] = 1,   [OP_JMP] = 1,        [OP_JMP_FALSE] = 2, [OP_JMP_TRUE] = 2,
};

void print_chunk(FILE* fd, Chunk chunk) {
	for (size_t i = 0; i < chunk.len; i++) {
		Instruction inst = chunk.insts[i];
		if (inst.opcode != OP_LABEL) fprintf(fd, "  ");
		fprintf(fd, "%s", opcode_repr[inst.opcode]);
		if (opcode_opnd_count[inst.opcode] >= 1) {
			fprintf(fd, " ");
			print_operand(fd, inst.operands[0]);
		}
		if (opcode_opnd_count[inst.opcode] >= 2) {
			fprintf(fd, ", ");
			print_operand(fd, inst.operands[1]);
		}
		if (opcode_opnd_count[inst.opcode] == 3) {
			fprintf(fd, ", ");
			print_operand(fd, inst.operands[2]);
		}
		fprintf(fd, "\n");
	}
}

Chunk compile_ast(Compiler* comp, AST ast, SymbolTable* syms) {
	Chunk chunk = chunk_init();
	Operand op = compile_node(comp, ast.root, syms, &chunk);
	(void)op;
	return chunk;
}

static Operand compile_builtin_write(Chunk* chunk, size_t argc, Operand* args) {
	for (size_t i = 0; i < argc; i++) {
		Instruction inst;
		if (args[i].type == OPND_STR)
			inst.opcode = OP_PRINTF;
		else
			inst.opcode = OP_PRINTV;
		inst.operands[0] = args[i];
		chunk_append(chunk, inst);
	}
	return OPERAND_NIL();
}

static Operand compile_builtin_read(
	Chunk* chunk, size_t argc, Operand* args, size_t* temps
) {
	(void)args;
	(void)argc;
	Instruction inst;
	inst.opcode = OP_READ;
	Operand tmp = (Operand) {.type = OPND_TMP, .index = (*temps)++};
	inst.operands[0] = tmp;
	chunk_append(chunk, inst);
	return tmp;
}

// returns the result register or immediate value
static Operand compile_node(
	Compiler* comp, Node node, SymbolTable* syms, Chunk* chunk
) {
	(void)syms;
	switch (node.type) { // TODO
		case AST_APP: {
			Node func = node.children[0];
			String func_name = sym_table_get(syms, func.index);
			Node args = node.children[1];
			Operand res;
			Operand* args_op = malloc(sizeof(Operand) * args.children_count);
			for (size_t i = 0; i < args.children_count; i++)
				args_op[i] = compile_node(comp, args.children[i], syms, chunk);

			if (strcmp(func_name, "write") == 0)
				res = compile_builtin_write(chunk, args.children_count, args_op);
			else if (strcmp(func_name, "read") == 0)
				res = compile_builtin_read(
					chunk, args.children_count, args_op, &comp->tmp_count
				);
			else
				assert(false && "COMPILER_ERR: Function application outside of builtins not implemented");

			free(args_op);
			return res;
		}
		case AST_NUM: return (Operand) {.type = OPND_NUM, .num = node.num};
		case AST_BLK: {
			size_t old_var_count = comp->var_count;
			for (size_t i = 0; i < node.children_count; i++)
				compile_node(comp, node.children[i], syms, chunk);
			comp->var_count = old_var_count;
			return OPERAND_NIL();
		}
		case AST_IF: {
			Node cond = node.children[0];
			Node yes = node.children[1];
			Node no = node.children[2];

			Operand cond_opnd = compile_node(cond, syms, chunk, vars, temps);
			Operand yes_opnd = compile_node(yes, syms, chunk, vars, temps);
			Operand no_opnd = compile_node(no, syms, chunk, vars, temps);
			(void)cond_opnd;
			(void)yes_opnd;
			(void)no_opnd;
			// TODO
			assert(false && "COMPILER_ERR: if expressions not implemented");
			return OPERAND_NIL();
		}
		case AST_WHEN:
			assert(false && "COMPILER_ERR: when expressions not implemented");
			return OPERAND_NIL();
		case AST_FOR:
			assert(false && "COMPILER_ERR: for loops not implemented");
			return OPERAND_NIL();
		case AST_WHILE:
			assert(false && "COMPILER_ERR: while loops not implemented");
			return OPERAND_NIL();
		case AST_ASS:
			assert(false && "COMPILER_ERR: assignment not implemented");
			return OPERAND_NIL();

#define BINARY_ARITH(OPCODE)                                                \
	{                                                                         \
		Node left = node.children[0];                                           \
		Node right = node.children[1];                                          \
		Operand left_op = compile_node(comp, left, syms, chunk);                \
		Operand right_op = compile_node(comp, right, syms, chunk);              \
		Operand res = (Operand) {.type = OPND_TMP, .index = comp->tmp_count++}; \
                                                                            \
		Instruction inst;                                                       \
		inst.opcode = OPCODE;                                                   \
		inst.operands[0] = res;                                                 \
		inst.operands[1] = left_op;                                             \
		inst.operands[2] = right_op;                                            \
		chunk_append(chunk, inst);                                              \
		return res;                                                             \
	}

		case AST_OR: BINARY_ARITH(OP_OR);
		case AST_AND: BINARY_ARITH(OP_AND);
		case AST_GTN: BINARY_ARITH(OP_GREATER);
		case AST_LTN: BINARY_ARITH(OP_LESS);
		case AST_GTE: BINARY_ARITH(OP_GREATER_EQ);
		case AST_LTE: BINARY_ARITH(OP_LESS_EQ);
		case AST_EQ: BINARY_ARITH(OP_EQ);
		case AST_ADD: BINARY_ARITH(OP_ADD);
		case AST_SUB: BINARY_ARITH(OP_SUB);
		case AST_MUL: BINARY_ARITH(OP_MUL);
		case AST_DIV: BINARY_ARITH(OP_DIV);
		case AST_MOD: BINARY_ARITH(OP_MOD);
		case AST_NOT: {
			Operand res = (Operand) {.type = OPND_TMP, .index = comp->tmp_count++};
			Instruction inst;
			inst.opcode = OP_NOT;
			inst.operands[0] = res;
			inst.operands[1] = compile_node(comp, node.children[0], syms, chunk);
			chunk_append(chunk, inst);
			return res;
		}
		case AST_ID: assert(false && "unreachable"); return OPERAND_NIL();
		case AST_STR:
			return (Operand) {
				.type = OPND_STR, .str = sym_table_get(syms, node.index)};
		case AST_DECL:
			assert(false && "COMPILER_ERR: variable declaration not implemented");
			return OPERAND_NIL();
		case AST_VAR:
			assert(false && "COMPILER_ERR: variable access not implemented");
			return OPERAND_NIL();
		case AST_NIL: return (Operand) {.type = OPND_NUM, .num = 0};
		case AST_TRUE: return (Operand) {.type = OPND_NUM, .num = 1};
		case AST_LET:
			assert(false && "COMPILER_ERR: let bindings not implemented");
			return OPERAND_NIL();
	}
	assert(false);
}
