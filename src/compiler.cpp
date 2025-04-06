#include "compiler.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <vector>

#include "ast.hpp"
#include "lir.hpp"
#include "str_pool.h"

namespace compiler {

using lir::Opcode;
using lir::Register;
using std::vector;

void err(const char* msg) {
	std::cout << "COMPILER_ERR: " << msg << std::endl;
	exit(1);
}

namespace builtin {
Result read_int(Compiler& comp, vector<Operand> args);
Result read_char(Compiler& comp, vector<Operand> args);
Result write_int(Compiler& comp, vector<Operand> args);
Result write_char(Compiler& comp, vector<Operand> args);
Result write_str(Compiler& comp, vector<Operand> args);
Result make_array(Compiler& comp, vector<Operand> args);

struct Builtin {
	Result (*ptr)(Compiler& comp, vector<Operand> args);
	const char* name;
};

constexpr auto builtins = {
	Builtin {read_int, "read_int"},
	Builtin {read_char, "read_char"},
	Builtin {write_int, "write_int"},
	Builtin {write_char, "write_char"},
	Builtin {write_str, "write_str"},
	Builtin {make_array, "make_array"}
};

Result write_int(Compiler&, vector<Operand> args) {
	if (args.size() != 1)
		err(
			"write_int accepts only a single pointer to character or integer as an "
			"argument"
		);

	Chunk chunk {};

	auto& op = args[0];
	assert(!(op.type == Operand::Type::REG && op.reg.has_addr()));
	chunk.emit(Opcode::PRINTV, op);
	return {chunk, {}};
}

Result write_char(Compiler&, vector<Operand> args) {
	if (args.size() != 1)
		err(
			"write_int accepts only a single pointer to character or integer as an "
			"argument"
		);

	Chunk chunk {};
	auto& op = args[0];
	assert(!(op.type == Operand::Type::REG && op.reg.has_addr()));
	chunk.emit(Opcode::PRINTC, op);
	return {chunk, {}};
}

Result write_str(Compiler&, vector<Operand> args) {
	if (args.size() != 1)
		err(
			"write_str accepts only a single pointer to character or integer as an "
			"argument"
		);

	Chunk chunk {};
	auto& op = args[0];
	assert(op.type == Operand::Type::REG && op.reg.has_addr());
	chunk.emit(Opcode::PRINTF, op);
	return {chunk, {}};
}

Result read_int(Compiler& comp, vector<Operand>) {
	Chunk chunk {};
	Operand tmp = comp.make_register();
	chunk.emit(Opcode::READV, tmp);
	return {chunk, tmp};
}

Result read_char(Compiler& comp, vector<Operand>) {
	Chunk chunk {};
	Operand tmp = comp.make_register();
	chunk.emit(Opcode::READC, tmp);
	return {chunk, tmp};
}

Result make_array(Compiler& comp, vector<Operand> args) {
	if (args.size() != 1)
		err("The `array' builtin expects a size as the first and only argument.");

	Chunk chunk {};
	// if size if constant we subtract the allocation start at compile time
	if (args[0].type == Operand::Type::NUM) {
		auto addr = Operand(lir::Array {Register(comp.reg_count++).as_addr()});
		auto dyn = Operand(comp.dyn_alloc_start -= args[0].num);

		chunk.emit(Opcode::MOV, addr, dyn).with_comment("static array");
		return {chunk, addr};

	} else {
		auto addr = Operand(lir::Array {Register(comp.reg_count++).as_addr()});
		auto dyn = Operand(Register(0).as_num());

		chunk.emit(Opcode::SUB, dyn, dyn, args[0]);
		chunk.emit(Opcode::MOV, addr, dyn).with_comment("allocating array");
		return {chunk, addr};
	}
}

} // namespace builtin

Chunk Compiler::compile(AST& ast, const StringPool& pool) {
	Chunk preamble {};
	Chunk chunk;

	auto main = make_label();

	auto dyn = make_register();
	preamble
		.emit(Opcode::MOV, dyn, {}) // snd will be patched after compilation
		.with_comment("contains address to start of the last allocated region");

	chunk.add_label(main);

	SignalHandlers handlers {};
	auto scope_id = env.root_scope_id;

	auto res_ = compile(ast, ast.root_index, pool, handlers, scope_id);
	chunk = chunk + res_.code;

	Operand start(dyn_alloc_start);
	preamble.m_vec[0].operands[1] =
		start; // backpatching snd after static allocations

	preamble.emit(Opcode::JMP, main);

	Chunk all_functions {};
	for (const auto& f : functions) {
		all_functions = all_functions + f;
	}

	Chunk res = preamble + all_functions + chunk;

	return res;
}

Operand Compiler::make_register() {
	return Operand(Register(reg_count++).as_num());
}

Operand Compiler::make_label() { return {lir::Label {label_count++}}; }

Operand Compiler::to_rvalue(Chunk* chunk, Operand opnd) {
	if (opnd.type == Operand::Type::REG && opnd.reg.has_addr()) {
		Operand tmp = make_register();
		chunk->emit(Opcode::LOAD, tmp, Operand(0), opnd)
			.with_comment("casting to rvalue");
		return tmp;
	} else {
		return opnd;
	}
}

Result Compiler::compile_app(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};

