#include "lower/ast_to_lir.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <ranges>
#include <string_view>
#include <vector>

#include "ast.hpp"
#include "lir/lir.hpp"
#include "str_pool.h"
#include "symbol_table.hpp"
#include "type.hpp"

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
static Result read_int(Compiler& comp, vector<lir::Operand> args);
static Result read_char(Compiler& comp, vector<lir::Operand> args);
static Result write_int(Compiler& comp, vector<lir::Operand> args);
static Result write_char(Compiler& comp, vector<lir::Operand> args);
static Result write_str(Compiler& comp, vector<lir::Operand> args);
static Result make_array(Compiler& comp, vector<lir::Operand> args);

struct Builtin {
	Result (*ptr)(Compiler& comp, vector<lir::Operand> args);
	std::string_view name;
};

constexpr auto builtins = {
	Builtin {read_int, "read_int"},
	Builtin {read_char, "read_char"},
	Builtin {write_int, "write_int"},
	Builtin {write_char, "write_char"},
	Builtin {write_str, "write_str"},
	Builtin {make_array, "make_array"}
};

Result write_int(Compiler& comp, vector<lir::Operand> args) {
	if (args.size() != 1)
		err("write_int accepts only a single integer as argument");
	lir::Chunk chunk {};
	auto t1 = comp.make_register();
	chunk.emit_loada(t1, lir::Immediate(0), args[0]);
	chunk.emit(Opcode::PRINTV, t1);
	return {chunk, {}};
}

Result write_char(Compiler& comp, vector<lir::Operand> args) {
	if (args.size() != 1)
		err("write_int accepts only a single character as argument");
	lir::Chunk chunk {};
	auto op = args[0];
	auto t1 = comp.make_register();
	chunk.emit_loada(t1, lir::Immediate(0), op);
	chunk.emit(Opcode::PRINTC, t1);
	return {chunk, {}};
}

Result write_str(Compiler&, vector<lir::Operand> args) {
	if (args.size() != 1)
		err("write_str accepts only a single pointer to character as argument");
	lir::Chunk chunk {};
	auto& op = args[0];
	chunk.emit(Opcode::PRINTF, op);
	return {chunk, {}};
}

Result read_int(Compiler& comp, vector<lir::Operand>) {
	lir::Chunk chunk {};
	lir::Operand tmp = comp.make_register();
	chunk.emit(Opcode::READV, tmp);
	return {chunk, tmp};
}

Result read_char(Compiler& comp, vector<lir::Operand>) {
	lir::Chunk chunk {};
	lir::Operand tmp = comp.make_register();
	chunk.emit(Opcode::READC, tmp);
	return {chunk, tmp};
}

Result make_array(Compiler& comp, vector<lir::Operand> args) {
	if (args.size() != 1)
		err("The `array' builtin expects a size as the first and only argument.");

	lir::Chunk chunk {};
	auto addr =
		lir::Operand(Register(comp.reg_count++, lir::Type::make_integer_array()));
	addr.as_register().is_lvalue_pointer = true;

	auto t1 = comp.make_register();
	chunk.emit_loada(t1, lir::Immediate(0), args[0]);
	chunk.emit_alloca(addr, t1);
	return {chunk, addr};
}

} // namespace builtin

lir::Chunk Compiler::compile() {
	lir::Chunk preamble {};
	lir::Chunk chunk;

	auto main = make_label();

	chunk.add_label(main);

	SignalHandlers handlers {};

	Context ctx {
		.handlers = handlers,
	};

	auto res_ = compile(ast.root_index, ctx);
	chunk = chunk + res_.code;

	preamble.emit(Opcode::JMP, main);

	lir::Chunk all_functions {};
	for (const auto& f : functions) {
		all_functions = all_functions + f;
	}

	lir::Chunk res = preamble + all_functions + chunk;

	return res;
}

lir::Operand Compiler::make_register() {
	return lir::Operand(Register(reg_count++, lir::Type::make_integer()));
}

lir::Label Compiler::make_label() { return lir::Label(label_count++); }

