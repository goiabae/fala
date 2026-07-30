#include "lower/ast_to_lir.hpp"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <iterator>
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

} // namespace builtin

enum class PassingConvention {
	CLONE,
	ALIAS,
	INVALID,
};

static PassingConvention passing_convention_for(
	ConcreteMode param_mode, ConcreteMode arg_mode
) {
	if (param_mode == ConcreteMode::OUT) {
		if (arg_mode == ConcreteMode::OUT) {
			return PassingConvention::ALIAS;
		} else if (arg_mode == ConcreteMode::VAL) {
			return PassingConvention::INVALID;
		} else if (arg_mode == ConcreteMode::VAR) {
			return PassingConvention::ALIAS;
		} else {
			assert(false);
		}
	} else if (param_mode == ConcreteMode::VAL) {
		if (arg_mode == ConcreteMode::OUT) {
			return PassingConvention::CLONE;
		} else if (arg_mode == ConcreteMode::VAL) {
			return PassingConvention::ALIAS;
		} else if (arg_mode == ConcreteMode::VAR) {
			return PassingConvention::CLONE;
		} else {
			assert(false);
		}
	} else if (param_mode == ConcreteMode::VAR) {
		if (arg_mode == ConcreteMode::OUT) {
			return PassingConvention::CLONE;
		} else if (arg_mode == ConcreteMode::VAL) {
			return PassingConvention::CLONE;
		} else if (arg_mode == ConcreteMode::VAR) {
			return PassingConvention::CLONE;
		} else {
			assert(false);
		}
	} else {
		assert(false);
	}
}

lir::Chunk Compiler::compile() {
	allocate_variables();

	const auto main = make_label();
	const SignalHandlers handlers {};
	const Context ctx {.handlers = handlers};
	const auto c0 = compile(ast.root_index, ctx);

	lir::Chunk chunk {};
	chunk.emit(Opcode::JMP, main);
	for (const auto& f : functions) chunk += f;
	chunk.add_label(main);
	chunk += c0.code;

	return chunk;
}

lir::Operand Compiler::make_register() {
	return lir::Operand(lir::Register(reg_count++));
}

lir::Label Compiler::make_label() { return lir::Label(label_count++); }

Result Compiler::compile_or_allocate_lvalue(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	if (node.type == NodeType::PATH) {
		return compile_lvalue(node_idx, ctx);
	} else {
		const auto c0 = compile(node_idx, ctx);
		const auto t1 = make_register();
		lir::Chunk chunk {};
		chunk += c0.code;
		chunk.emit_alloca(t1, lir::Immediate(1));
		chunk.emit_storea(c0.opnd, lir::Immediate(0), t1);
		return {chunk, t1};
	}
}

Result Compiler::compile_app(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto& func_node = ast.at(node[0]);
	const auto& args_node = ast.at(node[1]);

	assert(func_node.type == NodeType::GUID);
	assert(args_node.type == NodeType::METALIST);

	lir::Chunk chunk {};

	std::vector<lir::Operand> args {};
	std::vector<std::shared_ptr<Type>> arg_tys {};
	for (const auto arg_idx : args_node) {
		const auto res = compile_or_allocate_lvalue(arg_idx, ctx);
		chunk += res.code;
		auto ty = tpc.node_to_type.at(arg_idx);
		arg_tys.push_back(ty);
		args.push_back(res.opnd);
	}

	const auto scope = symtab.relative_scope_of(func_node.guid, node[0]).value();
	if (scope == RelativeScope::BUILTIN) {
		const auto name = symtab.name_of(func_node.guid).value();
		const auto func_name = std::string_view(pool.find(name));
		constexpr auto get_name = [](const auto& x) { return x.name; };
		if (const auto* builtin_ptr =
		      std::ranges::find(builtin::builtins, func_name, get_name);
		    builtin_ptr != std::end(builtin::builtins)) {
			const auto& builtin = *builtin_ptr;
			const auto result = builtin.ptr(*this, args);
			chunk += result.code;
			return {chunk, result.opnd};
		} else {
			throw std::runtime_error("unknown builtin");
		}
	} else {
		const auto func_opnd = variables.at(func_node.guid);

		const auto ty = tpc.variables.at(func_node.guid);
		const auto dty = get_datatype(ty);
		const auto fty = std::dynamic_pointer_cast<Function>(dty);
		for (const auto& [i, arg] :
		     std::ranges::views::enumerate(std::ranges::reverse_view(args))) {
			const auto param_mode =
				(*(fty->inputs.at(static_cast<std::size_t>(i))->mode))
					.to_concrete()
					.value();
			const auto arg_mode =
				arg_tys.at(static_cast<std::size_t>(i))->mode->to_concrete().value();
			const auto convention = passing_convention_for(param_mode, arg_mode);

			if (convention == PassingConvention::ALIAS) {
				chunk.emit(Opcode::PUSH, arg);
			} else if (convention == PassingConvention::CLONE) {
				const auto t = make_register();
				chunk.emit_clonea(t, arg);
				chunk.emit(Opcode::PUSH, t);
			} else if (convention == PassingConvention::INVALID) {
				throw std::runtime_error("cannot call function. conflicting modes");
			} else {
				assert(false);
			}
		}

		chunk.emit(Opcode::CALL, func_opnd);
		const auto res = make_register();
		chunk.emit(Opcode::POP, res);
		chunk.result_opnd = res;

		return {chunk, res};
	}
}

