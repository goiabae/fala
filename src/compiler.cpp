#include "compiler.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include "ast.h"
#include "str_pool.h"

Compiler::Compiler() {
	back_patch_stack = new size_t[32];
	back_patch = new size_t[32];
}

Compiler::~Compiler() {
	delete[] back_patch;
	delete[] back_patch_stack;
}

void err(const char* msg) {
	std::cout << "COMPILER_ERR: " << msg << std::endl;
	exit(1);
}

Chunk& Chunk::emit(
	InstructionOp opcode, Operand fst, Operand snd, Operand trd
) {
	m_vec.push_back(Instruction {opcode, {fst, snd, trd}, ""});
	return *this;
}

Chunk& Chunk::with_comment(std::string comment) {
	m_vec.back().comment = comment;
	return *this;
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
		chunk->m_vec[idx].operands[0] = dest;
	}
	back_patch_stack_len--;
}

int print_str(FILE* fd, const char* str) {
	int printed = 2; // because open and closing double quotes
	fputc('"', fd);
	char c;
	while ((c = *(str++))) {
		if (c == '\n')
			printed += fprintf(fd, "\\n");
		else if (c == '\t')
			printed += fprintf(fd, "\\t");
		else {
			fputc(c, fd);
			printed++;
		}
	}
	fputc('"', fd);
	return printed;
}

