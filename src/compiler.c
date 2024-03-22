#include "compiler.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "env.h"

#define OPERAND_NIL() \
	(Operand) { .type = OPND_NIL, .nil = NULL }

#define OPERAND_TMP(IDX)                                      \
	(Operand) {                                                 \
		.type = OPND_TMP, .reg = {.type = VAL_NUM, .index = IDX } \
	}

#define OPERAND_LAB(IDX) \
	(Operand) { .type = OPND_LAB, .lab = IDX }

#define REG_NUM(IDX) \
	(Register) { .type = VAL_NUM, .index = IDX }

static Chunk chunk_init();
static void chunk_append(Chunk* chunk, Instruction inst);

static void operand_deinit(Operand opnd) {
	if (opnd.type == OPND_STR) free(opnd.str);
	return;
}

static void var_stack_pop(VariableStack* vars, size_t index) {
	operand_deinit(vars->opnds[index]);
	vars->len--;
}

Compiler compiler_init() {
	Compiler comp;
	comp.label_count = 0;
	comp.tmp_count = 0;
	comp.reg_count = 0;
	comp.vars.len = 0;
	comp.vars.cap = 32;
	comp.vars.opnds = malloc(sizeof(Operand) * comp.vars.cap);
	comp.env = env_init();
	return comp;
}

static void comp_env_push(Compiler* comp) { env_push(&comp->env); }

static void comp_env_pop(Compiler* comp) {
	env_pop(&comp->env, (void (*)(void*, size_t))var_stack_pop, &comp->vars);
}

static Operand* comp_env_get_new(Compiler* comp, size_t sym_index) {
	size_t idx = env_get_new(&comp->env, sym_index);
	comp->vars.len++;
	Operand* op = &comp->vars.opnds[idx];
	op->type = OPND_REG;
	op->reg.index = comp->reg_count++;
	op->reg.type = VAL_NUM;
	return op;
}

static Operand* comp_env_get_new_offset(
	Compiler* comp, size_t sym_index, size_t size
) {
	// one for base and another for offset
	size_t idx = env_get_new(&comp->env, sym_index);
	comp->vars.len++;
	Operand* op = &comp->vars.opnds[idx];
	op->type = OPND_REG;
	op->reg.index = comp->reg_count;
	op->reg.type = VAL_NUM;
	comp->reg_count += size;
	return op;
}

static Operand* comp_env_find(Compiler* comp, size_t sym_index) {
	bool found = true;
	size_t idx = env_find(&comp->env, sym_index, &found);
	if (!found) return NULL;
	return &comp->vars.opnds[idx];
}

void compiler_deinit(Compiler* comp) {
	env_deinit(&comp->env, (void (*)(void*, size_t))var_stack_pop, &comp->vars);
	(void)comp;
	return;
}

static Operand compile_node(
	Compiler* comp, Node node, SymbolTable* syms, Chunk* chunk
);

