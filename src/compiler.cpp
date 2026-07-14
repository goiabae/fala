#include "compiler.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <memory>
#include <vector>

#include "ast.hpp"
#include "lir.hpp"
#include "str_pool.h"
#include "type.hpp"
#include "utils.hpp"

namespace compiler {

using lir::Opcode;
using lir::Register;
using std::vector;

static void err(const char* msg);

void err(const char* msg) {
	std::cout << "COMPILER_ERR: " << msg << std::endl;
	exit(1);
}

namespace builtin {
static Result read_int(Compiler& comp, vector<Operand> args);
static Result read_char(Compiler& comp, vector<Operand> args);
static Result write_int(Compiler& comp, vector<Operand> args);
static Result write_char(Compiler& comp, vector<Operand> args);
static Result write_str(Compiler& comp, vector<Operand> args);
static Result make_array(Compiler& comp, vector<Operand> args);

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

Result write_int(Compiler& comp, vector<Operand> args) {
	if (args.size() != 1)
		err("write_int accepts only a single integer as argument");
	Chunk chunk {};
	auto t1 = comp.make_register();
	chunk.emit_loada(t1, Operand::make_immediate_integer(0), args[0]);
	chunk.emit(Opcode::PRINTV, t1);
	return {chunk, {}};
}

Result write_char(Compiler& comp, vector<Operand> args) {
	if (args.size() != 1)
		err("write_int accepts only a single character as argument");
	Chunk chunk {};
	auto op = args[0];
	auto t1 = comp.make_register();
	chunk.emit_loada(t1, Operand::make_immediate_integer(0), op);
	chunk.emit(Opcode::PRINTC, t1);
	return {chunk, {}};
}

Result write_str(Compiler&, vector<Operand> args) {
	if (args.size() != 1)
		err("write_str accepts only a single pointer to character as argument");
	Chunk chunk {};
	auto& op = args[0];
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
	auto addr =
		Operand(Register(comp.reg_count++, lir::Type::make_integer_array()));
	addr.as_register().is_lvalue_pointer = true;

	auto t1 = comp.make_register();
	chunk.emit_loada(t1, Operand::make_immediate_integer(0), args[0]);
	chunk.emit_alloca(addr, t1).with_comment("allocating array");
	return {chunk, addr};
}

} // namespace builtin

Chunk Compiler::compile() {
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

	auto res_ = compile(ast.root_index, handlers, scope_id);
	chunk = chunk + res_.code;

	auto start = Operand::make_immediate_integer(dyn_alloc_start);
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
	return Operand(Register(reg_count++, lir::Type::make_integer()));
}

Operand Compiler::make_label() {
	return lir::Operand(lir::Label(label_count++));
}

Result Compiler::compile_or_allocate_lvalue(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	const auto& node = ast.at(node_idx);
	if (node.type == NodeType::PATH) {
		return compile_lvalue(node_idx, handlers, scope_id);
	} else {
		bool is_array = false;
		{
			auto t1 = tpc.node_to_type.at(node_idx);
			auto t2 = get_datatype(t1);
			if (nullptr != std::dynamic_pointer_cast<Array>(t2)) is_array = true;
		}
		if (is_array) {
			return compile(node_idx, handlers, scope_id);
		} else {
			Chunk chunk {};
			auto res = compile(node_idx, handlers, scope_id);
			chunk = chunk + res.code;
			auto res_opnd = res.opnd;
			auto t1 = make_register();
			chunk.emit_alloca(t1, Operand::make_immediate_integer(1));
			chunk.emit_storea(res_opnd, Operand::make_immediate_integer(0), t1);
			return {chunk, t1};
		}
	}
}

Result Compiler::compile_app(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};

	const auto& node = ast.at(node_idx);
	const auto& func_node = ast.at(node[0]);
	const auto& args_node = ast.at(node[1]);

	vector<Operand> args {};