Result Compiler::compile_if(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto cond_idx = node[0];
	const auto then_idx = node[1];
	const auto else_idx = node[2];
	const auto res = make_register();
	const auto l1 = make_label();
	const auto l2 = make_label();
	const auto r0 = compile(cond_idx, ctx);
	const auto r1 = compile(then_idx, ctx);
	const auto r2 = compile(else_idx, ctx);

	lir::Chunk chunk {};
	chunk += r0.code;
	chunk.emit(Opcode::JMP_FALSE, r0.opnd, l1);
	chunk += r1.code;
	chunk.emit(Opcode::MOV, res, r1.opnd);
	chunk.emit(Opcode::JMP, l2);
	chunk.add_label(l1);
	chunk += r2.code;
	chunk.emit(Opcode::MOV, res, r2.opnd);
	chunk.add_label(l2);

	return {chunk, res};
}

Result Compiler::compile_for(NodeIndex node_idx, Context ctx) {
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

	Context header_ctx {ctx};
	Context body_ctx {
		.handlers = new_handlers,
	};

	const auto c0 = [&]() -> Result {
		if (step_node.type != NodeType::EMPTY) {
			return compile(step_idx, ctx);
		} else {
			return {{}, lir::Immediate(1)};
		}
	}();

	const auto c1 = compile(decl_idx, header_ctx);
	const auto c2 = compile(to_idx, header_ctx);
	const auto c3 = compile(then_idx, body_ctx);

	const auto t1 = make_register();
	const auto t2 = make_register();

	lir::Chunk chunk {};
	chunk += c0.code;
	chunk += c1.code;
	chunk += c2.code;
	chunk.add_label(beg);
	chunk.emit_loada(t1, lir::Immediate(0), c1.opnd);
	chunk.emit(Opcode::EQ, cmp, t1, c2.opnd);
	chunk.emit(Opcode::JMP_TRUE, cmp, end);
	chunk += c3.code;
	chunk.emit(Opcode::MOV, result_register, c3.opnd);
	chunk.add_label(inc);
	chunk.emit(Opcode::ADD, t2, t1, c0.opnd);
	chunk.emit_storea(t2, lir::Immediate(0), c1.opnd);
	chunk.emit(Opcode::JMP, beg);
	chunk.add_label(end);

	return {chunk, result_register};
}

Result Compiler::compile_when(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto cond_idx = node[0];
	const auto then_idx = node[1];
	const auto l1 = make_label();
	const auto r = make_register();
	const auto c0 = compile(cond_idx, ctx);
	const auto c1 = compile(then_idx, ctx);

	lir::Chunk chunk {};
	chunk += c0.code;
	chunk.emit(Opcode::MOV, r, {});
	chunk.emit(Opcode::JMP_FALSE, c0.opnd, l1);
	chunk += c1.code;
	chunk.emit(Opcode::MOV, r, c1.opnd);
	chunk.add_label(l1);

	return {chunk, r};
}

Result Compiler::compile_while(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto beg = make_label();
	const auto end = make_label();
	const auto result_register = make_register();

	SignalHandlers new_handlers {
		{beg, result_register},
		{end, result_register},
		ctx.handlers.return_handler,
		true,
		true,
		ctx.handlers.has_return_handler
	};

	Context body_ctx {
		.handlers = new_handlers,
	};

	const auto r0 = compile(node[0], ctx);
	const auto r1 = compile(node[1], body_ctx);

	lir::Chunk chunk {};
	chunk.add_label(beg);
	chunk += r0.code;
	chunk.emit(Opcode::JMP_FALSE, r0.opnd, end);
	chunk += r1.code;
	chunk.emit(Opcode::MOV, result_register, r1.opnd);
	chunk.emit(Opcode::JMP, beg);
	chunk.add_label(end);

	return {chunk, result_register};
}