static Operand compile_builtin_write(Chunk* chunk, size_t argc, Operand* args);
static Operand compile_builtin_read(
	Compiler* comp, Chunk* chunk, size_t argc, Operand* args
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

static void print_str(FILE* fd, String str) {
	fputc('"', fd);
	char c;
	while ((c = *(str++))) {
		if (c == '\n')
			fprintf(fd, "\\n");
		else if (c == '\t')
			fprintf(fd, "\\t");
		else
			fputc(c, fd);
	}
	fputc('"', fd);
}

static void print_operand(FILE* fd, Operand opnd) {
	switch (opnd.type) {
		case OPND_NIL: fprintf(fd, "0"); break;
		case OPND_TMP: fprintf(fd, "%%t%zu", opnd.reg.index); break;
		case OPND_REG: fprintf(fd, "%%r%zu", opnd.reg.index); break;
		case OPND_LAB: fprintf(fd, "L%03zu", opnd.lab); break;
		case OPND_STR: print_str(fd, opnd.str); break;
		case OPND_NUM: fprintf(fd, "%d", opnd.num); break;
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
	[OP_GREATER] = 3, [OP_GREATER_EQ] = 3, [OP_LOAD] = 3,      [OP_STORE] = 3,
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
		if (inst.opcode == OP_LOAD || inst.opcode == OP_STORE) {
			fprintf(fd, ", ");
			print_operand(fd, inst.operands[1]);
			fprintf(fd, "(");

			Operand opnd = inst.operands[2];
			if (opnd.reg.type == VAL_NUM)
				fprintf(fd, "%zu", opnd.reg.index);
			else
				fprintf(fd, "%%r%zu", opnd.reg.index);

			fprintf(fd, ")");
			fprintf(fd, "\n");
			continue;
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

	// heap start
	chunk_append(
		&chunk,
		(Instruction) {
			.opcode = OP_MOV,
			.operands[0] = (Operand) {OPND_REG, .reg = REG_NUM(comp->reg_count++)},
			.operands[1] = (Operand) {OPND_NUM, .num = 1024 - 1},
		}
	);

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
	Compiler* comp, Chunk* chunk, size_t argc, Operand* args
) {
	(void)args;
	(void)argc;
	Instruction inst;
	inst.opcode = OP_READ;
	Operand tmp = OPERAND_TMP(comp->tmp_count++);
	inst.operands[0] = tmp;
	chunk_append(chunk, inst);
	return tmp;
}

// create array with runtime-known size
static Operand compile_builtin_array(
	Compiler* comp, Chunk* chunk, size_t argc, Operand* args
) {
	assert(
		argc == 1
		&& "The `array' builtin expects a size as the first and only argument."
	);
	Operand addr = (Operand) {
		OPND_TMP, .reg = (Register) {VAL_ADDR, .index = comp->tmp_count++}};
	Operand heap_start = (Operand) {OPND_REG, .reg = REG_NUM(0)};

	chunk_append(
		chunk,
		(Instruction) {
			.opcode = OP_MOV,
			.operands[0] = addr,
			.operands[1] = heap_start,
		}
	);

	chunk_append(
		chunk,
		(Instruction) {
			.opcode = OP_ADD,
			.operands[0] = heap_start,
			.operands[1] = heap_start,
			.operands[2] = args[0],
		}
	);

	return addr;
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
				res = compile_builtin_read(comp, chunk, args.children_count, args_op);
			else if (strcmp(func_name, "array") == 0)
				res = compile_builtin_array(comp, chunk, args.children_count, args_op);
			else
				assert(false && "COMPILER_ERR: Function application outside of builtins not implemented");

			free(args_op);
			return res;
		}
		case AST_NUM: return (Operand) {.type = OPND_NUM, .num = node.num};
		case AST_BLK: {
			comp_env_push(comp);
			Operand opnd;
			for (size_t i = 0; i < node.children_count; i++)
				opnd = compile_node(comp, node.children[i], syms, chunk);
			comp_env_pop(comp);
			return opnd;
		}
		case AST_IF: {
			// code for condition
			// if condition false jump to label 1
			// code for true branch
			// move result value to tmp
			// jump to label 2
			// label 1
			// code for false branch
			// move result value to tmp
			// label 2

			Node cond = node.children[0];
			Node yes = node.children[1];
			Node no = node.children[2];

			Operand l1 = OPERAND_LAB(comp->label_count++);
			Operand l2 = OPERAND_LAB(comp->label_count++);
			Operand tmp = OPERAND_TMP(comp->tmp_count++);

			Operand cond_opnd = compile_node(comp, cond, syms, chunk);

			Instruction jfalse;
			jfalse.opcode = OP_JMP_FALSE;
			jfalse.operands[0] = cond_opnd;
			jfalse.operands[1] = l1;
			chunk_append(chunk, jfalse);

			Operand yes_opnd = compile_node(comp, yes, syms, chunk);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_MOV,
					.operands[0] = tmp,
					.operands[1] = yes_opnd,
				}
			);
			chunk_append(chunk, (Instruction) {.opcode = OP_JMP, .operands[0] = l2});
			chunk_append(
				chunk, (Instruction) {.opcode = OP_LABEL, .operands[0] = l1}
			);

			Operand no_opnd = compile_node(comp, no, syms, chunk);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_MOV,
					.operands[0] = tmp,
					.operands[1] = no_opnd,
				}
			);

			chunk_append(
				chunk, (Instruction) {.opcode = OP_LABEL, .operands[0] = l2}
			);

			return tmp;
		}
		case AST_WHEN: {
			// cond code
			// mov nil to result reg
			// if cond is false jmp to label l1
			// yes code
			// mov yes result to result reg
			// label l1

			Node cond = node.children[0];
			Node yes = node.children[1];

			Operand l1 = OPERAND_LAB(comp->label_count++);
			Operand res = OPERAND_TMP(comp->tmp_count++);

			Operand cond_opnd = compile_node(comp, cond, syms, chunk);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_MOV,
					.operands[0] = res,
					.operands[1] = (Operand) {OPND_NIL, .nil = NULL}}
			);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_JMP_FALSE, .operands[0] = cond_opnd, .operands[1] = l1}
			);

			Operand yes_opnd = compile_node(comp, yes, syms, chunk);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_MOV,
					.operands[0] = res,
					.operands[1] = yes_opnd,
				}
			);
			chunk_append(
				chunk, (Instruction) {.opcode = OP_LABEL, .operands[0] = l1}
			);

			return res;
		}
		case AST_FOR: {
			Node var = node.children[0];
			Node id = var.children[0];
			Node from = node.children[1];
			Node to = node.children[2];
			Node exp = node.children[3];

			Operand l1 = OPERAND_LAB(comp->label_count++);
			Operand l2 = OPERAND_LAB(comp->label_count++);

			comp_env_push(comp);

			Operand from_opnd = compile_node(comp, from, syms, chunk);
			Operand to_opnd = compile_node(comp, to, syms, chunk);
			Operand* var_opnd = comp_env_get_new(comp, id.index);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_MOV,
					.operands[0] = *var_opnd,
					.operands[1] = from_opnd,
				}
			);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_LABEL,
					.operands[0] = l1,
				}
			);

			// FIXME hardcode comparison
			// comparison cannot be determined at compile-time
			Operand cmp = OPERAND_TMP(comp->tmp_count++);
			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_LESS_EQ,
					.operands[0] = cmp,
					.operands[1] = *var_opnd,
					.operands[2] = to_opnd,
				}
			);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_JMP_FALSE, .operands[0] = cmp, .operands[1] = l2}
			);

			Operand exp_opnd = compile_node(comp, exp, syms, chunk);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_ADD,
					.operands[0] = *var_opnd,
					.operands[1] = *var_opnd,
					.operands[2] = (Operand) {OPND_NUM, .num = 1},
				}
			);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_JMP,
					.operands[0] = l1,
				}
			);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_LABEL,
					.operands[0] = l2,
				}
			);

			comp_env_pop(comp);

			return exp_opnd;
		}
		case AST_WHILE: {
			Node cond = node.children[0];
			Node exp = node.children[1];

			Operand l1 = OPERAND_LAB(comp->label_count++);
			Operand l2 = OPERAND_LAB(comp->label_count++);

			chunk_append(
				chunk, (Instruction) {.opcode = OP_LABEL, .operands[0] = l1}
			);

			Operand cond_opnd = compile_node(comp, cond, syms, chunk);

			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_JMP_FALSE, .operands[0] = cond_opnd, .operands[1] = l2}
			);

			Operand exp_opnd = compile_node(comp, exp, syms, chunk);

			chunk_append(chunk, (Instruction) {.opcode = OP_JMP, .operands[0] = l1});

			chunk_append(
				chunk, (Instruction) {.opcode = OP_LABEL, .operands[0] = l2}
			);

			return exp_opnd;
		}
		case AST_ASS: {
			Node var = node.children[0];
			Node exp = node.children[1];
			Node id = var.children[0];

			Operand tmp = compile_node(comp, exp, syms, chunk);
			Operand* reg = comp_env_find(comp, id.index);
			assert(reg);

			if (var.children_count == 2) {
				// if register is the start of the array then say so, otherwise it is a
				// address to the register that contains the beggining of the array
				Operand base = (reg->reg.type == VAL_NUM)
				               ? (Operand) {OPND_NUM, .num = reg->reg.index}
				               : *reg;
				Operand idx = compile_node(comp, var.children[1], syms, chunk);
				chunk_append(chunk, (Instruction) {OP_STORE, {tmp, idx, base}});
				return tmp;
			}

			chunk_append(chunk, (Instruction) {OP_MOV, {*reg, tmp}});
			return tmp;
		}