	for (size_t i = 0; i < args_node.branch.children_count; i++) {
		auto arg_idx = args_node[i];
		auto res = compile_or_allocate_lvalue(arg_idx, handlers, scope_id);
		chunk = chunk + res.code;
		args.push_back(res.opnd);
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
	if (func_opnd->type != Operand::Type::LABEL)
		err("Type of <name> is not function.");

	// push arguments in the reverse order the parameters where declared
	for (size_t i = args_node.branch.children_count; i > 0; i--)
		chunk.emit(Opcode::PUSH, args[i - 1]);

	chunk.emit(Opcode::CALL, *func_opnd);
	Operand res = make_register();
	chunk.emit(Opcode::POP, res);
	chunk.result_opnd = res;

	return {chunk, res};
}

Result Compiler::compile_if(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto cond_idx = node[0];
	auto then_idx = node[1];
	auto else_idx = node[2];

	Operand l1 = make_label();
	Operand l2 = make_label();
	Operand res = make_register();

	auto cond_res = compile(cond_idx, handlers, scope_id);
	chunk = chunk + cond_res.code;
	Operand cond_opnd = cond_res.opnd;

	chunk.emit(Opcode::JMP_FALSE, cond_opnd, l1).with_comment("if branch");

	auto yes_res = compile(then_idx, handlers, scope_id);
	chunk = chunk + yes_res.code;
	Operand yes_opnd = yes_res.opnd;

	chunk.emit(Opcode::MOV, res, yes_opnd);
	chunk.emit(Opcode::JMP, l2);
	chunk.add_label(l1);

	auto no_res = compile(else_idx, handlers, scope_id);
	chunk = chunk + no_res.code;
	Operand no_opnd = no_res.opnd;

	chunk.emit(Opcode::MOV, res, no_opnd);
	chunk.add_label(l2);

	return {chunk, res};
}

Result Compiler::compile_for(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
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
		{inc.as_label(), result_register},
		{end.as_label(), result_register},
		handlers.return_handler,
		true,
		true,
		handlers.has_return_handler
	};

	Operand step = [&]() {
		if (step_node.type != NodeType::EMPTY) {
			auto step_res = compile(step_idx, handlers, scope_id);
			chunk = chunk + step_res.code;
			return step_res.opnd;
		} else {
			return Operand::make_immediate_integer(1);
		}
	}();

	auto new_scope_id = env.create_child_scope(scope_id);

	auto var_res = compile(decl_idx, handlers, new_scope_id);
	chunk = chunk + var_res.code;
	Operand var = var_res.opnd;

	auto to_res = compile(to_idx, handlers, new_scope_id);
	chunk = chunk + to_res.code;
	Operand to = to_res.opnd;

	const auto t1 = make_register();
	const auto t2 = make_register();

	chunk.add_label(beg);
	chunk.emit_loada(t1, Operand::make_immediate_integer(0), var);
	chunk.emit(Opcode::EQ, cmp, t1, to);
	chunk.emit(Opcode::JMP_TRUE, cmp, end);

	auto exp_res = compile(then_idx, new_handlers, new_scope_id);
	chunk = chunk + exp_res.code;
	Operand exp = exp_res.opnd;

	chunk.emit(Opcode::MOV, result_register, exp);

	chunk.add_label(inc);
	chunk.emit(Opcode::ADD, t2, t1, step);
	chunk.emit_storea(t2, Operand::make_immediate_integer(0), var);
	chunk.emit(Opcode::JMP, beg);
	chunk.add_label(end);

	return {chunk, result_register};
}

Result Compiler::compile_when(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto cond_idx = node[0];
	auto then_idx = node[1];

	Operand l1 = make_label();
	Operand res = make_register();

	auto cond_res = compile(cond_idx, handlers, scope_id);
	chunk = chunk + cond_res.code;
	Operand cond_opnd = cond_res.opnd;

	chunk.emit(Opcode::MOV, res, {}).with_comment("when conditional");
	chunk.emit(Opcode::JMP_FALSE, cond_opnd, l1);

	auto yes_res = compile(then_idx, handlers, scope_id);
	chunk = chunk + yes_res.code;
	Operand yes_opnd = yes_res.opnd;

	chunk.emit(Opcode::MOV, res, yes_opnd);
	chunk.add_label(l1);

	return {chunk, res};
}

Result Compiler::compile_while(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	Operand beg = make_label();
	Operand end = make_label();

	Operand result_register = make_register();

	SignalHandlers new_handlers {
		{beg.as_label(), result_register},
		{end.as_label(), result_register},
		handlers.return_handler,
		true,
		true,
		handlers.has_return_handler
	};

	chunk.add_label(beg);

	auto cond_res = compile(node[0], handlers, scope_id);
	chunk = chunk + cond_res.code;
	Operand cond = cond_res.opnd;

	chunk.emit(Opcode::JMP_FALSE, cond, end);

	auto exp_res = compile(node[1], new_handlers, scope_id);
	chunk = chunk + exp_res.code;
	Operand exp = exp_res.opnd;

	chunk.emit(Opcode::MOV, result_register, exp);

	chunk.emit(Opcode::JMP, beg);
	chunk.add_label(end);

	return {chunk, result_register};
}