	const auto& node = ast.at(node_idx);
	const auto& func_node = ast.at(node[0]);
	const auto& args_node = ast.at(node[1]);

	vector<Operand> args {};

	for (size_t i = 0; i < args_node.branch.children_count; i++) {
		auto arg_idx = args_node[i];
		const auto& arg_node = ast.at(arg_idx);

		auto res = compile(ast, arg_idx, pool, handlers, scope_id);
		chunk = chunk + res.code;
		args.push_back(res.opnd);

		if (arg_node.type == NodeType::PATH) args[i] = to_rvalue(&chunk, args[i]);
	}

	const char* func_name = pool.find(func_node.str_id);

	// try to match function name with any builtin. otherwise, call it as a
	// user-defined function
	for (const auto& builtin : builtin::builtins)
		if (strcmp(func_name, builtin.name) == 0) {
			auto result = builtin.ptr(*this, args);
			return {chunk + result.code, result.opnd};
		};

	Operand* func_opnd = env.find(scope_id, func_node.str_id);
	if (!func_opnd) err("Function not found");
	if (func_opnd->type != Operand::Type::LAB)
		err("Type of <name> is not function.");

	// push arguments in the reverse order the parameters where declared
	for (size_t i = args_node.branch.children_count; i > 0; i--)
		chunk.emit(Opcode::PUSH, args[i - 1]);

	chunk.emit(Opcode::CALL, *func_opnd);
	Operand res = make_register();
	chunk.emit(Opcode::POP, res);

	return {chunk, res};
}

Result Compiler::compile_if(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto cond_idx = node[0];
	auto then_idx = node[1];
	auto else_idx = node[2];

	Operand l1 = make_label();
	Operand l2 = make_label();
	Operand res = make_register();

	auto cond_res = compile(ast, cond_idx, pool, handlers, scope_id);
	chunk = chunk + cond_res.code;
	Operand cond_opnd = to_rvalue(&chunk, cond_res.opnd);

	chunk.emit(Opcode::JMP_FALSE, cond_opnd, l1).with_comment("if branch");

	auto yes_res = compile(ast, then_idx, pool, handlers, scope_id);
	chunk = chunk + yes_res.code;
	Operand yes_opnd = yes_res.opnd;

	chunk.emit(Opcode::MOV, res, yes_opnd);
	chunk.emit(Opcode::JMP, l2);
	chunk.add_label(l1);

	auto no_res = compile(ast, else_idx, pool, handlers, scope_id);
	chunk = chunk + no_res.code;
	Operand no_opnd = no_res.opnd;

	chunk.emit(Opcode::MOV, res, no_opnd);
	chunk.add_label(l2);

	return {chunk, res};
}

Result Compiler::compile_for(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto decl_idx = node[0];
	auto to_idx = node[1];
	auto step_idx = node[2];
	auto then_idx = node[3];

	const auto& step_node = ast.at(step_idx);

	Operand beg = make_label();
	Operand inc = make_label();
	Operand end = make_label();
	Operand cmp = make_register();

	Operand result_register = make_register();

	SignalHandlers new_handlers {
		{inc.lab, result_register},
		{end.lab, result_register},
		handlers.return_handler,
		true,
		true,
		handlers.has_return_handler
	};

	Operand step = [&]() {
		if (step_node.type != NodeType::EMPTY) {
			auto step_res = compile(ast, step_idx, pool, handlers, scope_id);
			chunk = chunk + step_res.code;
			return to_rvalue(&chunk, step_res.opnd);
		} else {
			return Operand(1);
		}
	}();

	auto new_scope_id = env.create_child_scope(scope_id);

	auto var_res = compile(ast, decl_idx, pool, handlers, new_scope_id);
	chunk = chunk + var_res.code;
	Operand var = var_res.opnd;
	if (var.type != Operand::Type::REG)
		err("Declaration must be of a number lvalue");

	auto to_res = compile(ast, to_idx, pool, handlers, new_scope_id);
	chunk = chunk + to_res.code;
	Operand to = to_rvalue(&chunk, to_res.opnd);

	chunk.add_label(beg);
	chunk.emit(Opcode::EQ, cmp, var, to);
	chunk.emit(Opcode::JMP_TRUE, cmp, end);

	auto exp_res = compile(ast, then_idx, pool, new_handlers, new_scope_id);
	chunk = chunk + exp_res.code;
	Operand exp = exp_res.opnd;

	chunk.emit(Opcode::MOV, result_register, exp);

	chunk.add_label(inc);
	chunk.emit(Opcode::ADD, var, var, step);
	chunk.emit(Opcode::JMP, beg);
	chunk.add_label(end);

	return {chunk, result_register};
}