#define BINARY_ARITH(OPCODE)                                   \
	{                                                            \
		Node left = node.children[0];                              \
		Node right = node.children[1];                             \
		Operand left_op = compile_node(comp, left, syms, chunk);   \
		Operand right_op = compile_node(comp, right, syms, chunk); \
		Operand res = OPERAND_TMP(comp->tmp_count++);              \
                                                               \
		Instruction inst;                                          \
		inst.opcode = OPCODE;                                      \
		inst.operands[0] = res;                                    \
		inst.operands[1] = left_op;                                \
		inst.operands[2] = right_op;                               \
		chunk_append(chunk, inst);                                 \
		return res;                                                \
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
			Operand res = OPERAND_TMP(comp->tmp_count++);
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
		case AST_DECL: {
			Node var = node.children[0];
			Node id = var.children[0];
			Operand* opnd;

			// var id = exp
			if (node.children_count == 2) {
				opnd = comp_env_get_new(comp, id.index);
				Operand tmp = compile_node(comp, node.children[1], syms, chunk);
				if (tmp.type == OPND_TMP) opnd->reg.type = tmp.reg.type;
				chunk_append(
					chunk,
					(Instruction) {
						.opcode = OP_MOV,
						.operands[0] = *opnd,
						.operands[1] = tmp,
					}
				);
				return OPERAND_NIL();
			}

			// var id [idx]
			if (var.children_count == 2) {
				Node size = var.children[1];
				assert(
					size.type == AST_NUM
					&& "COMPILER_ERR: Bracket declarations requires the size to be constant. Use the `array' built-in, instead."
				);
				Operand size_opnd = compile_node(comp, size, syms, chunk);

				opnd = comp_env_get_new_offset(comp, id.index, size_opnd.num);
				return OPERAND_NIL();
			}

			opnd = comp_env_get_new(comp, id.index);
			chunk_append(
				chunk,
				(Instruction) {
					.opcode = OP_MOV,
					.operands[0] = *opnd,
					.operands[1] = OPERAND_NIL(),
				}
			);
			return OPERAND_NIL();
		}
		case AST_VAR: {
			Node id = node.children[0];
			Operand* reg = comp_env_find(comp, id.index);
			assert(reg && "Variable not previously declared");

			// id
			if (node.children_count == 1) return *reg;

			// id [idx]
			Operand res = OPERAND_TMP(comp->tmp_count++);
			Operand off = compile_node(comp, node.children[1], syms, chunk);
			Operand base = (reg->reg.type == VAL_NUM)
			               ? (Operand) {OPND_NUM, .num = reg->reg.index}
			               : *reg;

			chunk_append(chunk, (Instruction) {OP_LOAD, {res, off, base}});
			return res;
		}
		case AST_NIL: return (Operand) {.type = OPND_NUM, .num = 0};
		case AST_TRUE: return (Operand) {.type = OPND_NUM, .num = 1};
		case AST_LET: {
			Node decls = node.children[0];
			Node exp = node.children[1];
			comp_env_push(comp);

			for (size_t i = 0; i < decls.children_count; i++)
				compile_node(comp, decls.children[i], syms, chunk);

			Operand res = compile_node(comp, exp, syms, chunk);

			comp_env_pop(comp);
			return res;
		}
	}
	assert(false);
}