Result Compiler::compile_lvalue(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::EMPTY: assert(false);
		case NodeType::APP: assert(false);
		case NodeType::NUM: assert(false);
		case NodeType::BLK: assert(false);
		case NodeType::IF: assert(false);
		case NodeType::WHEN: assert(false);
		case NodeType::FOR: assert(false);
		case NodeType::WHILE: assert(false);
		case NodeType::BREAK: assert(false);
		case NodeType::CONTINUE: assert(false);
		case NodeType::ASS: assert(false);
		case NodeType::OR: assert(false);
		case NodeType::AND: assert(false);
		case NodeType::GTN: assert(false);
		case NodeType::LTN: assert(false);
		case NodeType::GTE: assert(false);
		case NodeType::LTE: assert(false);
		case NodeType::EQ: assert(false);
		case NodeType::AT: {
			Chunk chunk {};
			const auto& node = ast.at(node_idx);
			auto base_res = compile_lvalue(node[0], handlers, scope_id);
			chunk = chunk + base_res.code;
			auto base = base_res.opnd;
			auto off_res = compile(node[1], handlers, scope_id);
			chunk = chunk + off_res.code;
			auto off = off_res.opnd;

			auto tmp = make_register();
			auto t1 = make_register();
			chunk.emit_shifta(tmp, off, base)
				.with_comment("accessing allocated array");
			return {chunk, tmp};
		}
		case NodeType::ADD: assert(false);
		case NodeType::SUB: assert(false);
		case NodeType::MUL: assert(false);
		case NodeType::DIV: assert(false);
		case NodeType::MOD: assert(false);
		case NodeType::NOT: assert(false);
		case NodeType::ID: {
			Chunk chunk {};
			Operand* opnd = env.find(scope_id, node.str_id);
			if (opnd == nullptr) {
				fprintf(stderr, "%s ", pool.find(node.str_id));
				err("Variable not found");
			}
			chunk.result_opnd = *opnd;
			return {chunk, *opnd};
		}
		case NodeType::STR: assert(false);
		case NodeType::VAR_DECL: assert(false);
		case NodeType::FUN_DECL: assert(false);
		case NodeType::NIL: assert(false);
		case NodeType::TRUE: assert(false);
		case NodeType::FALSE: assert(false);
		case NodeType::LET: assert(false);
		case NodeType::CHAR: assert(false);
		case NodeType::PATH: {
			return compile_lvalue(node[0], handlers, scope_id);
		}
		case NodeType::AS: assert(false);
		case NodeType::INSTANCE: assert(false);
	}
	assert(false);
}

Result Compiler::compile_var_decl(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto id_idx = node[0];
	auto opt_type_idx = node[1];
	auto exp_idx = node[2];

	(void)opt_type_idx;

	const auto& id_node = ast.at(id_idx);
	const auto& exp_node = ast.at(exp_idx);

	const auto comment =
		std::format("creating variable \"{}\"", pool.find(id_node.str_id));

	bool is_array = false;
	{
		auto t1 = tpc.node_to_type.at(exp_idx);
		auto t2 = get_datatype(t1);
		if (nullptr != std::dynamic_pointer_cast<Array>(t2)) is_array = true;
	}

	if (exp_node.type == NodeType::PATH) {
		auto initial_res = compile_lvalue(exp_idx, handlers, scope_id);
		chunk = chunk + initial_res.code;
		auto initial = initial_res.opnd;
		// FIXME: properly set register type
		auto* var = env.insert(scope_id, id_node.str_id, make_register());
		chunk.emit_clonea(*var, initial).with_comment(comment);
		return {chunk, *var};
	} else {
		auto initial_res = compile(exp_idx, handlers, scope_id);
		chunk = chunk + initial_res.code;
		auto initial = initial_res.opnd;
		// FIXME: properly set register type
		auto* var = env.insert(scope_id, id_node.str_id, make_register());
		if (is_array) {
			chunk.emit_mov(*var, initial);
		} else {
			chunk.emit_alloca(*var, Operand::make_immediate_integer(1))
				.with_comment(comment);
			chunk.emit_storea(initial, Operand::make_immediate_integer(0), *var);
		}
		return {chunk, *var};
	}
}