#if 0
static bool fits_in_register(const Datatype& d) {
	if (dynamic_cast<const Integer*>(&d) != nullptr) return true;
	if (dynamic_cast<const Nil*>(&d) != nullptr) return true;
	return false;
}
#endif

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
			//                   +---> [... v]
			//                   |
			// c0 : *---> *---> [*, n $]
			// r0 : *---> v
			//
			const auto& node = ast.at(node_idx);
			const auto c0 = compile_lvalue(node[0], ctx);
			const auto c1 = compile(node[1], ctx);
			const auto r0 = make_register();
			const auto r1 = make_register();
			const auto r2 = make_register();

			lir::Chunk chunk {};
			chunk += c0.code;
			chunk += c1.code;
			chunk.emit_loada(r1, lir::Immediate(0), c0.opnd);
			chunk.emit_loada(r2, lir::Immediate(0), r1);
			chunk.emit_shifta(r0, c1.opnd, r2);
			return {chunk, r0};
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
		case NodeType::MODE: assert(false);
		case NodeType::PARAM: assert(false);
		case NodeType::RECORD_LITERAL: assert(false);
		case NodeType::FIELD: assert(false);
		case NodeType::FIELD_ACCESS: {
			const auto r0 = compile_lvalue(node[0], ctx);
			const auto t0 = std::dynamic_pointer_cast<Record>(
				get_datatype(tpc.node_to_type.at(node[0]))
			);
			const auto& field_node = ast.at(node[1]);
			assert(field_node.type == NodeType::ID);
			const auto field_name = std::string(pool.find(field_node.str_id));
			auto found = std::ranges::find(t0->fields, field_name, [](const auto& x) {
				return x.first;
			});
			assert(found != t0->fields.end());
			auto d = std::distance(t0->fields.begin(), found);
			const auto field_ty = found->second;
			const auto t1 = make_register();
			const auto t3 = make_register();
			lir::Chunk chunk {};
			chunk += r0.code;
			chunk.emit_loada(t3, lir::Immediate(0), r0.opnd);
			chunk.emit_shifta(t1, lir::Immediate(static_cast<int>(d)), t3);
			return {chunk, t1};
		}
		case NodeType::ARRAY_LITERAL: assert(false);
		case NodeType::REPEAT: assert(false);
		case NodeType::LEN: assert(false);
	}
	assert(false);
}

Result Compiler::compile_var_decl(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);

	const auto id_idx = node[1];
	const auto exp_idx = node[3];
	const auto& id_node = ast.at(id_idx);
	const auto& exp_node = ast.at(exp_idx);
	assert(id_node.type == NodeType::GUID);

	const auto name =
		std::string_view(pool.find(symtab.name_of(id_node.guid).value()));
	const auto comment = std::format("creating variable \"{}\"", name);

	lir::Chunk chunk {};
	const auto var = variables.at(id_node.guid);

	if (exp_node.type == NodeType::PATH) {
		const auto r0 = compile_lvalue(exp_idx, ctx);
		chunk += r0.code;
		chunk.emit_clonea(var, r0.opnd);
	} else {
		const auto r0 = compile(exp_idx, ctx);
		chunk += r0.code;
		chunk.emit_alloca(var, lir::Immediate(1));
		chunk.emit_storea(r0.opnd, lir::Immediate(0), var);
	}
	return {chunk, var};
}

void Compiler::allocate_variables() {
	const auto syms = symtab.all_symbols();
	for (const auto& sym : syms) {
		const auto scope = symtab.declaration_scope_of(sym).value();
		if (scope != symtab.builtin_scope) {
			const auto ty = tpc.variables.at(sym);
			const auto dty = get_datatype(ty);
			const auto is_function =
				std::dynamic_pointer_cast<Function>(dty) != nullptr;
			if (is_function) {
				variables[sym] = make_label();
			} else {
				variables[sym] = make_register();
			}
		}
	}
}