Result Compiler::compile_when(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto cond_idx = node[0];
	auto then_idx = node[1];

	Operand l1 = make_label();
	Operand res = make_register();

	auto cond_res = compile(ast, cond_idx, pool, handlers, scope_id);
	chunk = chunk + cond_res.code;
	Operand cond_opnd = to_rvalue(&chunk, cond_res.opnd);

	chunk.emit(Opcode::MOV, res, {}).with_comment("when conditional");
	chunk.emit(Opcode::JMP_FALSE, cond_opnd, l1);

	auto yes_res = compile(ast, then_idx, pool, handlers, scope_id);
	chunk = chunk + yes_res.code;
	Operand yes_opnd = yes_res.opnd;

	chunk.emit(Opcode::MOV, res, yes_opnd);
	chunk.add_label(l1);

	return {chunk, res};
}

Result Compiler::compile_while(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	Operand beg = make_label();
	Operand end = make_label();

	Operand result_register = make_register();

	SignalHandlers new_handlers {
		{beg.lab, result_register},
		{end.lab, result_register},
		handlers.return_handler,
		true,
		true,
		handlers.has_return_handler
	};

	chunk.add_label(beg);

	auto cond_res = compile(ast, node[0], pool, handlers, scope_id);
	chunk = chunk + cond_res.code;
	Operand cond = to_rvalue(&chunk, cond_res.opnd);

	chunk.emit(Opcode::JMP_FALSE, cond, end);

	auto exp_res = compile(ast, node[1], pool, new_handlers, scope_id);
	chunk = chunk + exp_res.code;
	Operand exp = to_rvalue(&chunk, exp_res.opnd);

	chunk.emit(Opcode::MOV, result_register, exp);

	chunk.emit(Opcode::JMP, beg);
	chunk.add_label(end);

	return {chunk, result_register};
}

#if 0
Result Compiler::compile_app(
	AST& ast, NodeIndex node_idx, const StringPool& pool
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	return {chunk, exp};
}
#endif

Result Compiler::compile_decl(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	// "fun" id params opt-type "=" body
	if (node.branch.children_count == 4) {
		auto id_idx = node[0];
		auto params_idx = node[1];
		auto opt_type_idx = node[2];
		auto body_idx = node[3];

		const auto& id_node = ast.at(id_idx);
		const auto& params_node = ast.at(params_idx);

		(void)opt_type_idx;

		Operand* opnd = env.insert(scope_id, id_node.str_id, make_register());

		auto func_name = make_label();
		*opnd = func_name;
		auto new_scope_id = env.create_child_scope(scope_id);

		Chunk func {};

		func.add_label(func_name);
		func.emit(Opcode::FUNC);

		for (size_t i = 0; i < params_node.branch.children_count; i++) {
			const auto& param_node = ast.at(params_node[i]);
			auto arg = make_register();
			func.emit(Opcode::POP, arg);
			env.insert(new_scope_id, param_node.str_id, arg);
		}

		auto op_res = compile(ast, body_idx, pool, handlers, new_scope_id);
		func = func + op_res.code;
		auto op = op_res.opnd;

		func.emit(Opcode::PUSH, op);
		func.emit(Opcode::RET);

		functions.push_back(func);

		return {chunk, func_name};
	}

	// "var" id opt-type "=" exp
	if (node.branch.children_count == 3) {
		auto id_idx = node[0];
		auto opt_type_idx = node[1];
		auto exp_idx = node[2];

		(void)opt_type_idx;

		const auto& id_node = ast.at(id_idx);

		auto initial_res = compile(ast, exp_idx, pool, handlers, scope_id);
		chunk = chunk + initial_res.code;
		Operand initial = initial_res.opnd;

		// initial is an array
		if (initial.type == Operand::Type::ARR)
			return {chunk, *env.insert(scope_id, id_node.str_id, initial)};

		// anything else
		initial = to_rvalue(&chunk, initial);
		Operand* var = env.insert(scope_id, id_node.str_id, make_register());
		if (initial.type == Operand::Type::REG) var->reg.type = initial.reg.type;
		chunk.emit(Opcode::MOV, *var, initial).with_comment("creating variable");
		return {chunk, *var};
	}

	assert(false && "unreachable");
}