Result Compiler::compile_fun_decl(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

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

	for (auto param_idx : params_node) {
		const auto& param_node = ast.at(param_idx);
		auto arg = make_register();
		func.emit(Opcode::POP, arg);
		env.insert(new_scope_id, param_node.str_id, arg);
	}

	auto op_res = compile(body_idx, handlers, new_scope_id);
	func = func + op_res.code;
	auto op = op_res.opnd;

	func.emit(Opcode::PUSH, op);
	func.emit(Opcode::RET);

	functions.push_back(func);

	return {chunk, func_name};
}

Result Compiler::compile_ass(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto cell_res = compile_lvalue(node[0], handlers, scope_id);
	chunk = chunk + cell_res.code;
	Operand cell = cell_res.opnd;

	auto exp_res = compile(node[1], handlers, scope_id);
	chunk = chunk + exp_res.code;
	Operand exp = exp_res.opnd;

	chunk.emit_storea(exp, Operand::make_immediate_integer(0), cell)
		.with_comment("assigning to array variable");

	return {chunk, exp};
}

Result Compiler::compile_let(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto decls_idx = node[0];
	auto exp_idx = node[1];

	const auto& decls_node = ast.at(decls_idx);

	{
		auto new_scope_id = env.create_child_scope(scope_id);
		for (size_t i = 0; i < decls_node.branch.children_count; i++) {
			auto res = compile(decls_node[i], handlers, new_scope_id);
			chunk = chunk + res.code;
		}
		auto res_res = compile(exp_idx, handlers, new_scope_id);
		chunk = chunk + res_res.code;
		Operand res = res_res.opnd;
		return {chunk, res};
	}
}

Result Compiler::compile_str(
	NodeIndex node_idx, SignalHandlers, Env<Operand>::ScopeID
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	// allocates a static buffer for the string characters, ending in a
	// sentinel null character and returns a pointer to the start

	const char* str = pool.find(node.str_id);
	auto str_len = strlen(str);
	dyn_alloc_start -= (Number)str_len + 1;

	auto t1 = make_register();
	chunk.emit_alloca(t1, Operand::make_immediate_integer((int)str_len));
	for (size_t i = 0; i < str_len; i++) {
		chunk.emit_storea(
			Operand::make_immediate_integer(str[i]),
			Operand::make_immediate_integer((int)i),
			t1
		);
	}

	chunk.result_opnd = t1;
	return {chunk, t1};
}

Result Compiler::compile_at(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	Chunk chunk {};
	const auto& node = ast.at(node_idx);

	// evaluates to a temporary register containing the address of the lvalue

	auto base_res = compile(node[0], handlers, scope_id);
	chunk = chunk + base_res.code;
	Operand base = base_res.opnd;
	auto off_res = compile(node[1], handlers, scope_id);
	chunk = chunk + off_res.code;
	Operand off = off_res.opnd;

	auto is_array = false;
	const auto t1 = tpc.node_to_type.at(node[0]);
	const auto t3 = get_datatype(t1);
	if (auto t2 = std::dynamic_pointer_cast<Array>(t3)) {
		is_array = true;
	}
	if (not is_array) err("Base must be an lvalue");

	Operand tmp = make_register();
	tmp.as_register().is_lvalue_pointer = true;
	chunk.emit(Opcode::ADD, tmp, base, off)
		.with_comment("accessing allocated array");

	return {chunk, Operand(tmp)};
}

// FIXME: Temporary workaround
#define COMPILE_WITH_HANDLER(METH) return METH(node_idx, handlers, scope_id);

