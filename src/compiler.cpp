#include "compiler.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include "ast.h"
#include "bytecode.hpp"
#include "str_pool.h"

using bytecode::Opcode;
using bytecode::Register;

void err(const char* msg) {
	std::cout << "COMPILER_ERR: " << msg << std::endl;
	exit(1);
}

// push instruction at index id of a chunk to back patch
void Compiler::push_to_back_patch(size_t idx) {
	back_patch_count.top()++;
	back_patch.push(idx - 1);
}

// back patch destination of MOVs in control-flow jump expressions
void Compiler::back_patch_jumps(Chunk* chunk, Operand dest) {
	// for the inner-most loop (top of the "stack"):
	//   patch the destination operand of MOV instructions
	size_t to_patch = back_patch_count.top();
	back_patch_count.pop();
	for (size_t i = 0; i < to_patch; i++) {
		size_t idx = back_patch.top();
		back_patch.pop();
		chunk->m_vec[idx].operands[0] = dest;
	}
}

Chunk Compiler::compile(AST ast, const StringPool& pool) {
	Chunk chunk;

	auto dyn = make_register();
	chunk
		.emit(Opcode::MOV, dyn, {}) // snd will be patched after compilation
		.with_comment("contains address to start of the last allocated region");

	compile(ast.root, pool, &chunk);

	Operand start(dyn_alloc_start);
	chunk.m_vec[0].operands[1] =
		start; // backpatching snd after static allocations

	return chunk;
}

Operand Compiler::make_temporary() {
	return Operand(Register(tmp_count++).as_num()).as_temp();
}

Operand Compiler::make_register() {
	return Operand(Register(reg_count++).as_num()).as_reg();
}

Operand Compiler::make_label() { return {bytecode::Label {label_count++}}; }

Operand Compiler::builtin_write_int(Chunk* chunk, size_t argc, Operand args[]) {
	if (argc != 1)
		err(
			"write_int accepts only a single pointer to character or integer as an "
			"argument"
		);

	auto& op = args[0];
	assert(!(op.is_register() && op.reg.has_addr()));
	chunk->emit(Opcode::PRINTV, op);
	return {};
}

Operand Compiler::builtin_write_char(
	Chunk* chunk, size_t argc, Operand args[]
) {
	if (argc != 1)
		err(
			"write_int accepts only a single pointer to character or integer as an "
			"argument"
		);

	auto& op = args[0];
	assert(!(op.is_register() && op.reg.has_addr()));
	chunk->emit(Opcode::PRINTC, op);
	return {};
}

Operand Compiler::builtin_write_str(Chunk* chunk, size_t argc, Operand args[]) {
	if (argc != 1)
		err(
			"write_str accepts only a single pointer to character or integer as an "
			"argument"
		);

	auto& op = args[0];
	assert(op.is_register() && op.reg.has_addr());
	chunk->emit(Opcode::PRINTF, op);
	return {};
}

Operand Compiler::builtin_read_int(Chunk* chunk, size_t, Operand[]) {
	Operand tmp = make_temporary();
	chunk->emit(Opcode::READV, tmp);
	return tmp;
}

Operand Compiler::builtin_read_char(Chunk* chunk, size_t, Operand[]) {
	Operand tmp = make_temporary();
	chunk->emit(Opcode::READC, tmp);
	return tmp;
}

Operand Compiler::builtin_array(Chunk* chunk, size_t argc, Operand args[]) {
	if (!(argc == 1))
		err("The `array' builtin expects a size as the first and only argument.");

	// if size if constant we subtract the allocation start at compile time
	if (args[0].type == Operand::Type::NUM) {
		auto addr = Operand(Register(reg_count++).as_addr()).as_reg();
		auto dyn = Operand(dyn_alloc_start -= args[0].num);

		chunk->emit(Opcode::MOV, addr, dyn).with_comment("static array");
		return addr;

	} else {
		auto addr = Operand(Register(reg_count++).as_addr()).as_reg();
		auto dyn = Operand(Register(0).as_num()).as_reg();

		chunk->emit(Opcode::SUB, dyn, dyn, args[0]);
		chunk->emit(Opcode::MOV, addr, dyn).with_comment("allocating array");
		return addr;
	}
}