Result Compiler::compile_or_allocate_lvalue(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	if (node.type == NodeType::PATH) {
		return compile_lvalue(node_idx, ctx);
	} else {
		bool is_array = false;
		{
			auto t1 = tpc.node_to_type.at(node_idx);
			auto t2 = get_datatype(t1);
			if (nullptr != std::dynamic_pointer_cast<Array>(t2)) is_array = true;
		}
		if (is_array) {
			return compile(node_idx, ctx);
		} else {
			lir::Chunk chunk {};
			auto res = compile(node_idx, ctx);
			chunk = chunk + res.code;
			auto res_opnd = res.opnd;
			auto t1 = make_register();
			chunk.emit_alloca(t1, lir::Immediate(1));
			chunk.emit_storea(res_opnd, lir::Immediate(0), t1);
			return {chunk, t1};
		}
	}
}

Result Compiler::compile_app(NodeIndex node_idx, Context ctx) {
	lir::Chunk chunk {};

	const auto& node = ast.at(node_idx);
	const auto& func_node = ast.at(node[0]);
	const auto& args_node = ast.at(node[1]);

	vector<lir::Operand> args {};

	for (const auto arg_idx : args_node) {
		const auto res = compile_or_allocate_lvalue(arg_idx, ctx);
		chunk = chunk + res.code;
		args.push_back(res.opnd);
	}

	assert(func_node.type == NodeType::GUID);
	auto scope = symtab.relative_scope_of(func_node.guid, node[0]).value();
	if (scope == RelativeScope::BUILTIN) {
		const auto name = symtab.name_of(func_node.guid).value();
		const auto func_name = std::string_view(pool.find(name));
		constexpr auto get_name = [](const auto& x) { return x.name; };
		if (const auto* builtin_ptr =
		      std::ranges::find(builtin::builtins, func_name, get_name);
		    builtin_ptr != std::end(builtin::builtins)) {
			const auto& builtin = *builtin_ptr;
			const auto result = builtin.ptr(*this, args);
			chunk = chunk + result.code;
			return {chunk, result.opnd};
		} else {
			throw std::runtime_error("unknown builtin");
		}
	} else {
		const auto func_opnd = variables.at(func_node.guid);

		for (const auto& arg : std::ranges::reverse_view(args))
			chunk.emit(Opcode::PUSH, arg);

		chunk.emit(Opcode::CALL, func_opnd);
		const auto res = make_register();
		chunk.emit(Opcode::POP, res);
		chunk.result_opnd = res;

		return {chunk, res};
	}
}

Result Compiler::compile_if(NodeIndex node_idx, Context ctx) {
	lir::Chunk chunk {};
	const auto& node = ast.at(node_idx);

	const auto cond_idx = node[0];
	const auto then_idx = node[1];
	const auto else_idx = node[2];

	const auto res = make_register();

	const auto l1 = make_label();
	const auto l2 = make_label();

	const auto r0 = compile(cond_idx, ctx);
	chunk = chunk + r0.code;

	chunk.emit(Opcode::JMP_FALSE, r0.opnd, l1);

	const auto r1 = compile(then_idx, ctx);
	chunk = chunk + r1.code;

	chunk.emit(Opcode::MOV, res, r1.opnd);
	chunk.emit(Opcode::JMP, l2);
	chunk.add_label(l1);

	const auto r2 = compile(else_idx, ctx);
	chunk = chunk + r2.code;

	chunk.emit(Opcode::MOV, res, r2.opnd);
	chunk.add_label(l2);

	return {chunk, res};
}