Result Compiler::compile(
	NodeIndex node_idx, SignalHandlers handlers, Env<Operand>::ScopeID scope_id
) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::APP: COMPILE_WITH_HANDLER(compile_app)
		case NodeType::NUM: {
			Chunk chunk {};
			auto opnd = Operand::make_immediate_integer(node.num);
			chunk.result_opnd = opnd;
			return {chunk, opnd};
		}
		case NodeType::BLK: {
			Chunk chunk {};
			auto new_scope_id = env.create_child_scope(scope_id);
			Operand opnd;
			for (size_t i = 0; i < node.branch.children_count; i++) {
				auto opnd_res = compile(node[i], handlers, new_scope_id);
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

			auto res_res = compile(node[0], handlers, scope_id);
			chunk = chunk + res_res.code;
			Operand res = res_res.opnd;
			chunk.emit(Opcode::MOV, handlers.break_handler.result_register, res);
			chunk
				.emit(
					Opcode::JMP, lir::Operand(handlers.break_handler.destination_label)
				)
				.with_comment("break out of loop");
			return {chunk, {}};
		}
		case NodeType::CONTINUE: {
			Chunk chunk {};
			if (!handlers.has_continue_handler)
				err("can't continue outside of loops");
			if (!(node.branch.children_count == 1))
				err("continue requires a expression to evaluate the loop to");

			auto res_res = compile(node[0], handlers, scope_id);
			chunk = chunk + res_res.code;
			Operand res = res_res.opnd;
			chunk.emit(Opcode::MOV, handlers.continue_handler.result_register, res);
			chunk
				.emit(
					Opcode::JMP, lir::Operand(handlers.continue_handler.destination_label)
				)
				.with_comment("continue to next iteration of loop");
			return {chunk, {}};
		}
		case NodeType::ASS: COMPILE_WITH_HANDLER(compile_ass)

#define BINARY_ARITH(OPCODE)                                  \
	{                                                           \
		Chunk chunk {};                                           \
		auto left_node = node[0];                                 \
		auto right_node = node[1];                                \
                                                              \
		auto left_res = compile(left_node, handlers, scope_id);   \
		chunk = chunk + left_res.code;                            \
		auto right_res = compile(right_node, handlers, scope_id); \
		chunk = chunk + right_res.code;                           \
		Operand left = left_res.opnd;                             \
		Operand right = right_res.opnd;                           \
		Operand res = make_register();                            \
                                                              \
		chunk.emit(OPCODE, res, left, right);                     \
		chunk.result_opnd = res;                                  \
		return {chunk, res};                                      \
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
			auto inverse_res = compile(node[0], handlers, scope_id);
			chunk = chunk + inverse_res.code;
			Operand inverse = inverse_res.opnd;
			chunk.emit(Opcode::NOT, res, inverse);
			return {chunk, res};
		}
		case NodeType::AT: {
			Chunk chunk {};
			auto res = compile_lvalue(node_idx, handlers, scope_id);
			chunk = chunk + res.code;
			auto res_opnd = res.opnd;
			auto tmp = make_register();
			chunk.emit_loada(tmp, Operand::make_immediate_integer(0), res_opnd);
			return {chunk, tmp};
		}
		case NodeType::ID: {
			Chunk chunk {};
			auto res = compile_lvalue(node_idx, handlers, scope_id);
			chunk = chunk + res.code;
			auto res_opnd = res.opnd;
			auto tmp = make_register();
			chunk.emit_loada(tmp, Operand::make_immediate_integer(0), res_opnd);
			return {chunk, tmp};
		}
		case NodeType::STR: COMPILE_WITH_HANDLER(compile_str)
		case NodeType::VAR_DECL: COMPILE_WITH_HANDLER(compile_var_decl)
		case NodeType::FUN_DECL: COMPILE_WITH_HANDLER(compile_fun_decl)
		case NodeType::NIL: return {{}, Operand::make_immediate_integer(0)};
		case NodeType::TRUE: return {{}, lir::Operand::make_immediate_integer(1)};
		case NodeType::FALSE: return {{}, lir::Operand::make_immediate_integer(0)};
		case NodeType::LET: COMPILE_WITH_HANDLER(compile_let)
		case NodeType::EMPTY: assert(false && "unreachable");
		case NodeType::CHAR: {
			Chunk chunk {};
			auto opnd = lir::Operand::make_immediate_integer(node.character);
			chunk.result_opnd = opnd;
			return {chunk, opnd};
		}
		case NodeType::PATH: {
			Chunk chunk {};
			auto res = compile_lvalue(node_idx, handlers, scope_id);
			chunk = chunk + res.code;
			auto res_opnd = res.opnd;
			auto tmp = make_register();
			chunk.emit_loada(tmp, Operand::make_immediate_integer(0), res_opnd);
			chunk.result_opnd = tmp;
			return {chunk, tmp};
		}
		case NodeType::INSTANCE:
			assert(false && "used only in typechecking. should not be evaluated");
		case NodeType::AS: return compile(node[0], handlers, scope_id);
	}
	assert(false);
}

} // namespace compiler