int print_operand(FILE* fd, Operand opnd) {
	switch (opnd.type) {
		case Operand::Type::NIL: return fprintf(fd, "0");
		case Operand::Type::TMP: return fprintf(fd, "%%t%zu", opnd.reg.index);
		case Operand::Type::REG: return fprintf(fd, "%%r%zu", opnd.reg.index);
		case Operand::Type::LAB: return fprintf(fd, "L%03zu", opnd.lab.id);
		case Operand::Type::STR: return print_str(fd, opnd.str);
		case Operand::Type::NUM: return fprintf(fd, "%d", opnd.num);
		case Operand::Type::FUN: assert(false && "unreachable");
	}
	return 0;
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
size_t opcode_opnd_count(InstructionOp op) {
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

int print_operand_indirect(FILE* fd, Operand opnd) {
	if (opnd.type == Operand::Type::NUM) {
		return fprintf(fd, "%d", opnd.num);
	} else if (opnd.type == Operand::Type::REG) {
		if (opnd.reg.has_num())
			return fprintf(fd, "%zu", opnd.reg.index);
		else if (opnd.reg.has_addr())
			return fprintf(fd, "%%r%zu", opnd.reg.index);
	} else if (opnd.type == Operand::Type::TMP) {
		if (opnd.reg.has_num())
			return fprintf(fd, "%zu", opnd.reg.index);
		else if (opnd.reg.has_addr())
			return fprintf(fd, "%%t%zu", opnd.reg.index);
	} else
		assert(false && "unreachable");
	return 0;
}

// print an indirect memory access instruction (OP_STORE or OP_LOAD), where
// the base and offset operands are printed differently depending on their
// contents.
int print_inst_indirect(FILE* fd, const Instruction& inst) {
	int printed = 0;
	printed += fprintf(fd, "  ");
	printed += fprintf(fd, "%s", opcode_repr(inst.opcode));
	printed += fprintf(fd, " ");
	printed += print_operand(fd, inst.operands[0]);
	printed += fprintf(fd, ", ");
	printed += print_operand_indirect(fd, inst.operands[1]);
	printed += fprintf(fd, "(");
	printed += print_operand_indirect(fd, inst.operands[2]);
	printed += fprintf(fd, ")");
	return printed;
}

int print_inst(FILE* fd, const Instruction& inst) {
	constexpr const char* separators[3] = {" ", ", ", ", "};
	int printed = 0;
	if (inst.opcode != OP_LABEL) printed += fprintf(fd, "    ");
	printed += fprintf(fd, "%s", opcode_repr(inst.opcode));
	for (size_t i = 0; i < opcode_opnd_count(inst.opcode); i++) {
		printed += fprintf(fd, "%s", separators[i]);
		printed += print_operand(fd, inst.operands[i]);
	}
	return printed;
}

void print_chunk(FILE* fd, const Chunk& chunk) {
	int max = 0;
	int printed = 0;
	for (const auto& inst : chunk.m_vec) {
		if (inst.opcode == OP_LOAD || inst.opcode == OP_STORE)
			printed = print_inst_indirect(fd, inst);
		else
			printed = print_inst(fd, inst);
		if (printed > max) max = printed;
		if (inst.comment != "") {
			for (int i = 0; i < (max - printed); i++) fputc(' ', fd);
			fprintf(fd, "  ; ");
			fprintf(fd, "%s", inst.comment.c_str());
		}
		fprintf(fd, "\n");
	}
}

Chunk Compiler::compile(AST ast, const StringPool& pool) {
	Chunk chunk;
	auto dyn_alloc_start = Operand(Register(reg_count++).as_addr()).as_reg();
	Operand start(1024 - 1);

	chunk.emit(OP_MOV, dyn_alloc_start, start)
		.with_comment("contains address to start of dynamic allocation region");
	compile(ast.root, pool, &chunk);
	return chunk;
}

Operand Compiler::make_temporary() {
	return Operand(Register(tmp_count++).as_num()).as_temp();
}

Operand Compiler::make_register() {
	return Operand(Register(reg_count++).as_num()).as_reg();
}

Operand Compiler::make_label() { return {Label {label_count++}}; }

Operand Compiler::builtin_write(Chunk* chunk, size_t argc, Operand args[]) {
	for (size_t i = 0; i < argc; i++) {
		const auto op = args[i].type == Operand::Type::STR ? OP_PRINTF : OP_PRINTV;
		chunk->emit(op, args[i]);
	}
	return {};
}

Operand Compiler::builtin_read(Chunk* chunk, size_t, Operand[]) {
	Operand tmp = make_temporary();
	chunk->emit(OP_READ, tmp);
	return tmp;
}

// create array with runtime-known size
Operand Compiler::builtin_array(Chunk* chunk, size_t argc, Operand args[]) {
	if (!(argc == 1))
		err("The `array' builtin expects a size as the first and only argument.");

	// if operand is number, array has constant size and is "inline" into the
	// statically allocated registers
	if (args[0].type == Operand::Type::NUM) {
		auto op = Operand(Register(reg_count).as_num()).as_reg();
		reg_count += (size_t)args[0].num;
		return op;
	} else {
		auto addr = Operand(Register(tmp_count++).as_addr()).as_temp();
		auto dyn = Operand(Register(0).as_num()).as_reg();

		chunk->emit(OP_MOV, addr, dyn).with_comment("allocating array");
		chunk->emit(OP_ADD, dyn, dyn, args[0]);
		return addr;
	}
}

Operand Compiler::to_rvalue(Chunk* chunk, Operand opnd) {
	if (opnd.is_register() && opnd.reg.has_addr()) {
		Operand tmp = make_temporary();
		chunk->emit(OP_LOAD, tmp, Operand(0), opnd)
			.with_comment("casting to rvalue");
		return tmp;
	} else {
		return opnd;
	}
}

Operand Compiler::compile(Node node, const StringPool& pool, Chunk* chunk) {
	switch (node.type) {
		case AST_APP: {
			Node func_node = node.branch.children[0];
			const char* func_name = pool.find(func_node.str_id);
			Node args_node = node.branch.children[1];

			Operand* args = new Operand[args_node.branch.children_count];
			for (size_t i = 0; i < args_node.branch.children_count; i++)
				args[i] =
					to_rvalue(chunk, compile(args_node.branch.children[i], pool, chunk));

			Operand res;
			if (strcmp(func_name, "write") == 0)
				res = builtin_write(chunk, args_node.branch.children_count, args);
			else if (strcmp(func_name, "read") == 0)
				res = builtin_read(chunk, args_node.branch.children_count, args);
			else if (strcmp(func_name, "array") == 0)
				res = builtin_array(chunk, args_node.branch.children_count, args);
			else {
				Operand* func_opnd = env.find(func_node.str_id);
				if (!func_opnd) err("Function not found");
				if (func_opnd->type != Operand::Type::FUN)
					err("Type of <name> is not function.");
				Funktion func = func_opnd->fun;
				{
					auto scope = env.make_scope();
					for (size_t i = 0; i < func.argc; i++) {
						Node arg = func.args[i];
						assert(arg.type == AST_ID);
						Operand* arg_opnd = env.insert(arg.str_id, make_register());
						*arg_opnd = args[i];
					}

					res = compile(func.root, pool, chunk);
				}
			}

			delete[] args;
			return res;
		}
		case AST_NUM: return Operand(node.num);
		case AST_BLK: {
			auto scope = env.make_scope();
			Operand opnd;
			for (size_t i = 0; i < node.branch.children_count; i++)
				opnd = compile(node.branch.children[i], pool, chunk);
			return opnd;
		}
		case AST_IF: {
			Node cond = node.branch.children[0];
			Node yes = node.branch.children[1];
			Node no = node.branch.children[2];

			Operand l1 = make_label();
			Operand l2 = make_label();
			Operand res = make_temporary();

			Operand cond_opnd = to_rvalue(chunk, compile(cond, pool, chunk));

			chunk->emit(OP_JMP_FALSE, cond_opnd, l1).with_comment("if branch");

			Operand yes_opnd = compile(yes, pool, chunk);

			chunk->emit(OP_MOV, res, yes_opnd);
			chunk->emit(OP_JMP, l2);
			chunk->emit(OP_LABEL, l1).with_comment("else branch");

			Operand no_opnd = compile(no, pool, chunk);

			chunk->emit(OP_MOV, res, no_opnd);
			chunk->emit(OP_LABEL, l2);

			return res;
		}
		case AST_WHEN: {
			Node cond = node.branch.children[0];
			Node yes = node.branch.children[1];

			Operand l1 = make_label();
			Operand res = make_temporary();

			Operand cond_opnd = to_rvalue(chunk, compile(cond, pool, chunk));

			chunk->emit(OP_MOV, res, {}).with_comment("when conditional");
			chunk->emit(OP_JMP_FALSE, cond_opnd, l1);

			Operand yes_opnd = compile(yes, pool, chunk);

			chunk->emit(OP_MOV, res, yes_opnd);
			chunk->emit(OP_LABEL, l1);

			return res;
		}
		case AST_FOR: {
			Node decl_node = node.branch.children[0];
			Node to_node = node.branch.children[1];
			Node step_node = node.branch.children[2];
			Node exp_node = node.branch.children[3];

			Operand beg = make_label();
			Operand inc = cnt_lab = make_label();
			Operand end = brk_lab = make_label();
			Operand cmp = make_temporary();
			Operand step = (step_node.type != AST_EMPTY)
			               ? to_rvalue(chunk, compile(step_node, pool, chunk))
			               : Operand(1);

			auto scope = env.make_scope();

			Operand var = compile(decl_node, pool, chunk);
			if (!var.is_register()) err("Declaration must be of a number lvalue");

			Operand to = to_rvalue(chunk, compile(to_node, pool, chunk));

			back_patch_stack[back_patch_stack_len++] = 0;
			in_loop = true;

			chunk->emit(OP_LABEL, beg).with_comment("beginning of for loop");
			chunk->emit(OP_EQ, cmp, var, to);
			chunk->emit(OP_JMP_TRUE, cmp, end);

			Operand exp = compile(exp_node, pool, chunk);

			chunk->emit(OP_LABEL, inc);
			chunk->emit(OP_ADD, var, var, step);
			chunk->emit(OP_JMP, beg);
			chunk->emit(OP_LABEL, end).with_comment("end of for loop");

			back_patch_jumps(chunk, exp);
			in_loop = false;

			return exp;
		}
		case AST_WHILE: {
			Operand beg = cnt_lab = make_label();
			Operand end = brk_lab = make_label();

			back_patch_stack[back_patch_stack_len++] = 0;
			in_loop = true;

			chunk->emit(OP_LABEL, beg).with_comment("beginning of while loop");

			Operand cond =
				to_rvalue(chunk, compile(node.branch.children[0], pool, chunk));

			chunk->emit(OP_JMP_FALSE, cond, end);

			Operand exp =
				to_rvalue(chunk, compile(node.branch.children[1], pool, chunk));

			chunk->emit(OP_JMP, beg);
			chunk->emit(OP_LABEL, end).with_comment("end of while loop");

			back_patch_jumps(chunk, exp);
			in_loop = false;

			return exp;
		}
		case AST_BREAK: {
			if (!in_loop) err("Can't break outside of loops");
			if (!(node.branch.children_count == 1))
				err("`break' requires a expression to evaluate the loop to");

			Operand res =
				to_rvalue(chunk, compile(node.branch.children[0], pool, chunk));
			chunk->emit(OP_MOV, {}, res);
			push_to_back_patch(chunk->m_vec.size() - 1);
			chunk->emit(OP_JMP, brk_lab).with_comment("break out of loop");
			return {};
		}
		case AST_CONTINUE: {
			if (!in_loop) err("can't continue outside of loops");
			if (!(node.branch.children_count == 1))
				err("continue requires a expression to evaluate the loop to");

			Operand res =
				to_rvalue(chunk, compile(node.branch.children[0], pool, chunk));
			chunk->emit(OP_MOV, {}, res);
			push_to_back_patch(chunk->m_vec.size() - 1);
			chunk->emit(OP_JMP, cnt_lab)
				.with_comment("continue to next iteration of loop");
			return {};
		}
		case AST_ASS: {
			Operand cell = compile(node.branch.children[0], pool, chunk);
			if (!cell.is_register())
				err("Left-hand side of assignment must be an lvalue");

			Operand exp =
				to_rvalue(chunk, compile(node.branch.children[1], pool, chunk));

			if (cell.reg.has_addr())
				chunk->emit(OP_STORE, exp, Operand(0), cell)
					.with_comment("assigning to array variable");
			else if (cell.reg.has_num())
				chunk->emit(OP_MOV, cell, exp)
					.with_comment("assigning to integer variable");

			return exp;
		}

#define BINARY_ARITH(OPCODE)                                            \
	{                                                                     \
		Node left_node = node.branch.children[0];                           \
		Node right_node = node.branch.children[1];                          \
                                                                        \
		Operand left = to_rvalue(chunk, compile(left_node, pool, chunk));   \
		Operand right = to_rvalue(chunk, compile(right_node, pool, chunk)); \
		Operand res = make_temporary();                                     \
                                                                        \
		chunk->emit(OPCODE, res, left, right);                              \
		return res;                                                         \
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
			Operand res = make_temporary();
			Operand inverse =
				to_rvalue(chunk, compile(node.branch.children[0], pool, chunk));
			chunk->emit(OP_NOT, res, inverse);
			return res;
		}
		case AST_AT: {
			// evaluates to a temporary register containing the address of the lvalue

			Operand base = compile(node.branch.children[0], pool, chunk);
			Operand off = compile(node.branch.children[1], pool, chunk);

			if (!base.is_register()) err("Base must be an lvalue");

			Operand tmp = make_temporary();
			if (base.reg.has_num())
				chunk->emit(OP_ADD, tmp, Operand((Number)base.reg.index), off)
					.with_comment("accessing inline array");
			else if (base.reg.has_addr())
				chunk->emit(OP_ADD, tmp, base, off)
					.with_comment("accessing allocated array");

			return Operand(tmp.reg.as_addr()).as_temp();
		}
		case AST_ID: {
			Operand* opnd = env.find(node.str_id);
			if (opnd == nullptr) err("Variable not found");
			return *opnd;
		}
		case AST_STR: return Operand(pool.find(node.str_id));
		case AST_DECL: {
			Node id = node.branch.children[0];

			// fun f args = exp
			if (node.branch.children_count == 3) {
				Node args = node.branch.children[1];
				Node body = node.branch.children[2];

				Operand* opnd = env.insert(id.str_id, make_register());
				*opnd = Operand(
					Funktion {args.branch.children_count, args.branch.children, body}
				);
				return *opnd;
			}

			// initial value or nil
			Operand initial =
				(node.branch.children_count == 2)
					? to_rvalue(chunk, compile(node.branch.children[1], pool, chunk))
					: Operand();

			// is a statically allocated array
			if (initial.type == Operand::Type::REG && initial.reg.has_num())
				return *env.insert(id.str_id, initial);

			Operand* opnd = env.insert(id.str_id, make_register());
			if (initial.type == Operand::Type::TMP) opnd->reg.type = initial.reg.type;

			chunk->emit(OP_MOV, *opnd, initial).with_comment("initializing variable");
			return *opnd;
		}
		case AST_NIL: return {};
		case AST_TRUE: return {1};
		case AST_LET: {
			Node decls = node.branch.children[0];
			Node exp = node.branch.children[1];
			{
				auto scope = env.make_scope();
				for (size_t i = 0; i < decls.branch.children_count; i++)
					(void)compile(decls.branch.children[i], pool, chunk);
				Operand res = compile(exp, pool, chunk);
				return res;
			}
		}
		case AST_EMPTY: assert(false && "unreachable");
	}
	assert(false);
}
