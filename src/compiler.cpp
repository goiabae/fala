#include "compiler.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include "ast.h"
#include "env.h"

#define CHUNK_INST_CAP 1024

#define emit(C, OP, ...) chunk_append(C, Instruction {OP, {__VA_ARGS__}})

static void chunk_append(Chunk* chunk, Instruction inst);
static Operand compile_builtin_write(Compiler*, Chunk*, size_t, Operand*);
static Operand compile_builtin_read(Compiler*, Chunk*, size_t, Operand*);
static void print_operand(FILE*, Operand);

Compiler::Compiler() {
	back_patch_stack = new size_t[32];
	back_patch = new size_t[32];
	env.push_back({});
}

Compiler::~Compiler() {
	delete back_patch;
	delete back_patch_stack;
}

void err(const char* msg) {
	std::cout << "COMPILER_ERR: " << msg << std::endl;
	exit(1);
}

static void comp_env_push(Compiler* comp) { comp->env.push_back({}); }
static void comp_env_pop(Compiler* comp) { comp->env.pop_back(); }

static Operand* comp_env_get_new(Compiler* comp, size_t sym_index) {
	comp->env.back().push_back({sym_index, {}});
	Operand& op = comp->env.back().back().second;
	op.type = Operand::OPND_REG;
	op.value.reg = Register {Register::VAL_NUM, comp->reg_count++};
	return &op;
}

static Operand* comp_env_get_new_offset(
	Compiler* comp, size_t sym_index, size_t size
) {
	// one for base and another for offset
	comp->env.back().push_back({sym_index, {}});
	Operand& op = comp->env.back().back().second;
	op.type = Operand::OPND_REG;
	op.value.reg = Register {Register::VAL_NUM, comp->reg_count};
	comp->reg_count += size;
	return &op;
}

static Operand* comp_env_find(Compiler* comp, size_t sym_index) {
	for (size_t i = comp->env.size(); i-- > 0;)
		for (size_t j = comp->env[i].size(); j-- > 0;)
			if (comp->env[i][j].first == sym_index) return &comp->env[i][j].second;
	return nullptr;
}

static void chunk_append(Chunk* chunk, Instruction inst) {
	if (!(chunk->size() + 1 <= CHUNK_INST_CAP))
		err("Max number of instructions exceeded");
	chunk->push_back(inst);
}

// backpatch MOVs destination operand
static void chunk_backpatch(Compiler* comp, Chunk* chunk, Operand dest) {
	size_t to_patch = comp->back_patch_stack[comp->back_patch_stack_len - 1];
	for (size_t i = 0; i < to_patch; i++) {
		size_t idx = comp->back_patch[comp->back_patch_len-- - 1];
		(*chunk)[idx].operands[0] = dest;
	}
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
		case Operand::OPND_NIL: fprintf(fd, "0"); break;
		case Operand::OPND_TMP: fprintf(fd, "%%t%zu", opnd.value.reg.index); break;
		case Operand::OPND_REG: fprintf(fd, "%%r%zu", opnd.value.reg.index); break;
		case Operand::OPND_LAB: fprintf(fd, "L%03zu", opnd.value.lab); break;
		case Operand::OPND_STR: print_str(fd, opnd.value.str); break;
		case Operand::OPND_NUM: fprintf(fd, "%d", opnd.value.num); break;
		case Operand::OPND_FUN: assert(false);
	}
}

const char* opcode_repr(int idx) {
	switch (idx) {
		case OP_PRINTF: return "printf";
		case OP_PRINTV: return "printv";
		case OP_READ: return "read";
		case OP_MOV: return "mov";
		case OP_ADD: return "add";
		case OP_SUB: return "sub";
		case OP_MUL: return "mult";
		case OP_DIV: return "div";
		case OP_MOD: return "mod";
		case OP_NOT: return "not";
		case OP_OR: return "or";
		case OP_AND: return "and";
		case OP_EQ: return "equal";
		case OP_DIFF: return "diff";
		case OP_LESS: return "less";
		case OP_LESS_EQ: return "lesseq";
		case OP_GREATER: return "greater";
		case OP_GREATER_EQ: return "greatereq";
		case OP_LOAD: return "load";
		case OP_STORE: return "store";
		case OP_LABEL: return "label";
		case OP_JMP: return "jump";
		case OP_JMP_FALSE: return "jf";
		case OP_JMP_TRUE: return "jt";
		default: assert(false);
	}
}