Result Compiler::compile_ass(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto cell_res = compile(ast, node[0], pool, handlers, scope_id);
	chunk = chunk + cell_res.code;
	Operand cell = cell_res.opnd;
	if (cell.type != Operand::Type::REG)
		err("Left-hand side of assignment must be an lvalue");

	auto exp_res = compile(ast, node[1], pool, handlers, scope_id);
	chunk = chunk + exp_res.code;
	Operand exp = to_rvalue(&chunk, exp_res.opnd);

	if (cell.reg.has_addr())
		chunk.emit(Opcode::STORE, exp, Operand(0), cell)
			.with_comment("assigning to array variable");
	else // contains number
		chunk.emit(Opcode::MOV, cell, exp).with_comment("assigning to variable");

	return {chunk, exp};
}

Result Compiler::compile_let(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto decls_idx = node[0];
	auto exp_idx = node[1];

	const auto& decls_node = ast.at(decls_idx);

	{
		auto new_scope_id = env.create_child_scope(scope_id);
		for (size_t i = 0; i < decls_node.branch.children_count; i++) {
			auto res = compile(ast, decls_node[i], pool, handlers, new_scope_id);
			chunk = chunk + res.code;
		}
		auto res_res = compile(ast, exp_idx, pool, handlers, new_scope_id);
		chunk = chunk + res_res.code;
		Operand res = res_res.opnd;
		return {chunk, res};
	}
}

Result Compiler::compile_str(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers,
	Env<Operand>::ScopeID
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	// allocates a static buffer for the string characters, ending in a
	// sentinel null character and returns a pointer to the start

	const char* str = pool.find(node.str_id);
	auto str_len = strlen(str);
	auto buf = make_register();
	buf.reg = buf.reg.as_addr();
	dyn_alloc_start -= (Number)str_len + 1;

	chunk.emit(Opcode::MOV, buf, Operand(dyn_alloc_start));

	for (size_t i = 0; i < str_len + 1; i++)
		chunk.emit(
			Opcode::MOV,
			Operand(Register((size_t)dyn_alloc_start + i).as_num()),
			Operand((Number)str[i])
		);

	return {chunk, buf};
	// return Operand(pool.find(node.str_id));
}

Result Compiler::compile_at(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	// evaluates to a temporary register containing the address of the lvalue

	auto base_res = compile(ast, node[0], pool, handlers, scope_id);
	chunk = chunk + base_res.code;
	Operand base = base_res.opnd;
	auto off_res = compile(ast, node[1], pool, handlers, scope_id);
	chunk = chunk + off_res.code;
	Operand off = to_rvalue(&chunk, off_res.opnd);

	if (base.type != Operand::Type::ARR) err("Base must be an lvalue");

	Operand tmp = make_register();
	chunk.emit(Opcode::ADD, tmp, base, off)
		.with_comment("accessing allocated array");

	return {chunk, Operand(tmp.reg.as_addr())};
}

// FIXME: Temporary workaround
#define COMPILE_WITH_HANDLER(METH) \
	return METH(ast, node_idx, pool, handlers, scope_id);