Result Compiler::compile_for(NodeIndex node_idx, Context ctx) {
	lir::Chunk chunk {};
	const auto& node = ast.at(node_idx);

	const auto decl_idx = node[0];
	const auto to_idx = node[1];
	const auto step_idx = node[2];
	const auto then_idx = node[3];

	const auto& step_node = ast.at(step_idx);

	const auto beg = make_label();
	const auto inc = make_label();
	const auto end = make_label();
	const auto cmp = make_register();

	const auto result_register = make_register();

	SignalHandlers new_handlers {
		{inc, result_register},
		{end, result_register},
		ctx.handlers.return_handler,
		true,
		true,
		ctx.handlers.has_return_handler
	};

	const auto step = [&]() -> lir::Operand {
		if (step_node.type != NodeType::EMPTY) {
			auto step_res = compile(step_idx, ctx);
			chunk = chunk + step_res.code;
			return step_res.opnd;
		} else {
			return lir::Immediate(1);
		}
	}();

	Context header_ctx {
		.handlers = ctx.handlers,
	};

	Context body_ctx {
		.handlers = new_handlers,
	};

	const auto var_res = compile(decl_idx, header_ctx);
	chunk = chunk + var_res.code;
	const auto var = var_res.opnd;

	const auto to_res = compile(to_idx, header_ctx);
	chunk = chunk + to_res.code;
	const auto to = to_res.opnd;

	const auto t1 = make_register();
	const auto t2 = make_register();

	chunk.add_label(beg);
	chunk.emit_loada(t1, lir::Immediate(0), var);
	chunk.emit(Opcode::EQ, cmp, t1, to);
	chunk.emit(Opcode::JMP_TRUE, cmp, end);

	const auto exp_res = compile(then_idx, body_ctx);
	chunk = chunk + exp_res.code;
	const auto exp = exp_res.opnd;

	chunk.emit(Opcode::MOV, result_register, exp);

	chunk.add_label(inc);
	chunk.emit(Opcode::ADD, t2, t1, step);
	chunk.emit_storea(t2, lir::Immediate(0), var);
	chunk.emit(Opcode::JMP, beg);
	chunk.add_label(end);

	return {chunk, result_register};
}

Result Compiler::compile_when(NodeIndex node_idx, Context ctx) {
	lir::Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto cond_idx = node[0];
	auto then_idx = node[1];

	lir::Operand l1 = make_label();
	lir::Operand res = make_register();

	auto cond_res = compile(cond_idx, ctx);
	chunk = chunk + cond_res.code;
	lir::Operand cond_opnd = cond_res.opnd;

	chunk.emit(Opcode::MOV, res, {});
	chunk.emit(Opcode::JMP_FALSE, cond_opnd, l1);

	auto yes_res = compile(then_idx, ctx);
	chunk = chunk + yes_res.code;
	lir::Operand yes_opnd = yes_res.opnd;

	chunk.emit(Opcode::MOV, res, yes_opnd);
	chunk.add_label(l1);

	return {chunk, res};
}

Result Compiler::compile_while(NodeIndex node_idx, Context ctx) {
	lir::Chunk chunk {};
	const auto& node = ast.at(node_idx);

	lir::Operand beg = make_label();
	lir::Operand end = make_label();

	lir::Operand result_register = make_register();

	SignalHandlers new_handlers {
		{beg.as_label(), result_register},
		{end.as_label(), result_register},
		ctx.handlers.return_handler,
		true,
		true,
		ctx.handlers.has_return_handler
	};

	chunk.add_label(beg);

	auto cond_res = compile(node[0], ctx);
	chunk = chunk + cond_res.code;
	lir::Operand cond = cond_res.opnd;

	chunk.emit(Opcode::JMP_FALSE, cond, end);

	Context body_ctx {
		.handlers = new_handlers,
	};

	auto exp_res = compile(node[1], body_ctx);
	chunk = chunk + exp_res.code;
	lir::Operand exp = exp_res.opnd;

	chunk.emit(Opcode::MOV, result_register, exp);

	chunk.emit(Opcode::JMP, beg);
	chunk.add_label(end);

	return {chunk, result_register};
}

Result Compiler::compile_lvalue(NodeIndex node_idx, Context ctx) {
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
			lir::Chunk chunk {};
			const auto& node = ast.at(node_idx);
			auto base_res = compile_lvalue(node[0], ctx);
			chunk = chunk + base_res.code;
			auto base = base_res.opnd;
			auto off_res = compile(node[1], ctx);
			chunk = chunk + off_res.code;
			auto off = off_res.opnd;

			auto tmp = make_register();
			auto t1 = make_register();
			chunk.emit_shifta(tmp, off, base);
			return {chunk, tmp};
		}
		case NodeType::ADD: assert(false);
		case NodeType::SUB: assert(false);
		case NodeType::MUL: assert(false);
		case NodeType::DIV: assert(false);
		case NodeType::MOD: assert(false);
		case NodeType::NOT: assert(false);
		case NodeType::ID: assert(false);
		case NodeType::STR: assert(false);
		case NodeType::VAR_DECL: assert(false);
		case NodeType::FUN_DECL: assert(false);
		case NodeType::NIL: assert(false);
		case NodeType::TRUE: assert(false);
		case NodeType::FALSE: assert(false);
		case NodeType::LET: assert(false);
		case NodeType::CHAR: assert(false);
		case NodeType::PATH: {
			return compile_lvalue(node[0], ctx);
		}
		case NodeType::AS: assert(false);
		case NodeType::INSTANCE: assert(false);
		case NodeType::METALIST: assert(false);
		case NodeType::GUID: {
			assert(variables.contains(node.guid));
			return {{}, variables.at(node.guid)};
		}
	}
	assert(false);
}