Result Compiler::compile_fun_decl(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto id_idx = node[0];
	const auto params_idx = node[1];
	const auto body_idx = node[3];
	const auto& id_node = ast.at(id_idx);
	const auto& params_node = ast.at(params_idx);
	assert(id_node.type == NodeType::GUID);
	const auto func_name = variables.at(id_node.guid);
	Context body_ctx {
		.handlers = ctx.handlers,
	};
	const auto r0 = compile(body_idx, body_ctx);

	{
		lir::Chunk func {};
		func.add_label(func_name);
		func.emit(Opcode::FUNC);
		for (auto param_idx : params_node) {
			const auto& param_node = ast.at(param_idx);
			assert(param_node.type == NodeType::PARAM);
			const auto& param_name = ast.at(param_node[1]);
			assert(param_name.type == NodeType::GUID);
			const auto arg = variables.at(param_name.guid);
			func.emit(Opcode::POP, arg);
		}
		func = func + r0.code;
		func.emit(Opcode::PUSH, r0.opnd);
		func.emit(Opcode::RET);
		functions.push_back(func);
	}

	return {{}, func_name};
}

Result Compiler::compile_ass(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto r0 = compile_lvalue(node[0], ctx);
	const auto r1 = compile(node[1], ctx);

	lir::Chunk chunk {};
	chunk += r0.code;
	chunk += r1.code;
	chunk.emit_storea(r1.opnd, lir::Immediate(0), r0.opnd);

	return {chunk, r1.opnd};
}

Result Compiler::compile_let(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto decls_idx = node[0];
	const auto exp_idx = node[1];
	const auto& decls_node = ast.at(decls_idx);
	Context exp_ctx {ctx};
	const auto r0 = compile(exp_idx, exp_ctx);

	lir::Chunk chunk {};
	for (const auto decl_idx : decls_node) {
		const auto ri = compile(decl_idx, exp_ctx);
		chunk += ri.code;
	}
	chunk += r0.code;

	return {chunk, r0.opnd};
}

Result Compiler::compile_str(NodeIndex node_idx, Context) {
	//
	// str : "bbbbbbb" n
	// t2  : *---> [*, n $]
	//              |
	//              +---> [b, b, ..., b] (n times)
	//

	const auto& node = ast.at(node_idx);
	const auto str = std::string_view(pool.find(node.str_id));
	const auto t1 = make_register();
	const auto t2 = make_register();

	lir::Chunk chunk {};
	chunk.emit_alloca(t1, lir::Immediate((int)str.size()));
	chunk.emit(Opcode::INITA, t1, lir::ByteString {std::string(str)});
	chunk.emit_alloca(t2, lir::Immediate(2));
	chunk.emit_storea(t1, lir::Immediate(0), t2);
	chunk.emit_storea(
		lir::Immediate(static_cast<int>(str.size())), lir::Immediate(1), t2
	);
	chunk.result_opnd = t2;

	return {chunk, t2};
}

Result Compiler::compile_at(NodeIndex node_idx, Context ctx) {
	//
	// res : *---> value
	// tmp : value
	//
	lir::Chunk chunk {};
	auto res = compile_lvalue(node_idx, ctx);
	chunk = chunk + res.code;
	auto res_opnd = res.opnd;
	auto tmp = make_register();
	chunk.emit_loada(tmp, lir::Immediate(0), res_opnd);
	return {chunk, tmp};
}

// FIXME: Temporary workaround
#define COMPILE_WITH_HANDLER(METH) return METH(node_idx, ctx);

Result Compiler::compile_block(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	lir::Chunk chunk {};
	lir::Operand opnd;
	const Context inner_ctx {
		.handlers = ctx.handlers,
	};
	for (const auto stmt_idx : node) {
		const auto ri = compile(stmt_idx, inner_ctx);
		chunk += ri.code;
		opnd = ri.opnd;
	}
	return {chunk, opnd};
}

Result Compiler::compile_break(NodeIndex node_idx, Context ctx) {
	assert(ctx.handlers.has_break_handler);
	const auto res = ctx.handlers.break_handler.result_register;
	const auto dest = ctx.handlers.break_handler.destination_label;
	const auto& node = ast.at(node_idx);
	assert(node.branch.children_count == 1);
	const auto r0 = compile(node[0], ctx);
	lir::Chunk chunk {};
	chunk += r0.code;
	chunk.emit(Opcode::MOV, res, r0.opnd);
	chunk.emit(Opcode::JMP, dest);
	return {chunk, {}};
}