int opcode_opnd_count(int idx) {
	switch (idx) {
		case OP_PRINTF: return 1;
		case OP_PRINTV: return 1;
		case OP_READ: return 1;
		case OP_MOV: return 2;
		case OP_ADD: return 3;
		case OP_SUB: return 3;
		case OP_MUL: return 3;
		case OP_DIV: return 3;
		case OP_MOD: return 3;
		case OP_NOT: return 2;
		case OP_OR: return 3;
		case OP_AND: return 3;
		case OP_EQ: return 3;
		case OP_DIFF: return 3;
		case OP_LESS: return 3;
		case OP_LESS_EQ: return 3;
		case OP_GREATER: return 3;
		case OP_GREATER_EQ: return 3;
		case OP_LOAD: return 3;
		case OP_STORE: return 3;
		case OP_LABEL: return 1;
		case OP_JMP: return 1;
		case OP_JMP_FALSE: return 2;
		case OP_JMP_TRUE: return 2;
	}
	assert(false);
};

void print_chunk(FILE* fd, const Chunk& chunk) {
	for (size_t i = 0; i < chunk.size(); i++) {
		Instruction inst = chunk[i];

		if (inst.opcode != OP_LABEL) fprintf(fd, "  ");
		fprintf(fd, "%s", opcode_repr(inst.opcode));
		if (opcode_opnd_count(inst.opcode) >= 1) {
			fprintf(fd, " ");
			print_operand(fd, inst.operands[0]);
		}
		if (inst.opcode == OP_LOAD || inst.opcode == OP_STORE) {
			fprintf(fd, ", ");
			print_operand(fd, inst.operands[1]);
			fprintf(fd, "(");

			Operand opnd = inst.operands[2];
			if (opnd.value.reg.type == Register::VAL_NUM)
				fprintf(fd, "%zu", opnd.value.reg.index);
			else
				fprintf(fd, "%%r%zu", opnd.value.reg.index);

			fprintf(fd, ")");
			fprintf(fd, "\n");
			continue;
		}
		if (opcode_opnd_count(inst.opcode) >= 2) {
			fprintf(fd, ", ");
			print_operand(fd, inst.operands[1]);
		}
		if (opcode_opnd_count(inst.opcode) == 3) {
			fprintf(fd, ", ");
			print_operand(fd, inst.operands[2]);
		}
		fprintf(fd, "\n");
	}
}

Chunk Compiler::compile(AST ast, const SymbolTable& syms) {
	Chunk chunk;
	Operand heap = {Operand::OPND_REG, {{Register::VAL_NUM, reg_count++}}};
	Operand start = {Operand::OPND_NUM, {1024 - 1}};

	emit(&chunk, OP_MOV, heap, start);
	compile(ast.root, &syms, &chunk);
	return chunk;
}

Operand Compiler::get_temporary() {
	return {Operand::OPND_TMP, {{Register::VAL_NUM, tmp_count++}}};
}

Operand Compiler::get_label() { return {Operand::OPND_LAB, label_count++}; }

static Operand compile_builtin_write(
	Compiler*, Chunk* chunk, size_t argc, Operand* args
) {
	for (size_t i = 0; i < argc; i++)
		emit(
			chunk,
			((args[i].type == Operand::OPND_STR) ? OP_PRINTF : OP_PRINTV),
			args[i]
		);
	return {};
}

static Operand
compile_builtin_read(Compiler* comp, Chunk* chunk, size_t, Operand*) {
	Operand tmp = comp->get_temporary();
	emit(chunk, OP_READ, tmp);
	return tmp;
}

// create array with runtime-known size
static Operand compile_builtin_array(
	Compiler* comp, Chunk* chunk, size_t argc, Operand* args
) {
	if (!(argc == 1))
		err("The `array' builtin expects a size as the first and only argument.");
	Operand addr {Operand::OPND_TMP, {{Register::VAL_ADDR, comp->tmp_count++}}};
	Operand heap {Operand::OPND_REG, Register {Register::VAL_NUM, 0}};

	emit(chunk, OP_MOV, addr, heap);
	emit(chunk, OP_ADD, heap, heap, args[0]);

	return addr;
}