Result Compiler::compile_var_decl(NodeIndex node_idx, Context ctx) {
	lir::Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto id_idx = node[0];
	auto opt_type_idx = node[1];
	auto exp_idx = node[2];

	(void)opt_type_idx;

	const auto& id_node = ast.at(id_idx);
	const auto& exp_node = ast.at(exp_idx);

	assert(id_node.type == NodeType::GUID);

	const auto name =
		std::string_view(pool.find(symtab.name_of(id_node.guid).value()));
	const auto comment = std::format("creating variable \"{}\"", name);

	bool is_array = false;
	{
		auto t1 = tpc.node_to_type.at(exp_idx);
		auto t2 = get_datatype(t1);
		if (nullptr != std::dynamic_pointer_cast<Array>(t2)) is_array = true;
	}

	if (exp_node.type == NodeType::PATH) {
		auto initial_res = compile_lvalue(exp_idx, ctx);
		chunk = chunk + initial_res.code;
		auto initial = initial_res.opnd;
		// FIXME: properly set register type
		const auto var = variables[id_node.guid] = make_register();
		chunk.emit_clonea(var, initial);
		return {chunk, var};
	} else {
		auto initial_res = compile(exp_idx, ctx);
		chunk = chunk + initial_res.code;
		auto initial = initial_res.opnd;
		// FIXME: properly set register type
		const auto var = variables[id_node.guid] = make_register();
		if (is_array) {
			chunk.emit_mov(var, initial);
		} else {
			chunk.emit_alloca(var, lir::Immediate(1));
			chunk.emit_storea(initial, lir::Immediate(0), var);
		}
		return {chunk, var};
	}
}

Result Compiler::compile_fun_decl(NodeIndex node_idx, Context ctx) {
	lir::Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto id_idx = node[0];
	auto params_idx = node[1];
	auto opt_type_idx = node[2];
	auto body_idx = node[3];

	const auto& id_node = ast.at(id_idx);
	const auto& params_node = ast.at(params_idx);

	assert(id_node.type == NodeType::GUID);

	(void)opt_type_idx;

	const auto func_name = make_label();
	const auto opnd = variables[id_node.guid] = func_name;

	lir::Chunk func {};

	func.add_label(func_name);
	func.emit(Opcode::FUNC);

	for (auto param_idx : params_node) {
		const auto& param_node = ast.at(param_idx);
		auto arg = make_register();
		func.emit(Opcode::POP, arg);
		assert(param_node.type == NodeType::GUID);
		variables[param_node.guid] = arg;
	}

	Context body_ctx {
		.handlers = ctx.handlers,
	};

	auto op_res = compile(body_idx, body_ctx);
	func = func + op_res.code;
	auto op = op_res.opnd;

	func.emit(Opcode::PUSH, op);
	func.emit(Opcode::RET);

	functions.push_back(func);

	return {chunk, func_name};
}

Result Compiler::compile_ass(NodeIndex node_idx, Context ctx) {
	lir::Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto cell_res = compile_lvalue(node[0], ctx);
	chunk = chunk + cell_res.code;
	lir::Operand cell = cell_res.opnd;

	auto exp_res = compile(node[1], ctx);
	chunk = chunk + exp_res.code;
	lir::Operand exp = exp_res.opnd;

	chunk.emit_storea(exp, lir::Immediate(0), cell);

	return {chunk, exp};
}