Result Compiler::compile(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<Operand>::ScopeID scope_id
) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::APP: COMPILE_WITH_HANDLER(compile_app)
		case NodeType::NUM: return {{}, Operand(node.num)};
		case NodeType::BLK: {
			Chunk chunk {};
			auto new_scope_id = env.create_child_scope(scope_id);
			Operand opnd;
			for (size_t i = 0; i < node.branch.children_count; i++) {
				auto opnd_res = compile(ast, node[i], pool, handlers, new_scope_id);
				chunk = chunk + opnd_res.code;
				opnd = opnd_res.opnd;
			}

			return {chunk, opnd};
		}
		case NodeType::IF: COMPILE_WITH_HANDLER(compile_if)
		case NodeType::WHEN: COMPILE_WITH_HANDLER(compile_when)
		case NodeType::FOR: COMPILE_WITH_HANDLER(compile_for)
		case NodeType::WHILE: COMPILE_WITH_HANDLER(compile_while)
		case NodeType::BREAK: {
			Chunk chunk {};
			if (!handlers.has_break_handler) err("Can't break outside of loops");
			if (!(node.branch.children_count == 1))
				err("`break' requires a expression to evaluate the loop to");

			auto res_res = compile(ast, node[0], pool, handlers, scope_id);
			chunk = chunk + res_res.code;
			Operand res = to_rvalue(&chunk, res_res.opnd);
			chunk.emit(Opcode::MOV, handlers.break_handler.result_register, res);
			chunk.emit(Opcode::JMP, handlers.break_handler.destination_label)
				.with_comment("break out of loop");
			return {chunk, {}};
		}
		case NodeType::CONTINUE: {
			Chunk chunk {};
			if (!handlers.has_continue_handler)
				err("can't continue outside of loops");
			if (!(node.branch.children_count == 1))
				err("continue requires a expression to evaluate the loop to");

			auto res_res = compile(ast, node[0], pool, handlers, scope_id);
			chunk = chunk + res_res.code;
			Operand res = to_rvalue(&chunk, res_res.opnd);
			chunk.emit(Opcode::MOV, handlers.continue_handler.result_register, res);
			chunk.emit(Opcode::JMP, handlers.continue_handler.destination_label)
				.with_comment("continue to next iteration of loop");
			return {chunk, {}};
		}
		case NodeType::ASS: COMPILE_WITH_HANDLER(compile_ass)

#define BINARY_ARITH(OPCODE)                                             \
	{                                                                      \
		Chunk chunk {};                                                      \
		auto left_node = node[0];                                            \
		auto right_node = node[1];                                           \
                                                                         \
		auto left_res = compile(ast, left_node, pool, handlers, scope_id);   \
		chunk = chunk + left_res.code;                                       \
		auto right_res = compile(ast, right_node, pool, handlers, scope_id); \
		chunk = chunk + right_res.code;                                      \
		Operand left = to_rvalue(&chunk, left_res.opnd);                     \
		Operand right = to_rvalue(&chunk, right_res.opnd);                   \
		Operand res = make_register();                                       \
                                                                         \
		chunk.emit(OPCODE, res, left, right);                                \
		return {chunk, res};                                                 \
	}

		case NodeType::OR: BINARY_ARITH(Opcode::OR);
		case NodeType::AND: BINARY_ARITH(Opcode::AND);
		case NodeType::GTN: BINARY_ARITH(Opcode::GREATER);
		case NodeType::LTN: BINARY_ARITH(Opcode::LESS);
		case NodeType::GTE: BINARY_ARITH(Opcode::GREATER_EQ);
		case NodeType::LTE: BINARY_ARITH(Opcode::LESS_EQ);
		case NodeType::EQ: BINARY_ARITH(Opcode::EQ);
		case NodeType::ADD: BINARY_ARITH(Opcode::ADD);
		case NodeType::SUB: BINARY_ARITH(Opcode::SUB);
		case NodeType::MUL: BINARY_ARITH(Opcode::MUL);
		case NodeType::DIV: BINARY_ARITH(Opcode::DIV);
		case NodeType::MOD: BINARY_ARITH(Opcode::MOD);
		case NodeType::NOT: {
			Chunk chunk {};
			Operand res = make_register();
			auto inverse_res = compile(ast, node[0], pool, handlers, scope_id);
			chunk = chunk + inverse_res.code;
			Operand inverse = to_rvalue(&chunk, inverse_res.opnd);
			chunk.emit(Opcode::NOT, res, inverse);
			return {chunk, res};
		}
		case NodeType::AT: COMPILE_WITH_HANDLER(compile_at)
		case NodeType::ID: {
			Operand* opnd = env.find(scope_id, node.str_id);
			if (opnd == nullptr) {
				fprintf(stderr, "%s ", pool.find(node.str_id));
				err("Variable not found");
			}
			return {{}, *opnd};
		}
		case NodeType::STR: COMPILE_WITH_HANDLER(compile_str)
		case NodeType::DECL: COMPILE_WITH_HANDLER(compile_decl)
		case NodeType::NIL: return {{}, {}};
		case NodeType::TRUE: return {{}, {1}};
		case NodeType::FALSE: return {{}, {0}};
		case NodeType::LET: COMPILE_WITH_HANDLER(compile_let)
		case NodeType::EMPTY: assert(false && "unreachable");
		case NodeType::CHAR: return {{}, {node.character}};
		case NodeType::PATH: return compile(ast, node[0], pool, handlers, scope_id);
		case NodeType::PRIMITIVE_TYPE: assert(false && "TODO");
		case NodeType::AS: return compile(ast, node[0], pool, handlers, scope_id);
	}
	assert(false);
}

} // namespace compiler
