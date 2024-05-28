#include "compiler.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include "ast.h"
#include "str_pool.h"

#define CHUNK_INST_CAP 1024

#define emit(C, OP, ...) chunk_append(C, Instruction {OP, {__VA_ARGS__}})

static void chunk_append(Chunk* chunk, Instruction inst);
static void print_operand(FILE*, Operand);

Compiler::Compiler() {
	back_patch_stack = new size_t[32];
	back_patch = new size_t[32];
	env.push_back({});
}

Compiler::~Compiler() {
	delete[] back_patch;
	delete[] back_patch_stack;
}

void err(const char* msg) {
	std::cout << "COMPILER_ERR: " << msg << std::endl;
	exit(1);
}

Compiler::Scope Compiler::create_scope() { return Scope(&env); }

Operand* Compiler::env_get_new(StrID str_id, Operand value) {
	env.back().push_back({str_id.idx, value});
	Operand& op = env.back().back().second;
	return &op;
}

Operand* Compiler::env_find(StrID str_id) {
	for (size_t i = env.size(); i-- > 0;)
		for (size_t j = env[i].size(); j-- > 0;)
			if (env[i][j].first == str_id.idx) return &env[i][j].second;
	return nullptr;
}

static void chunk_append(Chunk* chunk, Instruction inst) {
	if (!(chunk->size() + 1 <= CHUNK_INST_CAP))
		err("Max number of instructions exceeded");
	chunk->push_back(inst);
}

// push instruction at index id of a chunk to back patch
void Compiler::push_to_back_patch(size_t idx) {
	back_patch_stack[back_patch_stack_len - 1]++;
	back_patch[back_patch_len++] = idx - 1;
}

// back patch destination of MOVs in control-flow jump expressions
void Compiler::back_patch_jumps(Chunk* chunk, Operand dest) {
	// for the inner-most loop (top of the "stack"):
	//   patch the destination operand of MOV instructions
	size_t to_patch = back_patch_stack[back_patch_stack_len - 1];
	for (size_t i = 0; i < to_patch; i++) {
		size_t idx = back_patch[back_patch_len-- - 1];
		(*chunk)[idx].operands[0] = dest;
	}
}