Operand Compiler::compile(Node node, const SymbolTable* syms, Chunk* chunk) {
	switch (node.type) {
		case AST_APP: {
			Node func_node = node.children[0];
			String func_name = sym_table_get((SymbolTable*)syms, func_node.index);
			Node args_node = node.children[1];

			Operand* args = new Operand[args_node.children_count];
			for (size_t i = 0; i < args_node.children_count; i++)
				args[i] = compile(args_node.children[i], syms, chunk);

			Operand res;
			if (strcmp(func_name, "write") == 0)
				res =
					compile_builtin_write(this, chunk, args_node.children_count, args);
			else if (strcmp(func_name, "read") == 0)
				res = compile_builtin_read(this, chunk, args_node.children_count, args);
			else if (strcmp(func_name, "array") == 0)
				res =
					compile_builtin_array(this, chunk, args_node.children_count, args);
			else {
				Operand* func_opnd = comp_env_find(this, func_node.index);
				assert(func_opnd);
				assert(func_opnd->type == Operand::OPND_FUN);
				Funktion func = func_opnd->value.fun;
				comp_env_push(this);
				for (size_t i = 0; i < func.argc; i++) {
					Node arg = func.args[i];
					assert(arg.type == AST_ID);
					Operand* arg_opnd = comp_env_get_new(this, arg.index);
					*arg_opnd = args[i];
				}

				res = compile(func.root, syms, chunk);
				comp_env_pop(this);
			}

			delete[] args;
			return res;
		}
		case AST_NUM: return {Operand::OPND_NUM, node.num};
		case AST_BLK: {
			comp_env_push(this);
			Operand opnd;
			for (size_t i = 0; i < node.children_count; i++)
				opnd = compile(node.children[i], syms, chunk);
			comp_env_pop(this);
			return opnd;
		}
		case AST_IF: {
			Node cond = node.children[0];
			Node yes = node.children[1];
			Node no = node.children[2];

			Operand l1 = get_label();
			Operand l2 = get_label();
			Operand res = get_temporary();

			Operand cond_opnd = compile(cond, syms, chunk);

			emit(chunk, OP_JMP_FALSE, cond_opnd, l1);

			Operand yes_opnd = compile(yes, syms, chunk);

			emit(chunk, OP_MOV, res, yes_opnd);
			emit(chunk, OP_JMP, l2);
			emit(chunk, OP_LABEL, l1);

			Operand no_opnd = compile(no, syms, chunk);

			emit(chunk, OP_MOV, res, no_opnd);
			emit(chunk, OP_LABEL, l2);

			return res;
		}
		case AST_WHEN: {
			Node cond = node.children[0];
			Node yes = node.children[1];

			Operand l1 = get_label();
			Operand res = get_temporary();

			Operand cond_opnd = compile(cond, syms, chunk);

			emit(chunk, OP_MOV, res, {});
			emit(chunk, OP_JMP_FALSE, cond_opnd, l1);

			Operand yes_opnd = compile(yes, syms, chunk);

			emit(chunk, OP_MOV, res, yes_opnd);
			emit(chunk, OP_LABEL, l1);

			return res;
		}
		case AST_FOR: {
			bool with_step = node.children_count == 5;

			Node var_node = node.children[0];
			Node id = var_node.children[0];
			Node from_node = node.children[1];
			Node to_node = node.children[2];
			Node exp_node = node.children[3 + with_step];

			Operand beg = get_label();
			Operand inc = cnt_lab = get_label();
			Operand end = brk_lab = get_label();
			Operand cmp = get_temporary();
			Operand step = (with_step) ? compile(node.children[3], syms, chunk)
			                           : Operand {Operand::OPND_NUM, 1};

			comp_env_push(this);

			Operand from = compile(from_node, syms, chunk);
			Operand to = compile(to_node, syms, chunk);
			Operand* var = comp_env_get_new(this, id.index);

			back_patch_stack[back_patch_stack_len++] = 0;
			in_loop = true;

			emit(chunk, OP_MOV, *var, from);
			emit(chunk, OP_LABEL, beg);
			emit(chunk, OP_EQ, cmp, *var, to);
			emit(chunk, OP_JMP_TRUE, cmp, end);

			Operand exp = compile(exp_node, syms, chunk);
			chunk_backpatch(this, chunk, exp);

			emit(chunk, OP_LABEL, inc);
			emit(chunk, OP_ADD, *var, *var, step);
			emit(chunk, OP_JMP, beg);
			emit(chunk, OP_LABEL, end);

			back_patch_stack_len--;
			in_loop = false;

			comp_env_pop(this);

			return exp;
		}
		case AST_WHILE: {
			Node cond = node.children[0];
			Node exp = node.children[1];

			Operand beg = cnt_lab = get_label();
			Operand end = brk_lab = get_label();

			back_patch_stack[back_patch_stack_len++] = 0;
			in_loop = true;

			emit(chunk, OP_LABEL, beg);

			Operand cond_opnd = compile(cond, syms, chunk);

			emit(chunk, OP_JMP_FALSE, cond_opnd, end);

			Operand exp_opnd = compile(exp, syms, chunk);
			chunk_backpatch(this, chunk, exp_opnd);

			emit(chunk, OP_JMP, beg);
			emit(chunk, OP_LABEL, end);

			back_patch_stack_len--;
			in_loop = false;

			return exp_opnd;
		}
		case AST_BREAK: {
			if (!in_loop) err("Can't break outside of loops");
			if (!(node.children_count == 1))
				err("`break' requires a expression to evaluate the loop to");
			Operand res = compile(node.children[0], syms, chunk);
			emit(chunk, OP_MOV, {}, res);
			back_patch_stack[back_patch_stack_len - 1]++;
			back_patch[back_patch_len++] = chunk->size() - 1;
			emit(chunk, OP_JMP, brk_lab);
			return {};
		}
		case AST_CONTINUE: {
			if (!in_loop) err("can't continue outside of loops");
			if (!(node.children_count == 1))
				err("continue requires a expression to evaluate the loop to");
			Operand res = compile(node.children[0], syms, chunk);
			emit(chunk, OP_MOV, {}, res);
			back_patch_stack[back_patch_stack_len - 1]++;
			back_patch[back_patch_len++] = chunk->size() - 1;
			emit(chunk, OP_JMP, cnt_lab);
			return {};
		}
		case AST_ASS: {
			Node var = node.children[0];
			Node exp = node.children[1];
			Node id = var.children[0];

			Operand tmp = compile(exp, syms, chunk);
			Operand* reg = comp_env_find(this, id.index);
			if (!reg) err("Variable not found");

			if (var.children_count == 2) {
				// if register is the start of the array then say so, otherwise it is a
				// address to the register that contains the beggining of the array
				Operand base =
					(reg->value.reg.type == Register::VAL_NUM)
						? Operand {Operand::OPND_NUM, (Number)reg->value.reg.index}
						: *reg;
				Operand idx = compile(var.children[1], syms, chunk);
				emit(chunk, OP_STORE, tmp, idx, base);
				return tmp;
			}

			emit(chunk, OP_MOV, *reg, tmp);
			return tmp;
		}

#define BINARY_ARITH(OPCODE)                          \
	{                                                   \
		Node left_node = node.children[0];                \
		Node right_node = node.children[1];               \
                                                      \
		Operand left = compile(left_node, syms, chunk);   \
		Operand right = compile(right_node, syms, chunk); \
		Operand res = get_temporary();                    \
                                                      \
		emit(chunk, OPCODE, res, left, right);            \
		return res;                                       \
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
			Operand res = get_temporary();
			Operand inverse = compile(node.children[0], syms, chunk);
			emit(chunk, OP_NOT, res, inverse);
			return res;
		}
		case AST_ID: assert(false && "unreachable");
		case AST_STR:
			return Operand {
				Operand::OPND_STR, sym_table_get((SymbolTable*)syms, node.index)};
		case AST_DECL: {
			// fun f args = exp
			if (node.children_count == 3) {
				Node id = node.children[0];
				Node args = node.children[1];
				Node body = node.children[2];

				Operand* opnd = comp_env_get_new(this, id.index);
				opnd->type = Operand::OPND_FUN;
				opnd->value.fun = Funktion {args.children_count, args.children, body};

				return {};
			}

			Node var = node.children[0];
			Node id = var.children[0];

			// var id = exp
			if (node.children_count == 2) {
				Operand* opnd = comp_env_get_new(this, id.index);
				Operand tmp = compile(node.children[1], syms, chunk);
				if (tmp.type == Operand::OPND_TMP)
					opnd->value.reg.type = tmp.value.reg.type;
				emit(chunk, OP_MOV, *opnd, tmp);
				return {};
			}

			// var id [idx]
			if (var.children_count == 2) {
				Node size = var.children[1];
				if (!(size.type == AST_NUM))
					err(
						"Bracket declarations requires the size to be constant. Use the "
						"`array' built-in, instead."
					);
				Operand size_opnd = compile(size, syms, chunk);
				comp_env_get_new_offset(this, id.index, (size_t)size_opnd.value.num);
				return {};
			}

			// var id
			Operand* opnd = comp_env_get_new(this, id.index);
			emit(chunk, OP_MOV, *opnd, {});
			return {};
		}
		case AST_VAR: {
			Node id = node.children[0];
			Operand* reg = comp_env_find(this, id.index);
			if (!reg) err("Variable not previously declared");

			// id
			if (node.children_count == 1) return *reg;

			// id [idx]
			Operand res = this->get_temporary();
			Operand off = compile(node.children[1], syms, chunk);
			Operand base = (reg->value.reg.type == Register::VAL_NUM)
			               ? Operand {Operand::OPND_NUM, (Number)reg->value.reg.index}
			               : *reg;

			emit(chunk, OP_LOAD, res, off, base);
			return res;
		}
		case AST_NIL: return {};
		case AST_TRUE: return {Operand::OPND_NUM, 1};
		case AST_LET: {
			Node decls = node.children[0];
			Node exp = node.children[1];
			comp_env_push(this);

			for (size_t i = 0; i < decls.children_count; i++)
				(void)compile(decls.children[i], syms, chunk);

			Operand res = compile(exp, syms, chunk);

			comp_env_pop(this);
			return res;
		}
	}
	assert(false);
}