Result Compiler::compile_let(NodeIndex node_idx, Context ctx) {
	lir::Chunk chunk {};
	const auto& node = ast.at(node_idx);

	auto decls_idx = node[0];
	auto exp_idx = node[1];

	const auto& decls_node = ast.at(decls_idx);

	{
		Context exp_ctx {
			.handlers = ctx.handlers,
		};
		for (size_t i = 0; i < decls_node.branch.children_count; i++) {
			auto res = compile(decls_node[i], exp_ctx);
			chunk = chunk + res.code;
		}
		auto res_res = compile(exp_idx, exp_ctx);
		chunk = chunk + res_res.code;
		lir::Operand res = res_res.opnd;
		return {chunk, res};
	}
}

Result Compiler::compile_str(NodeIndex node_idx, Context) {
	lir::Chunk chunk {};
	const auto& node = ast.at(node_idx);

	// allocates a static buffer for the string characters, ending in a
	// sentinel null character and returns a pointer to the start

	const char* str = pool.find(node.str_id);
	auto str_len = strlen(str);

	auto t1 = make_register();
	chunk.emit_alloca(t1, lir::Immediate((int)str_len));
	for (size_t i = 0; i < str_len; i++) {
		chunk.emit_storea(lir::Immediate(str[i]), lir::Immediate((int)i), t1);
	}

	chunk.result_opnd = t1;
	return {chunk, t1};
}

Result Compiler::compile_at(NodeIndex node_idx, Context ctx) {
	lir::Chunk chunk {};
	const auto& node = ast.at(node_idx);

	// evaluates to a temporary register containing the address of the lvalue

	auto base_res = compile(node[0], ctx);
	chunk = chunk + base_res.code;
	lir::Operand base = base_res.opnd;
	auto off_res = compile(node[1], ctx);
	chunk = chunk + off_res.code;
	lir::Operand off = off_res.opnd;

	auto is_array = false;
	const auto t1 = tpc.node_to_type.at(node[0]);
	const auto t3 = get_datatype(t1);
	if (auto t2 = std::dynamic_pointer_cast<Array>(t3)) {
		is_array = true;
	}
	if (not is_array) err("Base must be an lvalue");

	lir::Operand tmp = make_register();
	tmp.as_register().is_lvalue_pointer = true;
	chunk.emit(Opcode::ADD, tmp, base, off);

	return {chunk, lir::Operand(tmp)};
}

// FIXME: Temporary workaround
#define COMPILE_WITH_HANDLER(METH) return METH(node_idx, ctx);

Result Compiler::compile_block(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	lir::Chunk chunk {};
	lir::Operand opnd;
	Context inner_ctx {
		.handlers = ctx.handlers,
	};
	for (size_t i = 0; i < node.branch.children_count; i++) {
		auto opnd_res = compile(node[i], inner_ctx);
		chunk = chunk + opnd_res.code;
		opnd = opnd_res.opnd;
	}

	return {chunk, opnd};
}

Result Compiler::compile_break(NodeIndex node_idx, Context ctx) {
	if (not ctx.handlers.has_break_handler)
		throw std::runtime_error("Can't break outside of loops");

	const auto res = ctx.handlers.break_handler.result_register;
	const auto dest = ctx.handlers.break_handler.destination_label;

	const auto& node = ast.at(node_idx);
	assert(node.branch.children_count == 1);

	lir::Chunk chunk {};
	const auto r0 = compile(node[0], ctx);
	chunk = chunk + r0.code;
	chunk.emit(Opcode::MOV, res, r0.opnd);
	chunk.emit(Opcode::JMP, dest);
	return {chunk, {}};
}

Result Compiler::compile_continue(NodeIndex node_idx, Context ctx) {
	if (not ctx.handlers.has_continue_handler)
		throw std::runtime_error("can't continue outside of loops");

	const auto res = ctx.handlers.continue_handler.result_register;
	const auto dest = ctx.handlers.continue_handler.destination_label;

	const auto& node = ast.at(node_idx);
	assert(node.branch.children_count == 1);

	lir::Chunk chunk {};
	const auto r0 = compile(node[0], ctx);
	chunk = chunk + r0.code;
	chunk.emit(Opcode::MOV, res, r0.opnd);
	chunk.emit(Opcode::JMP, dest);
	return {chunk, {}};
}