static void print_str(FILE* fd, const char* str) {
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

// return textual representation of opcode
const char* opcode_repr(InstructionOp op) {
	switch (op) {
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

// return amount of operands of each opcode
int opcode_opnd_count(InstructionOp op) {
	switch (op) {
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
			if (opnd.type == Operand::OPND_NUM) {
				fprintf(fd, "%d", opnd.value.num);
			} else {
				if (opnd.value.reg.type == Register::VAL_NUM)
					fprintf(fd, "%zu", opnd.value.reg.index);
				else
					fprintf(fd, "%%r%zu", opnd.value.reg.index);
			}
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

Chunk Compiler::compile(AST ast, const StringPool& pool) {
	Chunk chunk;
	Operand heap {Operand::OPND_REG, Register(reg_count++).as_num()};
	Operand start = {Operand::OPND_NUM, {1024 - 1}};

	emit(&chunk, OP_MOV, heap, start);
	compile(ast.root, pool, &chunk);
	return chunk;
}

Operand Compiler::get_temporary() {
	return {Operand::OPND_TMP, Register(tmp_count++).as_num()};
}

Operand Compiler::get_register() {
	return {Operand::OPND_REG, Register(reg_count++).as_num()};
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

Operand compile_builtin_read(Compiler* comp, Chunk* chunk, size_t, Operand*) {
	Operand tmp = comp->get_temporary();
	emit(chunk, OP_READ, tmp);
	return tmp;
}

// create array with runtime-known size
Operand compile_builtin_array(
	Compiler* comp, Chunk* chunk, size_t argc, Operand args[]
) {
	if (!(argc == 1))
		err("The `array' builtin expects a size as the first and only argument.");

	if (args[0].type == Operand::Type::OPND_NUM) {
		// array is of constant size
		Operand op {Operand::OPND_REG, Register(comp->reg_count).as_num()};
		comp->reg_count += (size_t)args[0].value.num;
		return op;
	} else {
		Operand addr {Operand::OPND_TMP, Register(comp->tmp_count++).as_addr()};
		Operand heap {Operand::OPND_REG, Register(0).as_num()};

		emit(chunk, OP_MOV, addr, heap);
		emit(chunk, OP_ADD, heap, heap, args[0]);
		return addr;
	}
}

Operand Compiler::compile(Node node, const StringPool& pool, Chunk* chunk) {
	switch (node.type) {
		case AST_APP: {
			Node func_node = node.children[0];
			const char* func_name = pool.find(func_node.str_id);
			Node args_node = node.children[1];

			Operand* args = new Operand[args_node.children_count];
			for (size_t i = 0; i < args_node.children_count; i++)
				args[i] = compile(args_node.children[i], pool, chunk);

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
				Operand* func_opnd = env_find(func_node.str_id);
				assert(func_opnd);
				assert(func_opnd->type == Operand::OPND_FUN);
				Funktion func = func_opnd->value.fun;
				{
					Scope scope = create_scope();
					for (size_t i = 0; i < func.argc; i++) {
						Node arg = func.args[i];
						assert(arg.type == AST_ID);
						Operand* arg_opnd = env_get_new(arg.str_id, get_register());
						*arg_opnd = args[i];
					}

					res = compile(func.root, pool, chunk);
				}
			}

			delete[] args;
			return res;
		}
		case AST_NUM: return {Operand::OPND_NUM, node.num};
		case AST_BLK: {
			Scope scope = create_scope();
			Operand opnd;
			for (size_t i = 0; i < node.children_count; i++)
				opnd = compile(node.children[i], pool, chunk);
			return opnd;
		}
		case AST_IF: {
			Node cond = node.children[0];
			Node yes = node.children[1];
			Node no = node.children[2];

			Operand l1 = get_label();
			Operand l2 = get_label();
			Operand res = get_temporary();

			Operand cond_opnd = compile(cond, pool, chunk);

			emit(chunk, OP_JMP_FALSE, cond_opnd, l1);

			Operand yes_opnd = compile(yes, pool, chunk);

			emit(chunk, OP_MOV, res, yes_opnd);
			emit(chunk, OP_JMP, l2);
			emit(chunk, OP_LABEL, l1);

			Operand no_opnd = compile(no, pool, chunk);

			emit(chunk, OP_MOV, res, no_opnd);
			emit(chunk, OP_LABEL, l2);

			return res;
		}
		case AST_WHEN: {
			Node cond = node.children[0];
			Node yes = node.children[1];

			Operand l1 = get_label();
			Operand res = get_temporary();

			Operand cond_opnd = compile(cond, pool, chunk);

			emit(chunk, OP_MOV, res, {});
			emit(chunk, OP_JMP_FALSE, cond_opnd, l1);

			Operand yes_opnd = compile(yes, pool, chunk);

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
			Operand step = (with_step) ? compile(node.children[3], pool, chunk)
			                           : Operand {Operand::OPND_NUM, 1};

			Scope scope = create_scope();

			Operand from = compile(from_node, pool, chunk);
			Operand to = compile(to_node, pool, chunk);
			Operand* var = env_get_new(id.str_id, get_register());

			back_patch_stack[back_patch_stack_len++] = 0;
			in_loop = true;

			emit(chunk, OP_MOV, *var, from);
			emit(chunk, OP_LABEL, beg);
			emit(chunk, OP_EQ, cmp, *var, to);
			emit(chunk, OP_JMP_TRUE, cmp, end);

			Operand exp = compile(exp_node, pool, chunk);
			back_patch_jumps(chunk, exp);

			emit(chunk, OP_LABEL, inc);
			emit(chunk, OP_ADD, *var, *var, step);
			emit(chunk, OP_JMP, beg);
			emit(chunk, OP_LABEL, end);

			back_patch_stack_len--;
			in_loop = false;

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

			Operand cond_opnd = compile(cond, pool, chunk);

			emit(chunk, OP_JMP_FALSE, cond_opnd, end);

			Operand exp_opnd = compile(exp, pool, chunk);
			back_patch_jumps(chunk, exp_opnd);

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

			Operand res = compile(node.children[0], pool, chunk);
			emit(chunk, OP_MOV, {}, res);
			push_to_back_patch(chunk->size() - 1);
			emit(chunk, OP_JMP, brk_lab);
			return {};
		}
		case AST_CONTINUE: {
			if (!in_loop) err("can't continue outside of loops");
			if (!(node.children_count == 1))
				err("continue requires a expression to evaluate the loop to");

			Operand res = compile(node.children[0], pool, chunk);
			emit(chunk, OP_MOV, {}, res);
			push_to_back_patch(chunk->size() - 1);
			emit(chunk, OP_JMP, cnt_lab);
			return {};
		}
		case AST_ASS: {
			Node var = node.children[0];
			Node exp = node.children[1];
			Node id = var.children[0];

			Operand tmp = compile(exp, pool, chunk);
			Operand* reg = env_find(id.str_id);
			if (!reg) err("Variable not found");

			if (var.children_count == 2) {
				// if register is the start of the array then say so, otherwise it is a
				// address to the register that contains the beggining of the array
				Operand base =
					(reg->value.reg.type == Register::VAL_NUM)
						? Operand {Operand::OPND_NUM, (Number)reg->value.reg.index}
						: *reg;
				Operand idx = compile(var.children[1], pool, chunk);
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
		Operand left = compile(left_node, pool, chunk);   \
		Operand right = compile(right_node, pool, chunk); \
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
			Operand inverse = compile(node.children[0], pool, chunk);
			emit(chunk, OP_NOT, res, inverse);
			return res;
		}
		case AST_ID: assert(false && "unreachable");
		case AST_STR: return Operand {Operand::OPND_STR, pool.find(node.str_id)};
		case AST_DECL: {
			// fun f args = exp
			if (node.children_count == 3) {
				Node id = node.children[0];
				Node args = node.children[1];
				Node body = node.children[2];

				Operand* opnd = env_get_new(id.str_id, get_register());
				opnd->type = Operand::OPND_FUN;
				opnd->value.fun = Funktion {args.children_count, args.children, body};

				return {};
			}

			Node id = node.children[0];

			// var id = exp
			if (node.children_count == 2) {
				Operand exp = compile(node.children[1], pool, chunk);
				Operand* opnd;
				if (exp.type == Operand::OPND_REG)
					opnd = env_get_new(id.str_id, exp);
				else {
					Operand tmp = get_register();
					opnd = env_get_new(id.str_id, tmp);
					opnd->value.reg.type = exp.value.reg.type;
					emit(chunk, OP_MOV, *opnd, exp);
				}
				return {};
			}

			// var id
			Operand* opnd = env_get_new(id.str_id, get_register());
			emit(chunk, OP_MOV, *opnd, {});
			return {};
		}
		case AST_VAR: {
			Node id = node.children[0];
			Operand* reg = env_find(id.str_id);
			if (!reg) err("Variable not previously declared");

			// id
			if (node.children_count == 1) return *reg;

			// id [idx]
			Operand res = get_temporary();
			Operand off = compile(node.children[1], pool, chunk);
			Operand base = [&] {
				if (reg->value.reg.type == Register::VAL_ADDR) {
					return Operand(Operand::OPND_TMP, reg->value.reg);
				} else {
					return Operand {Operand::OPND_NUM, (Number)reg->value.reg.index};
				}
			}();

			emit(chunk, OP_LOAD, res, off, base);
			return res;
		}
		case AST_NIL: return {};
		case AST_TRUE: return {Operand::OPND_NUM, 1};
		case AST_LET: {
			Node decls = node.children[0];
			Node exp = node.children[1];
			{
				Scope scope = create_scope();
				for (size_t i = 0; i < decls.children_count; i++)
					(void)compile(decls.children[i], pool, chunk);
				Operand res = compile(exp, pool, chunk);
				return res;
			}
		}
	}
	assert(false);
}