Result Compiler::compile_continue(NodeIndex node_idx, Context ctx) {
	assert(ctx.handlers.has_continue_handler);
	const auto res = ctx.handlers.continue_handler.result_register;
	const auto dest = ctx.handlers.continue_handler.destination_label;
	const auto& node = ast.at(node_idx);
	assert(node.branch.children_count == 1);
	const auto r0 = compile(node[0], ctx);
	lir::Chunk chunk {};
	chunk += r0.code;
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
		const auto left_node = node[0];            \
		const auto right_node = node[1];           \
		const auto r0 = compile(left_node, ctx);   \
		const auto r1 = compile(right_node, ctx);  \
		const auto res = make_register();          \
                                               \
		lir::Chunk chunk {};                       \
		chunk += r0.code;                          \
		chunk += r1.code;                          \
		chunk.emit(OPCODE, res, r0.opnd, r1.opnd); \
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

#undef BINARY_ARITH

		case NodeType::NOT: {
			lir::Chunk chunk {};
			lir::Operand res = make_register();
			auto inverse_res = compile(node[0], ctx);
			chunk = chunk + inverse_res.code;
			lir::Operand inverse = inverse_res.opnd;
			chunk.emit(Opcode::NOT, res, inverse);
			return {chunk, res};
		}
		case NodeType::AT: COMPILE_WITH_HANDLER(compile_at)
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
		case NodeType::MODE: assert(false);
		case NodeType::PARAM: assert(false);
		case NodeType::RECORD_LITERAL: {
			const auto field_count = node.size();
			lir::Chunk chunk {};
			const auto t1 = make_register();
			chunk.emit_alloca(t1, lir::Immediate(static_cast<int>(field_count)));
			for (auto i = 0ul; i < field_count; i++) {
				const auto& field_node = ast.at(node[i]);
				assert(field_node.type == NodeType::FIELD);
				const auto ri = compile(field_node[1], ctx);
				chunk += ri.code;
				chunk.emit_storea(ri.opnd, lir::Immediate(static_cast<int>(i)), t1);
			}
			chunk.result_opnd = t1;
			return {chunk, t1};
		}
		case NodeType::FIELD: assert(false);
		case NodeType::FIELD_ACCESS: assert(false);
		case NodeType::ARRAY_LITERAL: {
			//
			// r1 : v1
			// r2 : v2
			// ...
			// rn : vn
			//
			// t2 : *---> [*, n $]
			//             |
			//             +---> [r1, r2, ..., rn]
			//
			lir::Chunk chunk {};
			const auto t1 = make_register();
			chunk.emit_alloca(t1, lir::Immediate(static_cast<int>(node.size())));
			for (const auto& [i, elt_idx] : std::ranges::views::enumerate(node)) {
				const auto ri = compile(elt_idx, ctx);
				chunk += ri.code;
				chunk.emit_storea(ri.opnd, lir::Immediate(static_cast<int>(i)), t1);
			}
			const auto t2 = make_register();
			chunk.emit_alloca(t2, lir::Immediate(2));
			chunk.emit_storea(t1, lir::Immediate(0), t2);
			chunk.emit_storea(
				lir::Immediate(static_cast<int>(node.size())), lir::Immediate(1), t2
			);
			chunk.result_opnd = t2;
			return {chunk, t2};
		}
		case NodeType::REPEAT: {
			//
			// r0 : value
			// r1 : amnt $
			// t1 : *---> [*, amnt $]
			//             |
			//             +---> [value, value, ...] (amnt times)
			//
			const auto r0 = compile(node[0], ctx);
			const auto r1 = compile(node[1], ctx);
			const auto t1 = make_register();
			const auto t2 = make_register();
			lir::Chunk chunk {};
			chunk += r0.code;
			chunk += r1.code;
			chunk.emit(Opcode::REPEAT, t1, r0.opnd, r1.opnd);
			chunk.emit_alloca(t2, lir::Immediate(2));
			chunk.emit_storea(t1, lir::Immediate(0), t2);
			chunk.emit_storea(r1.opnd, lir::Immediate(1), t2);
			chunk.result_opnd = t2;
			return {chunk, t2};
		}
		case NodeType::LEN: {
			//
			// c0 : *---> [data *, len $]
			//                  |
			//                  +---> ...
			// t1 : len $
			//

			const auto c0 = compile(node[0], ctx);
			const auto t1 = make_register();
			lir::Chunk chunk {};
			chunk += c0.code;
			chunk.emit_loada(t1, lir::Immediate(1), c0.opnd);
			chunk.result_opnd = t1;
			return {chunk, t1};
		}
	}
	assert(false);
}

} // namespace compiler