Result Compiler::compile(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::APP: COMPILE_WITH_HANDLER(compile_app)
		case NodeType::NUM: {
			lir::Chunk chunk {};
			auto opnd = lir::Immediate(node.num);
			chunk.result_opnd = opnd;
			return {chunk, opnd};
		}
		case NodeType::BLK: COMPILE_WITH_HANDLER(compile_block)
		case NodeType::IF: COMPILE_WITH_HANDLER(compile_if)
		case NodeType::WHEN: COMPILE_WITH_HANDLER(compile_when)
		case NodeType::FOR: COMPILE_WITH_HANDLER(compile_for)
		case NodeType::WHILE: COMPILE_WITH_HANDLER(compile_while)
		case NodeType::BREAK: COMPILE_WITH_HANDLER(compile_break)
		case NodeType::CONTINUE: COMPILE_WITH_HANDLER(compile_continue)
		case NodeType::ASS: COMPILE_WITH_HANDLER(compile_ass)

#define BINARY_ARITH(OPCODE)                   \
	{                                            \
		lir::Chunk chunk {};                       \
		auto left_node = node[0];                  \
		auto right_node = node[1];                 \
                                               \
		auto left_res = compile(left_node, ctx);   \
		chunk = chunk + left_res.code;             \
		auto right_res = compile(right_node, ctx); \
		chunk = chunk + right_res.code;            \
		lir::Operand left = left_res.opnd;         \
		lir::Operand right = right_res.opnd;       \
		lir::Operand res = make_register();        \
                                               \
		chunk.emit(OPCODE, res, left, right);      \
		chunk.result_opnd = res;                   \
		return {chunk, res};                       \
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
			lir::Chunk chunk {};
			lir::Operand res = make_register();
			auto inverse_res = compile(node[0], ctx);
			chunk = chunk + inverse_res.code;
			lir::Operand inverse = inverse_res.opnd;
			chunk.emit(Opcode::NOT, res, inverse);
			return {chunk, res};
		}
		case NodeType::AT: {
			lir::Chunk chunk {};
			auto res = compile_lvalue(node_idx, ctx);
			chunk = chunk + res.code;
			auto res_opnd = res.opnd;
			auto tmp = make_register();
			chunk.emit_loada(tmp, lir::Immediate(0), res_opnd);
			return {chunk, tmp};
		}
		case NodeType::ID: assert(false);
		case NodeType::STR: COMPILE_WITH_HANDLER(compile_str)
		case NodeType::VAR_DECL: COMPILE_WITH_HANDLER(compile_var_decl)
		case NodeType::FUN_DECL: COMPILE_WITH_HANDLER(compile_fun_decl)
		case NodeType::NIL: return {{}, lir::Immediate(0)};
		case NodeType::TRUE: return {{}, lir::Immediate(1)};
		case NodeType::FALSE: return {{}, lir::Immediate(0)};
		case NodeType::LET: COMPILE_WITH_HANDLER(compile_let)
		case NodeType::EMPTY: assert(false && "unreachable");
		case NodeType::CHAR: {
			lir::Chunk chunk {};
			auto opnd = lir::Immediate(node.character);
			chunk.result_opnd = opnd;
			return {chunk, opnd};
		}
		case NodeType::PATH: {
			lir::Chunk chunk {};
			auto res = compile_lvalue(node_idx, ctx);
			chunk = chunk + res.code;
			auto res_opnd = res.opnd;
			auto tmp = make_register();
			chunk.emit_loada(tmp, lir::Immediate(0), res_opnd);
			chunk.result_opnd = tmp;
			return {chunk, tmp};
		}
		case NodeType::INSTANCE: assert(false);
		case NodeType::AS: return compile(node[0], ctx);
		case NodeType::METALIST: assert(false);
		case NodeType::GUID: {
			lir::Chunk chunk {};
			auto res = compile_lvalue(node_idx, ctx);
			chunk = chunk + res.code;
			auto res_opnd = res.opnd;
			auto tmp = make_register();
			chunk.emit_loada(tmp, lir::Immediate(0), res_opnd);
			return {chunk, tmp};
		}
	}
	assert(false);
}

} // namespace compiler