Operand Compiler::to_rvalue(Chunk* chunk, Operand opnd) {
	if (opnd.is_register() && opnd.reg.has_addr()) {
		Operand tmp = make_temporary();
		chunk->emit(Opcode::LOAD, tmp, Operand(0), opnd)
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
			for (size_t i = 0; i < args_node.branch.children_count; i++) {
				auto& arg_node = args_node.branch.children[i];
				args[i] = compile(arg_node, pool, chunk);

				if (arg_node.type == AST_PATH) args[i] = to_rvalue(chunk, args[i]);
			}

			Operand res;
			if (strcmp(func_name, "write_int") == 0)
				res = builtin_write_int(chunk, args_node.branch.children_count, args);
			else if (strcmp(func_name, "write_char") == 0)
				res = builtin_write_char(chunk, args_node.branch.children_count, args);
			else if (strcmp(func_name, "write_str") == 0)
				res = builtin_write_str(chunk, args_node.branch.children_count, args);
			else if (strcmp(func_name, "read_int") == 0)
				res = builtin_read_int(chunk, args_node.branch.children_count, args);
			else if (strcmp(func_name, "read_char") == 0)
				res = builtin_read_char(chunk, args_node.branch.children_count, args);
			else if (strcmp(func_name, "array") == 0)
				res = builtin_array(chunk, args_node.branch.children_count, args);
			else {
				Operand* func_opnd = env.find(func_node.str_id);
				if (!func_opnd) err("Function not found");
				if (func_opnd->type != Operand::Type::FUN)
					err("Type of <name> is not function.");
				auto func = func_opnd->fun;
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

			chunk->emit(Opcode::JMP_FALSE, cond_opnd, l1).with_comment("if branch");

			Operand yes_opnd = compile(yes, pool, chunk);

			chunk->emit(Opcode::MOV, res, yes_opnd);
			chunk->emit(Opcode::JMP, l2);
			chunk->add_label(l1);

			Operand no_opnd = compile(no, pool, chunk);

			chunk->emit(Opcode::MOV, res, no_opnd);
			chunk->add_label(l2);

			return res;
		}
		case AST_WHEN: {
			Node cond = node.branch.children[0];
			Node yes = node.branch.children[1];

			Operand l1 = make_label();
			Operand res = make_temporary();

			Operand cond_opnd = to_rvalue(chunk, compile(cond, pool, chunk));

			chunk->emit(Opcode::MOV, res, {}).with_comment("when conditional");
			chunk->emit(Opcode::JMP_FALSE, cond_opnd, l1);

			Operand yes_opnd = compile(yes, pool, chunk);

			chunk->emit(Opcode::MOV, res, yes_opnd);
			chunk->add_label(l1);

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

			back_patch_count.push(0);
			in_loop = true;

			chunk->add_label(beg);
			chunk->emit(Opcode::EQ, cmp, var, to);
			chunk->emit(Opcode::JMP_TRUE, cmp, end);

			Operand exp = compile(exp_node, pool, chunk);

			chunk->add_label(inc);
			chunk->emit(Opcode::ADD, var, var, step);
			chunk->emit(Opcode::JMP, beg);
			chunk->add_label(end);

			back_patch_jumps(chunk, exp);
			in_loop = false;

			return exp;
		}
		case AST_WHILE: {
			Operand beg = cnt_lab = make_label();
			Operand end = brk_lab = make_label();

			back_patch_count.push(0);
			in_loop = true;

			chunk->add_label(beg);

			Operand cond =
				to_rvalue(chunk, compile(node.branch.children[0], pool, chunk));

			chunk->emit(Opcode::JMP_FALSE, cond, end);

			Operand exp =
				to_rvalue(chunk, compile(node.branch.children[1], pool, chunk));

			chunk->emit(Opcode::JMP, beg);
			chunk->add_label(end);

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
			chunk->emit(Opcode::MOV, {}, res);
			push_to_back_patch(chunk->m_vec.size() - 1);
			chunk->emit(Opcode::JMP, brk_lab).with_comment("break out of loop");
			return {};
		}
		case AST_CONTINUE: {
			if (!in_loop) err("can't continue outside of loops");
			if (!(node.branch.children_count == 1))
				err("continue requires a expression to evaluate the loop to");

			Operand res =
				to_rvalue(chunk, compile(node.branch.children[0], pool, chunk));
			chunk->emit(Opcode::MOV, {}, res);
			push_to_back_patch(chunk->m_vec.size() - 1);
			chunk->emit(Opcode::JMP, cnt_lab)
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
				chunk->emit(Opcode::STORE, exp, Operand(0), cell)
					.with_comment("assigning to array variable");
			else // contains number
				chunk->emit(Opcode::MOV, cell, exp)
					.with_comment("assigning to variable");

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

		case AST_OR: BINARY_ARITH(Opcode::OR);
		case AST_AND: BINARY_ARITH(Opcode::AND);
		case AST_GTN: BINARY_ARITH(Opcode::GREATER);
		case AST_LTN: BINARY_ARITH(Opcode::LESS);
		case AST_GTE: BINARY_ARITH(Opcode::GREATER_EQ);
		case AST_LTE: BINARY_ARITH(Opcode::LESS_EQ);
		case AST_EQ: BINARY_ARITH(Opcode::EQ);
		case AST_ADD: BINARY_ARITH(Opcode::ADD);
		case AST_SUB: BINARY_ARITH(Opcode::SUB);
		case AST_MUL: BINARY_ARITH(Opcode::MUL);
		case AST_DIV: BINARY_ARITH(Opcode::DIV);
		case AST_MOD: BINARY_ARITH(Opcode::MOD);
		case AST_NOT: {
			Operand res = make_temporary();
			Operand inverse =
				to_rvalue(chunk, compile(node.branch.children[0], pool, chunk));
			chunk->emit(Opcode::NOT, res, inverse);
			return res;
		}
		case AST_AT: {
			// evaluates to a temporary register containing the address of the lvalue

			Operand base = compile(node.branch.children[0], pool, chunk);
			Operand off =
				to_rvalue(chunk, compile(node.branch.children[1], pool, chunk));

			if (!base.is_register()) err("Base must be an lvalue");
			if (base.type == Operand::Type::TMP) err("Not an array");

			Operand tmp = make_temporary();
			chunk->emit(Opcode::ADD, tmp, base, off)
				.with_comment("accessing allocated array");

			return Operand(tmp.reg.as_addr()).as_temp();
		}
		case AST_ID: {
			Operand* opnd = env.find(node.str_id);
			if (opnd == nullptr) err("Variable not found");
			return *opnd;
		}
		case AST_STR: {
			// allocates a static buffer for the string characters, ending in a
			// setinel null character and returns a pointer to the start

			const char* str = pool.find(node.str_id);
			auto str_len = strlen(str);
			auto buf = make_temporary();
			buf.reg = buf.reg.as_addr();
			dyn_alloc_start -= (Number)str_len + 1;

			chunk->emit(Opcode::MOV, buf, Operand(dyn_alloc_start));

			for (size_t i = 0; i < str_len + 1; i++)
				chunk->emit(
					Opcode::MOV,
					Operand(Register((size_t)dyn_alloc_start + i).as_num()).as_reg(),
					Operand((Number)str[i])
				);

			return buf;
			// return Operand(pool.find(node.str_id));
		}
		case AST_DECL: {
			Node id = node.branch.children[0];

			// fun f args = exp
			if (node.branch.children_count == 3) {
				Node args = node.branch.children[1];
				Node body = node.branch.children[2];

				Operand* opnd = env.insert(id.str_id, make_register());
				*opnd = Operand(bytecode::Funktion {
					args.branch.children_count, args.branch.children, body
				});
				return *opnd;
			}

			// var id = exp
			if (node.branch.children_count == 2) {
				Operand initial = compile(node.branch.children[1], pool, chunk);

				// initial is an array
				if (initial.type == Operand::Type::REG && initial.reg.has_addr())
					return *env.insert(id.str_id, initial);

				// anything else
				initial = to_rvalue(chunk, initial);
				Operand* var = env.insert(id.str_id, make_register());
				if (initial.is_register()) var->reg.type = initial.reg.type;
				chunk->emit(Opcode::MOV, *var, initial)
					.with_comment("creating variable");
				return *var;
			}

			// var id
			if (node.branch.children_count == 1) {
				Operand* var = env.insert(id.str_id, make_register());
				chunk->emit(Opcode::MOV, *var, {})
					.with_comment("creating uninitialized variable");
				return *var;
			}

			assert(false && "unreachable");
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
		case AST_CHAR: return {node.character};
		case AST_PATH: return compile(node.branch.children[0], pool, chunk);
	}
	assert(false);
}
