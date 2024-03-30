#include "compiler.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "env.h"

#define OPERAND_NIL() ((Operand) {OPND_NIL, .nil = NULL})
#define OPERAND_TMP(IDX) ((Operand) {OPND_TMP, .reg = {VAL_NUM, IDX}})
#define OPERAND_LAB(IDX) ((Operand) {OPND_LAB, .lab = IDX})

#define emit(C, OP, ...) chunk_append(C, (Instruction) {OP, {__VA_ARGS__}})

static Chunk chunk_init();
static void chunk_append(Chunk* chunk, Instruction inst);
static Operand compile_node(Compiler*, Node, SymbolTable*, Chunk*);
static Operand compile_builtin_write(Compiler*, Chunk*, size_t, Operand*);
static Operand compile_builtin_read(Compiler*, Chunk*, size_t, Operand*);
static void print_operand(FILE*, Operand);

static void operand_deinit(Operand opnd) {
	if (opnd.type == OPND_STR) free(opnd.str);
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

void compiler_deinit(Compiler* comp) {
	env_deinit(&comp->env, (void (*)(void*, size_t))var_stack_pop, &comp->vars);
	free(comp->vars.opnds);
	return;
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
	Operand heap = {OPND_REG, .reg = {VAL_NUM, comp->reg_count++}};
	Operand start = {OPND_NUM, .num = 1024 - 1};

	emit(&chunk, OP_MOV, heap, start);
	(void)compile_node(comp, ast.root, syms, &chunk);
	return chunk;
}

static Operand compile_builtin_write(
	Compiler* comp, Chunk* chunk, size_t argc, Operand* args
) {
	(void)comp;
	for (size_t i = 0; i < argc; i++)
		emit(chunk, ((args[i].type == OPND_STR) ? OP_PRINTF : OP_PRINTV), args[i]);
	return OPERAND_NIL();
}

static Operand compile_builtin_read(
	Compiler* comp, Chunk* chunk, size_t argc, Operand* args
) {
	(void)args;
	(void)argc;
	Operand tmp = OPERAND_TMP(comp->tmp_count++);
	emit(chunk, OP_READ, tmp);
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
	Operand addr = {OPND_TMP, .reg = (Register) {VAL_ADDR, comp->tmp_count++}};
	Operand heap = {OPND_REG, .reg = {VAL_NUM, 0}};

	emit(chunk, OP_MOV, addr, heap);
	emit(chunk, OP_ADD, heap, heap, args[0]);

	return addr;
}

// returns the result register or immediate value
static Operand compile_node(
	Compiler* comp, Node node, SymbolTable* syms, Chunk* chunk
) {
	switch (node.type) {
		case AST_APP: {
			Node func = node.children[0];
			String func_name = sym_table_get(syms, func.index);
			Node args_node = node.children[1];

			Operand* args = malloc(sizeof(Operand) * args_node.children_count);
			for (size_t i = 0; i < args_node.children_count; i++)
				args[i] = compile_node(comp, args_node.children[i], syms, chunk);

			Operand res;
			if (strcmp(func_name, "write") == 0)
				res =
					compile_builtin_write(comp, chunk, args_node.children_count, args);
			else if (strcmp(func_name, "read") == 0)
				res = compile_builtin_read(comp, chunk, args_node.children_count, args);
			else if (strcmp(func_name, "array") == 0)
				res =
					compile_builtin_array(comp, chunk, args_node.children_count, args);
			else
				assert(false && "COMPILER_ERR: Function application outside of builtins not implemented");

			free(args);
			return res;
		}
		case AST_NUM: return (Operand) {OPND_NUM, .num = node.num};
		case AST_BLK: {
			comp_env_push(comp);
			Operand opnd = OPERAND_NIL();
			for (size_t i = 0; i < node.children_count; i++)
				opnd = compile_node(comp, node.children[i], syms, chunk);
			comp_env_pop(comp);
			return opnd;
		}
		case AST_IF: {
			Node cond = node.children[0];
			Node yes = node.children[1];
			Node no = node.children[2];

			Operand l1 = OPERAND_LAB(comp->label_count++);
			Operand l2 = OPERAND_LAB(comp->label_count++);
			Operand res = OPERAND_TMP(comp->tmp_count++);

			Operand cond_opnd = compile_node(comp, cond, syms, chunk);

			emit(chunk, OP_JMP_FALSE, cond_opnd, l1);

			Operand yes_opnd = compile_node(comp, yes, syms, chunk);

			emit(chunk, OP_MOV, res, yes_opnd);
			emit(chunk, OP_JMP, l2);
			emit(chunk, OP_LABEL, l1);

			Operand no_opnd = compile_node(comp, no, syms, chunk);

			emit(chunk, OP_MOV, res, no_opnd);
			emit(chunk, OP_LABEL, l2);

			return res;
		}
		case AST_WHEN: {
			Node cond = node.children[0];
			Node yes = node.children[1];

			Operand l1 = OPERAND_LAB(comp->label_count++);
			Operand res = OPERAND_TMP(comp->tmp_count++);

			Operand cond_opnd = compile_node(comp, cond, syms, chunk);

			emit(chunk, OP_MOV, res, OPERAND_NIL());
			emit(chunk, OP_JMP_FALSE, cond_opnd, l1);

			Operand yes_opnd = compile_node(comp, yes, syms, chunk);

			emit(chunk, OP_MOV, res, yes_opnd);
			emit(chunk, OP_LABEL, l1);

			return res;
		}
		case AST_FOR: {
			// FIXME hardcode comparison
			// comparison cannot be determined at compile-time

			Node var_node = node.children[0];
			Node id = var_node.children[0];
			Node from_node = node.children[1];
			Node to_node = node.children[2];
			Node exp_node = node.children[3];

			Operand l1 = OPERAND_LAB(comp->label_count++);
			Operand l2 = OPERAND_LAB(comp->label_count++);
			Operand cmp = OPERAND_TMP(comp->tmp_count++);
			Operand one = {OPND_NUM, .num = 1};

			comp_env_push(comp);

			Operand from = compile_node(comp, from_node, syms, chunk);
			Operand to = compile_node(comp, to_node, syms, chunk);
			Operand* var = comp_env_get_new(comp, id.index);

			emit(chunk, OP_MOV, *var, from);
			emit(chunk, OP_LABEL, l1);
			emit(chunk, OP_LESS_EQ, cmp, *var, to);
			emit(chunk, OP_JMP_FALSE, cmp, l2);

			Operand exp = compile_node(comp, exp_node, syms, chunk);

			emit(chunk, OP_ADD, *var, *var, one);
			emit(chunk, OP_JMP, l1);
			emit(chunk, OP_LABEL, l2);

			comp_env_pop(comp);

			return exp;
		}
		case AST_WHILE: {
			Node cond = node.children[0];
			Node exp = node.children[1];

			Operand l1 = OPERAND_LAB(comp->label_count++);
			Operand l2 = OPERAND_LAB(comp->label_count++);

			emit(chunk, OP_LABEL, l1);

			Operand cond_opnd = compile_node(comp, cond, syms, chunk);

			emit(chunk, OP_JMP_FALSE, cond_opnd, l2);

			Operand exp_opnd = compile_node(comp, exp, syms, chunk);

			emit(chunk, OP_JMP, l1);
			emit(chunk, OP_LABEL, l2);

			return exp_opnd;
		}
		case AST_ASS: {
			Node var = node.children[0];
			Node exp = node.children[1];
			Node id = var.children[0];

			Operand tmp = compile_node(comp, exp, syms, chunk);
			Operand* reg = comp_env_find(comp, id.index);
			assert(reg && "COMPILE_ERR: Variable not found");

			if (var.children_count == 2) {
				// if register is the start of the array then say so, otherwise it is a
				// address to the register that contains the beggining of the array
				Operand base = (reg->reg.type == VAL_NUM)
				               ? (Operand) {OPND_NUM, .num = reg->reg.index}
				               : *reg;
				Operand idx = compile_node(comp, var.children[1], syms, chunk);
				emit(chunk, OP_STORE, tmp, idx, base);
				return tmp;
			}

			emit(chunk, OP_MOV, *reg, tmp);
			return tmp;
		}

#define BINARY_ARITH(OPCODE)                                     \
	{                                                              \
		Node left_node = node.children[0];                           \
		Node right_node = node.children[1];                          \
                                                                 \
		Operand left = compile_node(comp, left_node, syms, chunk);   \
		Operand right = compile_node(comp, right_node, syms, chunk); \
		Operand res = OPERAND_TMP(comp->tmp_count++);                \
                                                                 \
		emit(chunk, OPCODE, res, left, right);                       \
		return res;                                                  \
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
			Operand inverse = compile_node(comp, node.children[0], syms, chunk);
			emit(chunk, OP_NOT, res, inverse);
			return res;
		}
		case AST_ID: assert(false && "unreachable");
		case AST_STR:
			return (Operand) {OPND_STR, .str = sym_table_get(syms, node.index)};
		case AST_DECL: {
			assert(
				node.children_count != 3
				&& "COMPILE_ERR: Function declaration not yet supported"
			);

			Node var = node.children[0];
			Node id = var.children[0];

			// var id = exp
			if (node.children_count == 2) {
				Operand* opnd = comp_env_get_new(comp, id.index);
				Operand tmp = compile_node(comp, node.children[1], syms, chunk);
				if (tmp.type == OPND_TMP) opnd->reg.type = tmp.reg.type;
				emit(chunk, OP_MOV, *opnd, tmp);
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
				(void)comp_env_get_new_offset(comp, id.index, size_opnd.num);
				return OPERAND_NIL();
			}

			// var id
			Operand* opnd = comp_env_get_new(comp, id.index);
			emit(chunk, OP_MOV, *opnd, OPERAND_NIL());
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

			emit(chunk, OP_LOAD, res, off, base);
			return res;
		}
		case AST_NIL: return (Operand) {OPND_NUM, .num = 0};
		case AST_TRUE: return (Operand) {OPND_NUM, .num = 1};
		case AST_LET: {
			Node decls = node.children[0];
			Node exp = node.children[1];
			comp_env_push(comp);

			for (size_t i = 0; i < decls.children_count; i++)
				(void)compile_node(comp, decls.children[i], syms, chunk);

			Operand res = compile_node(comp, exp, syms, chunk);

			comp_env_pop(comp);
			return res;
		}
	}
	assert(false);
}
