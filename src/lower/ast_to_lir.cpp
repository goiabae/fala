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
#include <stdexcept>
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

static NodeIndex decl_name(const Node& node) {
	if (node.type == NodeType::VAR_DECL) {
		return node[1];
	} else if (node.type == NodeType::FUN_DECL) {
		return node[0];
	} else if (node.type == NodeType::TYPE_DECL) {
		return node[0];
	} else if (node.type == NodeType::EFF_DECL) {
		return node[0];
	} else if (node.type == NodeType::EFF_ALIAS) {
		return node[0];
	} else {
		assert(false);
	}
}

namespace builtin {
static Result read_int(Compiler& comp, vector<lir::Operand> args);
static Result read_char(Compiler& comp, vector<lir::Operand> args);

struct Builtin {
	Result (*ptr)(Compiler& comp, vector<lir::Operand> args);
	std::string_view name;
};

constexpr auto builtins = {
	Builtin {read_int, "read_int"},
	Builtin {read_char, "read_char"},
};

Result read_int(Compiler& comp, vector<lir::Operand>) {
	lir::Operand tmp = comp.make_register();
	auto chunk = comp.make_chunk();
	chunk.emit(Opcode::READV, tmp);
	return {lir::Block::defer_last_jump(chunk.entry, chunk.index), tmp};
}

Result read_char(Compiler& comp, vector<lir::Operand>) {
	lir::Operand tmp = comp.make_register();
	auto chunk = comp.make_chunk();
	chunk.emit(Opcode::READC, tmp);
	return {lir::Block::defer_last_jump(chunk.entry, chunk.index), tmp};
}

} // namespace builtin

enum class PassingConvention {
	CLONE,
	ALIAS,
	INVALID,
};

static PassingConvention passing_convention_for(
	Mode::Concrete param_mode, Mode::Concrete arg_mode
) {
	if (param_mode == Mode::Concrete::OUT) {
		if (arg_mode == Mode::Concrete::OUT) {
			return PassingConvention::ALIAS;
		} else if (arg_mode == Mode::Concrete::VAL) {
			return PassingConvention::INVALID;
		} else if (arg_mode == Mode::Concrete::VAR) {
			return PassingConvention::ALIAS;
		} else {
			assert(false);
		}
	} else if (param_mode == Mode::Concrete::VAL) {
		if (arg_mode == Mode::Concrete::OUT) {
			return PassingConvention::CLONE;
		} else if (arg_mode == Mode::Concrete::VAL) {
			return PassingConvention::ALIAS;
		} else if (arg_mode == Mode::Concrete::VAR) {
			return PassingConvention::CLONE;
		} else {
			assert(false);
		}
	} else if (param_mode == Mode::Concrete::VAR) {
		if (arg_mode == Mode::Concrete::OUT) {
			return PassingConvention::CLONE;
		} else if (arg_mode == Mode::Concrete::VAL) {
			return PassingConvention::CLONE;
		} else if (arg_mode == Mode::Concrete::VAR) {
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

	const auto& root = ast.at(ast.root_index);
	if (root.type == NodeType::MODULE_BODY) {
		const SignalHandlers handlers {};
		const Context ctx {
			.handlers = handlers,
			.current_closure = {},
			.current_resumption = {},
		};

		auto chunk = make_chunk();
		auto start = make_chunk();
		auto write_char_handler = make_chunk();
		auto write_str_handler = make_chunk();
		auto write_int_handler = make_chunk();
		auto exit = make_chunk();
		auto module = compile(ast.root_index, ctx);
		auto start2 = make_chunk();

		const auto main = module.opnd;
		const auto r1 = make_register();
		const auto r2 = make_register();
		const auto r3 = make_register();
		const auto r4 = make_register();
		const auto r8 = make_register();
		const auto r9 = make_register();

		chunk.emit(Opcode::JMP, start.entry);
		chunk.finalize();

		start.emit(Opcode::JMP, module.block.entry);
		start.finalize();

		module.block.and_then(chunks, write_char_handler.entry);

		write_char_handler.emit(
			Opcode::CATCH, write_char_tag, write_str_handler.entry
		);
		write_char_handler.emit(Opcode::POP, r4);
		write_char_handler.emit(Opcode::POP, r8);
		write_char_handler.emit(Opcode::LOADA, r9, lir::Immediate(0), r8);
		write_char_handler.emit(Opcode::PRINTC, r9);
		write_char_handler.emit(Opcode::PUSH, lir::Immediate(0));
		write_char_handler.emit(Opcode::RESUME, r4);
		write_char_handler.emit(Opcode::JMP, exit.entry);
		write_char_handler.finalize();

		write_str_handler.emit(
			Opcode::CATCH, write_str_tag, write_int_handler.entry
		);
		write_str_handler.emit(Opcode::POP, r4);
		write_str_handler.emit(Opcode::POP, r8);
		write_str_handler.emit(Opcode::LOADA, r9, lir::Immediate(0), r8);
		write_str_handler.emit(Opcode::PRINTF, r8);
		write_str_handler.emit(Opcode::PUSH, lir::Immediate(0));
		write_str_handler.emit(Opcode::RESUME, r4);
		write_str_handler.emit(Opcode::JMP, exit.entry);
		write_str_handler.finalize();

		write_int_handler.emit(Opcode::CATCH, write_int_tag, start2.entry);
		write_int_handler.emit(Opcode::POP, r4);
		write_int_handler.emit(Opcode::POP, r8);
		write_int_handler.emit(Opcode::LOADA, r9, lir::Immediate(0), r8);
		write_int_handler.emit(lir::Opcode::PRINTV, r9);
		write_int_handler.emit(Opcode::PUSH, lir::Immediate(0));
		write_int_handler.emit(Opcode::RESUME, r4);
		write_int_handler.emit(Opcode::JMP, exit.entry);
		write_int_handler.finalize();

		start2.emit(Opcode::LOADA, r2, lir::Immediate(0), main);
		start2.emit(Opcode::ALLOCA, r3, lir::Immediate(1));
		start2.emit(Opcode::STOREA, lir::Immediate(0), lir::Immediate(0), r3);
		start2.emit(Opcode::PUSH, r3);
		start2.emit(Opcode::CALL, r2);
		start2.emit(Opcode::POP, r1);
		start2.emit(Opcode::JMP, exit.entry);
		start2.finalize();
	} else {
		const SignalHandlers handlers {};
		const Context ctx {
			.handlers = handlers,
			.current_closure = {},
			.current_resumption = {},
		};

		auto start = make_chunk();
		auto c0 = compile(ast.root_index, ctx);
		auto exit = make_chunk();

		start.emit(Opcode::JMP, c0.block.entry);
		start.finalize();

		c0.block.and_then(chunks, exit.entry);
	}

	lir::Chunk result {};
	for (const auto& c : chunks) result += c;
	return result;
}

lir::Register Compiler::make_register() { return lir::Register(reg_count++); }

lir::Label Compiler::make_label() { return lir::Label(label_count++); }

Result Compiler::compile_or_allocate_lvalue(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	if (node.type == NodeType::PATH) {
		return compile_lvalue(node_idx, ctx);
	} else {
		auto c0 = compile(node_idx, ctx);
		auto load_lvalue = make_chunk();
		const auto t1 = make_register();
		c0.block.and_then(chunks, load_lvalue.entry);
		load_lvalue.emit(Opcode::ALLOCA, t1, lir::Immediate(1));
		load_lvalue.emit(Opcode::STOREA, c0.opnd, lir::Immediate(0), t1);
		return {lir::Block::defer_last_jump(c0.block.entry, load_lvalue.index), t1};
	}
}

Result Compiler::compile_app(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto func_idx = node[0];
	const auto& func_node = ast.at(func_idx);
	const auto& args_node = ast.at(node[1]);

	assert(func_node.type == NodeType::GUID);
	assert(args_node.type == NodeType::METALIST);

	std::vector<lir::Block> arg_blocks {};
	std::vector<lir::Operand> args {};
	std::vector<std::shared_ptr<Type>> arg_tys {};
	for (const auto arg_idx : args_node) {
		const auto res = compile_or_allocate_lvalue(arg_idx, ctx);
		arg_blocks.push_back(res.block);
		auto ty = tpc.node_to_type.at(arg_idx);
		arg_tys.push_back(ty);
		args.push_back(res.opnd);
	}

	for (const auto& [a, b] : std::ranges::views::adjacent<2>(arg_blocks)) {
		a.and_then(chunks, b.entry);
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
			auto result = builtin.ptr(*this, args);
			if (arg_blocks.size() > 0) {
				arg_blocks.back().and_then(chunks, result.block.entry);
			}
			lir::Block blk {
				.entry =
					(arg_blocks.size() > 0) ? arg_blocks.at(0).entry : result.block.entry,
				.patches = result.block.patches,
			};
			return {blk, result.opnd};
		} else {
			throw std::runtime_error("unknown builtin");
		}
	} else if (tpc.constructors.contains(func_node.guid)) {
		const auto ty = tpc.node_to_type.at(func_idx);
		const auto dty = get_datatype(ty);
		const auto fty = erase_function_captures(dty);
		const auto name =
			std::string_view(pool.find(symtab.name_of(func_node.guid).value()));
		const auto unty = std::dynamic_pointer_cast<Union>(fty->output);
		assert(unty != nullptr);
		if (auto found = std::ranges::find(
					unty->constructors,
					std::string(name),
					[](const auto& x) { return x.name; }
				);
		    found != unty->constructors.end()) {
			auto index = std::distance(unty->constructors.begin(), found);
			const auto r0 = make_register();
			auto call = make_chunk();
			if (arg_blocks.size() > 0) {
				arg_blocks.back().and_then(chunks, call.entry);
			}
			call.emit(
				Opcode::ALLOCA,
				r0,
				lir::Immediate(static_cast<int>((*found).arguments.size()))
			);
			auto i = 0;
			for (const auto& arg : args) {
				const auto ri = make_register();
				call.emit(Opcode::LOADA, ri, lir::Immediate(0), arg);
				call.emit(Opcode::STOREA, ri, lir::Immediate(i), r0);
				i++;
			}
			const auto r1 = make_register();
			call.emit(Opcode::ALLOCA, r1, lir::Immediate(2));
			call.emit(
				Opcode::STOREA,
				lir::Immediate(static_cast<int>(index)),
				lir::Immediate(0),
				r1
			);
			call.emit(Opcode::STOREA, r0, lir::Immediate(1), r1);
			auto entry =
				(arg_blocks.size() > 0) ? arg_blocks.at(0).entry : call.entry;
			return {lir::Block::defer_last_jump(entry, call.index), r1};
		} else {
			logger.log(
				LogLevel::ERROR,
				"Constructor of union not found! Something went wrong with the "
				"typechecker"
			);
		}
		assert(false);
	} else {
		const auto ty = tpc.node_to_type.at(func_idx);
		const auto name =
			std::string_view(pool.find(symtab.name_of(func_node.guid).value()));
		logger.log(LogLevel::INFO, node.loc, "Applying {}", name);
		const auto dty = get_datatype(ty);
		const auto fty = erase_function_captures(dty);
		assert(fty != nullptr);
		auto call = make_chunk();
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
				call.emit(Opcode::PUSH, arg);
			} else if (convention == PassingConvention::CLONE) {
				const auto t = make_register();
				call.emit(Opcode::CLONEA, t, arg);
				call.emit(Opcode::PUSH, t);
			} else if (convention == PassingConvention::INVALID) {
				throw std::runtime_error("cannot call function. conflicting modes");
			} else {
				assert(false);
			}
		}

		auto func = compile(func_idx, ctx);

		call.emit(Opcode::JMP, func.block.entry);
		call.finalize();

		auto call2 = make_chunk();

		func.block.and_then(chunks, call2.entry);

		const auto res = make_register();
		call2.emit(Opcode::CALL, func.opnd);
		call2.emit(Opcode::POP, res);

		auto entry = (arg_blocks.size() > 0) ? arg_blocks.at(0).entry : call.entry;

		return {lir::Block::defer_last_jump(entry, call2.index), res};
	}
}

Result Compiler::compile_perform(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto func_idx = node[0];
	const auto& func_node = ast.at(func_idx);
	const auto& args_node = ast.at(node[1]);

	assert(func_node.type == NodeType::GUID);
	assert(args_node.type == NodeType::METALIST);

	const auto name = symtab.name_of(func_node.guid).value();
	const auto effect_name = std::string(pool.find(name));
	const auto effect = tpc.name_to_effect.at(func_node.guid);
	assert(effect != nullptr);
	const auto effect_tag = effect_tags.at(func_node.guid);

	std::vector<lir::Block> arg_blocks {};
	std::vector<lir::Operand> args {};
	std::vector<std::shared_ptr<Type>> arg_tys {};
	for (const auto arg_idx : args_node) {
		const auto res = compile_or_allocate_lvalue(arg_idx, ctx);
		arg_blocks.push_back(res.block);
		auto ty = tpc.node_to_type.at(arg_idx);
		arg_tys.push_back(ty);
		args.push_back(res.opnd);
	}

	for (const auto& [a, b] : std::ranges::views::adjacent<2>(arg_blocks)) {
		a.and_then(chunks, b.entry);
	}

	auto call = make_chunk();
	for (const auto& [i, arg] :
	     std::ranges::views::enumerate(std::ranges::reverse_view(args))) {
		const auto param_mode =
			(*(effect->inputs.at(static_cast<std::size_t>(i))->mode))
				.to_concrete()
				.value();
		const auto arg_mode =
			arg_tys.at(static_cast<std::size_t>(i))->mode->to_concrete().value();
		const auto convention = passing_convention_for(param_mode, arg_mode);

		if (convention == PassingConvention::ALIAS) {
			call.emit(Opcode::PUSH, arg);
		} else if (convention == PassingConvention::CLONE) {
			const auto t = make_register();
			call.emit(Opcode::CLONEA, t, arg);
			call.emit(Opcode::PUSH, t);
		} else if (convention == PassingConvention::INVALID) {
			throw std::runtime_error("cannot call function. conflicting modes");
		} else {
			assert(false);
		}
	}

	call.emit(
		Opcode::THROW, effect_tag, lir::Immediate(static_cast<int>(args.size()))
	);
	const auto res = make_register();
	call.emit(Opcode::POP, res);

	auto entry = (arg_blocks.size() > 0) ? arg_blocks.at(0).entry : call.entry;

	return {lir::Block::defer_last_jump(entry, call.index), res};
}

Result Compiler::compile_if(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto cond_idx = node[0];
	const auto then_idx = node[1];
	const auto else_idx = node[2];
	const auto res = make_register();

	// c0 ---> c3 ---> c2 ---> c5
	//         |               |
	//         v               v
	//         c1 ---> c4 ---> *

	auto c0 = compile(cond_idx, ctx);
	auto c1 = compile(then_idx, ctx);
	auto c2 = compile(else_idx, ctx);
	auto c3 = make_chunk();
	auto c4 = make_chunk();
	auto c5 = make_chunk();

	c0.block.and_then(chunks, c3.entry);
	c1.block.and_then(chunks, c4.entry);
	c2.block.and_then(chunks, c5.entry);

	c3.emit(Opcode::JMP_FALSE, c0.opnd, c2.block.entry);
	c3.emit(Opcode::JMP, c1.block.entry);
	c3.finalize();

	c4.emit(Opcode::MOV, res, c1.opnd);

	c5.emit(Opcode::MOV, res, c2.opnd);

	lir::Block blk {
		.entry = c0.block.entry,
		.patches = {
			lir::AppendFinalJump {
				.block_id = c4.index,
			},
			lir::AppendFinalJump {
				.block_id = c5.index,
			},
		},
	};

	return {blk, res};
}

Result Compiler::compile_for(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);

	const auto decl_idx = node[0];
	const auto to_idx = node[1];
	const auto step_idx = node[2];
	const auto then_idx = node[3];

	const auto& step_node = ast.at(step_idx);

	const auto cmp = make_register();
	const auto result = make_register();

	auto cend = make_chunk();
	auto cinc = make_chunk();

	SignalHandlers new_handlers {
		{cinc.entry, result},
		{cend.entry, result},
		ctx.handlers.return_handler,
		true,
		true,
		ctx.handlers.has_return_handler
	};

	Context header_ctx {ctx};
	Context body_ctx {
		.handlers = new_handlers,
		.current_closure = ctx.current_closure,
		.current_resumption = ctx.current_resumption,
	};

	const auto t1 = make_register();
	const auto t2 = make_register();

	auto c0 = [&]() -> Result {
		if (step_node.type != NodeType::EMPTY) {
			return compile(step_idx, ctx);
		} else {
			auto chunk = make_chunk();
			return {
				lir::Block::defer_last_jump(chunk.entry, chunk.index), lir::Immediate(1)
			};
		}
	}();

	auto c1 = compile(decl_idx, header_ctx);
	auto c2 = compile(to_idx, header_ctx);
	auto c3 = compile(then_idx, body_ctx);

	auto cbeg = make_chunk();
	auto cmov = make_chunk();

	c0.block.and_then(chunks, c1.block.entry);
	c1.block.and_then(chunks, c2.block.entry);
	c2.block.and_then(chunks, cbeg.entry);
	c3.block.and_then(chunks, cmov.entry);

	cbeg.emit(lir::Opcode::LOADA, t1, lir::Immediate(0), c1.opnd);
	cbeg.emit(Opcode::EQ, cmp, t1, c2.opnd);
	cbeg.emit(Opcode::JMP_FALSE, cmp, c3.block.entry);
	cbeg.emit(lir::Opcode::JMP, cend.entry);
	cbeg.finalize();

	cmov.emit(Opcode::MOV, result, c3.opnd);
	cmov.emit(lir::Opcode::JMP, cinc.entry);
	cmov.finalize();

	cinc.emit(Opcode::ADD, t2, t1, c0.opnd);
	cinc.emit(lir::Opcode::STOREA, t2, lir::Immediate(0), c1.opnd);
	cinc.emit(lir::Opcode::JMP, cbeg.entry);
	cinc.finalize();

	//                      cinc <-- cmov
	//                       |       ^
	//                       v       |
	// c0 --> c1 --> c2 --> cbeg --> c3
	//                       |
	//                       v
	//                      cend --> *

	lir::Block blk {
		.entry = c0.block.entry,
		.patches = {lir::AppendFinalJump {.block_id = cend.index}}
	};

	return {blk, result};
}

Result Compiler::compile_when(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto cond_idx = node[0];
	const auto then_idx = node[1];
	const auto r = make_register();
	auto c0 = compile(cond_idx, ctx);
	auto c1 = compile(then_idx, ctx);
	auto c2 = make_chunk();
	auto c3 = make_chunk();

	c0.block.and_then(chunks, c2.entry);

	c2.emit(Opcode::MOV, r, {});
	c2.emit(Opcode::JMP_TRUE, c0.opnd, c1.block.entry);

	c1.block.and_then(chunks, c3.entry);

	c3.emit(Opcode::MOV, r, c1.opnd);

	// c0 --> c2 --> c1
	//        |      |
	//        v      v
	//        * <--  c3

	lir::Block blk {
		.entry = c0.block.entry,
		.patches = {
			lir::AppendFinalJump {.block_id = c2.index},
			lir::AppendFinalJump {.block_id = c3.index},
		}
	};

	return {blk, r};
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
		.current_closure = ctx.current_closure,
		.current_resumption = ctx.current_resumption,
	};

	auto c0 = compile(node[0], ctx);
	auto c1 = compile(node[1], body_ctx);
	auto c2 = make_chunk();
	auto c3 = make_chunk();

	c0.block.and_then(chunks, c2.entry);

	c2.emit(Opcode::JMP_TRUE, c0.opnd, c1.block.entry);

	c1.block.and_then(chunks, c3.entry);

	c3.emit(Opcode::MOV, result_register, c1.opnd);
	c3.emit(Opcode::JMP, c0.block.entry);
	c3.finalize();

	lir::Block blk {
		.entry = c0.block.entry,
		.patches = {lir::AppendFinalJump {.block_id = c2.index}},
	};

	return {blk, result_register};
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
		case NodeType::AT: {
			//                   +---> [... v]
			//                   |
			// c0 : *---> *---> [*, n $]
			// r0 : *---> v
			//
			const auto& node = ast.at(node_idx);
			auto c0 = compile_lvalue(node[0], ctx);
			auto c1 = compile(node[1], ctx);
			auto c2 = make_chunk();
			const auto r0 = make_register();
			const auto r1 = make_register();
			const auto r2 = make_register();

			c0.block.and_then(chunks, c1.block.entry);
			c1.block.and_then(chunks, c2.entry);

			c2.emit(lir::Opcode::LOADA, r1, lir::Immediate(0), c0.opnd);
			c2.emit(lir::Opcode::LOADA, r2, lir::Immediate(0), r1);
			c2.emit(lir::Opcode::SHIFTA, r0, c1.opnd, r2);

			return {lir::Block::defer_last_jump(c0.block.entry, c2.index), r0};
		}
		case NodeType::PATH: return compile_lvalue(node[0], ctx);
		case NodeType::GUID: {
			const auto maybe_rel = symtab.relative_scope_of(node.guid, node_idx);
			assert(maybe_rel.has_value());
			const auto rel = maybe_rel.value();
			const auto name =
				std::string_view(pool.find(symtab.name_of(node.guid).value()));
			if (rel == RelativeScope::FREE) {
				assert(ctx.current_closure.has_value());
				const auto& clinfo = ctx.current_closure.value();
				const auto t1 = make_register();
				if (not clinfo.offsets.contains(node.guid)) {
					std::string s {};
					s += "[";
					for (const auto& [a, b] : clinfo.offsets) {
						s += std::string_view(pool.find(symtab.name_of(a).value()));
						s += " ";
					}
					s += "]";
					const auto function_name = std::string_view(
						pool.find(symtab.name_of(clinfo.function_name.value()).value())
					);
					throw std::runtime_error(
						std::format(
							"{}: symbol for free variable \"{}\" not in closure info {} of "
							"function \"{}\"",
							node.loc.begin.line + 1,
							name,
							s,
							function_name
						)
					);
				}
				logger.log(
					LogLevel::INFO,
					"Variable \"{}\" (guid={}) is free",
					name,
					node.guid.index
				);

				auto chunk = make_chunk();
				chunk.emit(
					lir::Opcode::SHIFTA,
					t1,
					lir::Immediate(static_cast<int>(clinfo.offsets.at(node.guid))),
					clinfo.object
				);

				return {lir::Block::defer_last_jump(chunk.entry, chunk.index), t1};
			} else {
				logger.log(
					LogLevel::INFO,
					"Variable \"{}\" (guid={}) is not free",
					name,
					node.guid.index
				);
				assert(variables.contains(node.guid));
				auto chunk = make_chunk();
				return {
					lir::Block::defer_last_jump(chunk.entry, chunk.index),
					variables.at(node.guid)
				};
			}
		}
		case NodeType::FIELD_ACCESS: {
			auto c0 = compile_lvalue(node[0], ctx);
			auto c1 = make_chunk();
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
			c0.block.and_then(chunks, c1.entry);
			c1.emit(lir::Opcode::LOADA, t3, lir::Immediate(0), c0.opnd);
			c1.emit(lir::Opcode::SHIFTA, t1, lir::Immediate(static_cast<int>(d)), t3);
			return {lir::Block::defer_last_jump(c0.block.entry, c1.index), t1};
		}
		default: {
			throw std::runtime_error(
				std::format("unhandled node type: {}", node_type_repr(node.type))
			);
		}
	}
}

lir::Block Compiler::compile_case(
	NodeIndex node_idx, Context ctx, lir::Label exit, lir::Register res,
	std::shared_ptr<Datatype> expr_type, lir::Operand expr
) {
	const auto& node = ast.at(node_idx);
	assert(node.type == NodeType::CASE);
	auto c3 = make_chunk();
	auto c0 = compile_pattern_match(node[0], ctx, c3.entry, expr_type, expr);
	auto c1 = compile(node[1], ctx);
	auto c2 = make_chunk();
	c0.and_then(chunks, c1.block.entry);
	c1.block.and_then(chunks, c2.entry);
	c2.emit(lir::Opcode::MOV, res, c1.opnd);
	c2.emit(lir::Opcode::JMP, exit);
	return lir::Block::defer_last_jump(c0.entry, c3.index);
}

lir::Block Compiler::compile_pattern_match(
	NodeIndex node_idx, Context ctx, lir::Label if_fail,
	std::shared_ptr<Datatype> expr_type, lir::Operand expr
) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::CTOR_PATTERN: {
			const auto& name_node = ast.at(node[0]);
			assert(name_node.type == NodeType::GUID);
			const auto ctor_name =
				std::string(pool.find(symtab.name_of(name_node.guid).value()));
			const auto union_ty =
				std::dynamic_pointer_cast<Union>(get_datatype(expr_type));
			assert(union_ty != nullptr);
			if (auto found = std::ranges::find(
						union_ty->constructors,
						ctor_name,
						[](const auto& x) { return x.name; }
					);
			    found != union_ty->constructors.end()) {
				auto tag = std::distance(union_ty->constructors.begin(), found);
				const auto r0 = make_register();
				const auto r1 = make_register();
				const auto r2 = make_register();
				auto c0 = make_chunk();
				c0.emit(lir::Opcode::LOADA, r0, lir::Immediate(0), expr);
				c0.emit(Opcode::EQ, r1, r0, lir::Immediate(static_cast<int>(tag)));
				c0.emit(Opcode::JMP_FALSE, r1, if_fail);
				c0.emit(lir::Opcode::LOADA, r2, lir::Immediate(1), expr);
				std::vector<lir::Block> blocks {lir::Block {
					.entry = c0.entry,
					.patches = {lir::AppendFinalJump {.block_id = c0.index}}
				}};
				const auto& arguments_node = ast.at(node[1]);
				for (auto i = 0ul; i < arguments_node.size(); i++) {
					const auto ri = make_register();
					auto ca = make_chunk();
					const auto ci = compile_pattern_match(
						arguments_node[i], ctx, if_fail, (*found).arguments.at(i), ri
					);
					ca.emit(
						lir::Opcode::LOADA, ri, lir::Immediate(static_cast<int>(i)), r2
					);
					ca.emit(lir::Opcode::JMP, ci.entry);
					ca.finalize();
					lir::Block blk {
						.entry = ca.entry,
						.patches = ci.patches,
					};
					blocks.push_back(blk);
				}
				for (const auto& [a, b] : std::ranges::views::adjacent<2>(blocks)) {
					a.and_then(chunks, b.entry);
				}
				lir::Block blk {
					.entry = c0.entry,
					.patches = blocks.back().patches,
				};
				return blk;
			} else {
				throw std::runtime_error(
					"pattern match type does not contain constructor"
				);
			};
		}
		case NodeType::GUID: {
			const auto r0 = variables.at(node.guid);
			auto chunk = make_chunk();
			chunk.emit(lir::Opcode::ALLOCA, r0, lir::Immediate(1));
			chunk.emit(lir::Opcode::STOREA, expr, lir::Immediate(0), r0);
			return lir::Block::defer_last_jump(chunk.entry, chunk.index);
		}
		default: {
			throw std::runtime_error("unhandled pattern match");
		}
	}
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

	const auto var = variables.at(id_node.guid);

	auto c = [&]() {
		if (exp_node.type == NodeType::PATH) {
			auto c0 = compile_lvalue(exp_idx, ctx);
			auto c1 = make_chunk();
			c0.block.and_then(chunks, c1.entry);
			c1.emit(lir::Opcode::CLONEA, var, c0.opnd);
			return lir::Block::defer_last_jump(c0.block.entry, c1.index);
		} else {
			auto c0 = compile(exp_idx, ctx);
			auto c1 = make_chunk();
			c0.block.and_then(chunks, c1.entry);
			c1.emit(lir::Opcode::ALLOCA, var, lir::Immediate(1));
			c1.emit(lir::Opcode::STOREA, c0.opnd, lir::Immediate(0), var);
			return lir::Block::defer_last_jump(c0.block.entry, c1.index);
		}
	}();

	logger.log(
		LogLevel::INFO,
		node.loc,
		"Variable \"{}\" compiled into register {}",
		name,
		var
	);

	return {c, var};
}

void Compiler::allocate_variables() {
	const auto syms = symtab.all_symbols(symtab.value_namespace);
	for (const auto& sym : syms) {
		const auto scope = symtab.declaration_scope_of(sym).value();
		if (scope != symtab.builtin_scope) {
			variables[sym] = make_register();
		}
	}
	auto i = 0ul;
	for (const auto& [name, eff] : tpc.name_to_effect) {
		effect_tags[name] = lir::Immediate(static_cast<int>(i++));
		const auto x = std::string_view(pool.find(symtab.name_of(name).value()));
		if (x == "write_str") {
			write_str_tag = effect_tags.at(name);
		} else if (x == "write_int") {
			write_int_tag = effect_tags.at(name);
		} else if (x == "write_char") {
			write_char_tag = effect_tags.at(name);
		}
	}
}

Result Compiler::compile_fun_decl(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto id_idx = node[0];
	const auto params_idx = node[1];
	const auto body_idx = node[4];
	const auto& id_node = ast.at(id_idx);
	const auto& params_node = ast.at(params_idx);
	assert(id_node.type == NodeType::GUID);

	auto c0 = make_chunk();
	const auto t2 = variables.at(id_node.guid);
	const auto t3 = make_register();

	const auto ty = tpc.variables.at(id_node.guid);
	const auto dty = ty->scheme->body;
	const auto cty = std::dynamic_pointer_cast<Closure>(dty);
	assert(cty != nullptr);

	auto chunk = make_chunk();

	const auto t1 = make_register();
	chunk.emit(
		lir::Opcode::ALLOCA,
		t1,
		lir::Immediate(static_cast<int>(cty->captures.size()))
	);
	ClosureInfo clinfo {
		.function_name = id_node.guid,
		.object = t3,
		.offsets = {},
	};
	{
		auto i = 0ul;
		for (const auto& [a, b] : cty->captures) {
			const auto ti = make_register();
			const auto ti2 = make_register();
			chunk.emit(lir::Opcode::LOADA, ti2, lir::Immediate(0), variables.at(a));
			chunk.emit(lir::Opcode::CLONEA, ti, ti2);
			chunk.emit(
				lir::Opcode::STOREA, ti, lir::Immediate(static_cast<int>(i)), t1
			);
			clinfo.offsets[a] = i;
			i++;
		}
	}
	const auto t4 = make_register();
	chunk.emit(lir::Opcode::ENCLOSE, t4, c0.entry, t1);
	chunk.emit(lir::Opcode::ALLOCA, t2, lir::Immediate(1));
	chunk.emit(lir::Opcode::STOREA, t4, lir::Immediate(0), t2);

	const Context body_ctx {
		.handlers = ctx.handlers,
		.current_closure = clinfo,
		.current_resumption = ctx.current_resumption,
	};

	auto r0 = compile(body_idx, body_ctx);

	{
		c0.emit(Opcode::FUNC);
		c0.emit(Opcode::POP, t3);
		for (auto param_idx : params_node) {
			const auto& param_node = ast.at(param_idx);
			assert(param_node.type == NodeType::PARAM);
			const auto& param_name = ast.at(param_node[1]);
			assert(param_name.type == NodeType::GUID);
			const auto arg = variables.at(param_name.guid);
			c0.emit(Opcode::POP, arg);
		}
		c0.emit(lir::Opcode::JMP, r0.block.entry);
		c0.finalize();

		auto c1 = make_chunk();

		r0.block.and_then(chunks, c1.entry);

		c1.emit(Opcode::PUSH, r0.opnd);
		c1.emit(Opcode::RET);
		c1.finalize();
	}

	const auto name =
		std::string_view(pool.find(symtab.name_of(id_node.guid).value()));
	logger.log(
		LogLevel::INFO, node.loc, "Compiled {} into entry point {}", name, c0.entry
	);

	return {lir::Block::defer_last_jump(chunk.entry, chunk.index), t2};
}

Result Compiler::compile_ass(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	auto r0 = compile_lvalue(node[0], ctx);
	auto r1 = compile(node[1], ctx);
	auto r2 = make_chunk();

	r0.block.and_then(chunks, r1.block.entry);
	r1.block.and_then(chunks, r2.entry);
	r2.emit(lir::Opcode::STOREA, r1.opnd, lir::Immediate(0), r0.opnd);

	return {lir::Block::defer_last_jump(r0.block.entry, r2.index), r1.opnd};
}

Result Compiler::compile_let(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto decls_idx = node[0];
	const auto exp_idx = node[1];
	const auto& decls_node = ast.at(decls_idx);
	Context exp_ctx {ctx};
	auto r0 = compile(exp_idx, exp_ctx);
	auto exit = make_chunk();

	std::vector<lir::Block> blocks {};
	for (const auto decl_idx : decls_node) {
		const auto& decl = ast.at(decl_idx);
		if (decl.type == NodeType::HANDLER_DECL) {
			const auto ri = compile_handler_decl(decl_idx, exp_ctx, exit.entry);
			blocks.push_back(ri);
		} else {
			const auto ri = compile(decl_idx, exp_ctx);
			blocks.push_back(ri.block);
		}
	}
	blocks.push_back(r0.block);
	for (const auto& [a, b] : std::ranges::views::adjacent<2>(blocks)) {
		a.and_then(chunks, b.entry);
	}
	blocks.back().and_then(chunks, exit.entry);

	return {
		lir::Block::defer_last_jump(blocks.front().entry, exit.index), r0.opnd
	};
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

	auto chunk = make_chunk();
	chunk.emit(lir::Opcode::ALLOCA, t1, lir::Immediate((int)str.size()));
	chunk.emit(Opcode::INITA, t1, lir::ByteString {std::string(str)});
	chunk.emit(lir::Opcode::ALLOCA, t2, lir::Immediate(2));
	chunk.emit(lir::Opcode::STOREA, t1, lir::Immediate(0), t2);
	chunk.emit(
		lir::Opcode::STOREA,
		lir::Immediate(static_cast<int>(str.size())),
		lir::Immediate(1),
		t2
	);

	return {lir::Block::defer_last_jump(chunk.entry, chunk.index), t2};
}

Result Compiler::compile_at(NodeIndex node_idx, Context ctx) {
	//
	// res : *---> value
	// tmp : value
	//
	auto res = compile_lvalue(node_idx, ctx);
	auto c1 = make_chunk();
	auto res_opnd = res.opnd;
	auto tmp = make_register();
	res.block.and_then(chunks, c1.entry);
	c1.emit(lir::Opcode::LOADA, tmp, lir::Immediate(0), res_opnd);
	return {lir::Block::defer_last_jump(res.block.entry, c1.index), tmp};
}

Result Compiler::compile_block(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	lir::Chunk chunk {};
	const auto entry = make_label();
	chunk.add_label(entry);
	lir::Operand opnd;
	const Context inner_ctx {
		.handlers = ctx.handlers,
		.current_closure = ctx.current_closure,
		.current_resumption = ctx.current_resumption,
	};
	std::vector<lir::Block> blocks {};
	for (const auto stmt_idx : node) {
		const auto ri = compile(stmt_idx, inner_ctx);
		blocks.push_back(ri.block);
		opnd = ri.opnd;
	}
	for (const auto& [a, b] : std::ranges::views::adjacent<2>(blocks)) {
		a.and_then(chunks, b.entry);
	}
	assert(blocks.size() >= 1);
	lir::Block blk {
		.entry = blocks.front().entry,
		.patches = blocks.back().patches,
	};
	return {blk, opnd};
}

Result Compiler::compile_break(NodeIndex node_idx, Context ctx) {
	assert(ctx.handlers.has_break_handler);
	const auto res = ctx.handlers.break_handler.result_register;
	const auto dest = ctx.handlers.break_handler.destination_label;
	const auto& node = ast.at(node_idx);
	assert(node.branch.children_count == 1);
	auto r0 = compile(node[0], ctx);
	auto c1 = make_chunk();
	r0.block.and_then(chunks, c1.entry);
	c1.emit(Opcode::MOV, res, r0.opnd);
	c1.emit(Opcode::JMP, dest);
	return {lir::Block::defer_last_jump(r0.block.entry, c1.index), {}};
}

Result Compiler::compile_continue(NodeIndex node_idx, Context ctx) {
	assert(ctx.handlers.has_continue_handler);
	const auto res = ctx.handlers.continue_handler.result_register;
	const auto dest = ctx.handlers.continue_handler.destination_label;
	const auto& node = ast.at(node_idx);
	assert(node.branch.children_count == 1);
	auto r0 = compile(node[0], ctx);
	auto c1 = make_chunk();
	r0.block.and_then(chunks, c1.entry);
	c1.emit(Opcode::MOV, res, r0.opnd);
	c1.emit(Opcode::JMP, dest);
	return {lir::Block::defer_last_jump(r0.block.entry, c1.index), {}};
}

Result Compiler::compile(NodeIndex node_idx, Context ctx) {
#define COMPILE_WITH_HANDLER(METH) return METH(node_idx, ctx);
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::APP: COMPILE_WITH_HANDLER(compile_app)
		case NodeType::NUM: {
			auto chunk = make_chunk();
			auto opnd = lir::Immediate(node.num);
			return {lir::Block::defer_last_jump(chunk.entry, chunk.index), opnd};
		}
		case NodeType::BLK: COMPILE_WITH_HANDLER(compile_block)
		case NodeType::IF: COMPILE_WITH_HANDLER(compile_if)
		case NodeType::WHEN: COMPILE_WITH_HANDLER(compile_when)
		case NodeType::FOR: COMPILE_WITH_HANDLER(compile_for)
		case NodeType::WHILE: COMPILE_WITH_HANDLER(compile_while)
		case NodeType::BREAK: COMPILE_WITH_HANDLER(compile_break)
		case NodeType::CONTINUE: COMPILE_WITH_HANDLER(compile_continue)
		case NodeType::ASS: COMPILE_WITH_HANDLER(compile_ass)

#define BINARY_ARITH(OPCODE)                                             \
	{                                                                      \
		const auto left_node = node[0];                                      \
		const auto right_node = node[1];                                     \
		auto r0 = compile(left_node, ctx);                                   \
		auto r1 = compile(right_node, ctx);                                  \
		auto c2 = make_chunk();                                              \
		const auto res = make_register();                                    \
                                                                         \
		r0.block.and_then(chunks, r1.block.entry);                           \
		r1.block.and_then(chunks, c2.entry);                                 \
		c2.emit(OPCODE, res, r0.opnd, r1.opnd);                              \
		return {lir::Block::defer_last_jump(r0.block.entry, c2.index), res}; \
	}

		case NodeType::OR: {
			const auto left_node = node[0];
			const auto right_node = node[1];
			auto r0 = compile(left_node, ctx);
			auto r1 = compile(right_node, ctx);
			auto c2 = make_chunk();
			auto c3 = make_chunk();
			const auto res = make_register();

			r0.block.and_then(chunks, c2.entry);

			c2.emit(lir::Opcode::MOV, res, r0.opnd);
			c2.emit(Opcode::JMP_FALSE, res, r1.block.entry);

			r1.block.and_then(chunks, c3.entry);

			c3.emit(lir::Opcode::MOV, res, r1.opnd);

			lir::Block blk {
				.entry = r0.block.entry,
				.patches = {
					lir::AppendFinalJump {.block_id = c2.index},
					lir::AppendFinalJump {.block_id = c3.index},
				},
			};

			return {blk, res};
		}
		case NodeType::AND: {
			const auto left_node = node[0];
			const auto right_node = node[1];
			auto r0 = compile(left_node, ctx);
			auto r1 = compile(right_node, ctx);
			auto c2 = make_chunk();
			auto c3 = make_chunk();
			const auto res = make_register();

			r0.block.and_then(chunks, c2.entry);

			c2.emit(lir::Opcode::MOV, res, r0.opnd);
			c2.emit(Opcode::JMP_TRUE, res, r1.block.entry);

			r1.block.and_then(chunks, c3.entry);

			c3.emit(lir::Opcode::MOV, res, r1.opnd);

			lir::Block blk {
				.entry = r0.block.entry,
				.patches = {
					lir::AppendFinalJump {.block_id = c2.index},
					lir::AppendFinalJump {.block_id = c3.index},
				},
			};

			return {blk, res};
		}
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
			const auto entry = make_label();
			chunk.add_label(entry);
			lir::Operand res = make_register();
			auto inverse_res = compile(node[0], ctx);
			auto c1 = make_chunk();
			inverse_res.block.and_then(chunks, c1.entry);
			lir::Operand inverse = inverse_res.opnd;
			c1.emit(Opcode::NOT, res, inverse);
			return {
				lir::Block::defer_last_jump(inverse_res.block.entry, c1.index), res
			};
		}
		case NodeType::AT: COMPILE_WITH_HANDLER(compile_at)
		case NodeType::STR: COMPILE_WITH_HANDLER(compile_str)
		case NodeType::VAR_DECL: COMPILE_WITH_HANDLER(compile_var_decl)
		case NodeType::FUN_DECL: COMPILE_WITH_HANDLER(compile_fun_decl)
		case NodeType::NIL: return compile_constant(lir::Immediate(0));
		case NodeType::TRUE: return compile_constant(lir::Immediate(1));
		case NodeType::FALSE: return compile_constant(lir::Immediate(0));
		case NodeType::LET: COMPILE_WITH_HANDLER(compile_let)
		case NodeType::CHAR: {
			return compile_constant(lir::Immediate(node.character));
		}
		case NodeType::PATH: {
			auto res = compile_lvalue(node_idx, ctx);
			auto c1 = make_chunk();
			res.block.and_then(chunks, c1.entry);
			auto res_opnd = res.opnd;
			auto tmp = make_register();
			c1.emit(lir::Opcode::LOADA, tmp, lir::Immediate(0), res_opnd);
			return {lir::Block::defer_last_jump(res.block.entry, c1.index), tmp};
		}
		case NodeType::AS: return compile(node[0], ctx);
		case NodeType::GUID: {
			auto res = compile_lvalue(node_idx, ctx);
			auto c1 = make_chunk();
			res.block.and_then(chunks, c1.entry);
			auto res_opnd = res.opnd;
			auto tmp = make_register();
			c1.emit(lir::Opcode::LOADA, tmp, lir::Immediate(0), res_opnd);
			return {lir::Block::defer_last_jump(res.block.entry, c1.index), tmp};
		}
		case NodeType::RECORD_LITERAL: COMPILE_WITH_HANDLER(compile_record_literal)
		case NodeType::ARRAY_LITERAL: COMPILE_WITH_HANDLER(compile_array_literal)
		case NodeType::REPEAT: {
			//
			// r0 : value
			// r1 : amnt $
			// t1 : *---> [*, amnt $]
			//             |
			//             +---> [value, value, ...] (amnt times)
			//
			auto r0 = compile(node[0], ctx);
			auto r1 = compile(node[1], ctx);
			auto c2 = make_chunk();
			const auto t1 = make_register();
			const auto t2 = make_register();
			r0.block.and_then(chunks, r1.block.entry);
			r1.block.and_then(chunks, c2.entry);
			c2.emit(Opcode::REPEAT, t1, r0.opnd, r1.opnd);
			c2.emit(lir::Opcode::ALLOCA, t2, lir::Immediate(2));
			c2.emit(lir::Opcode::STOREA, t1, lir::Immediate(0), t2);
			c2.emit(lir::Opcode::STOREA, r1.opnd, lir::Immediate(1), t2);
			return {lir::Block::defer_last_jump(r0.block.entry, c2.index), t2};
		}
		case NodeType::LEN: {
			//
			// c0 : *---> [data *, len $]
			//                  |
			//                  +---> ...
			// t1 : len $
			//

			auto c0 = compile(node[0], ctx);
			auto c1 = make_chunk();
			const auto t1 = make_register();
			c0.block.and_then(chunks, c1.entry);
			c1.emit(lir::Opcode::LOADA, t1, lir::Immediate(1), c0.opnd);
			return {lir::Block::defer_last_jump(c0.block.entry, c1.index), t1};
		}
		case NodeType::MODULE_BODY: {
			const auto& decl_list = ast.at(node[0]);
			assert(decl_list.type == NodeType::METALIST);
			lir::Operand main {};
			std::vector<lir::Block> blocks {};
			for (auto decl : decl_list) {
				const auto& decl_node = ast.at(decl);
				if (decl_node.type != NodeType::TYPE_DECL
				    and decl_node.type != NodeType::EFF_DECL
				    and decl_node.type != NodeType::EFF_ALIAS) {
					const auto& name = ast.at(decl_name(decl_node));
					assert(name.type == NodeType::GUID);
					const auto name2 =
						std::string_view(pool.find(symtab.name_of(name.guid).value()));
					const auto ri = compile(decl, ctx);
					blocks.push_back(ri.block);
					if (name2 == "main") main = ri.opnd;
				}
			}
			for (const auto& [a, b] : std::ranges::views::adjacent<2>(blocks)) {
				a.and_then(chunks, b.entry);
			}
			lir::Block blk {
				.entry = blocks.at(0).entry,
				.patches = blocks.back().patches,
			};
			return {blk, main};
		};
		case NodeType::PERFORM: COMPILE_WITH_HANDLER(compile_perform)
		case NodeType::RESUME: {
			auto c0 = compile(node[0], ctx);
			auto c1 = make_chunk();
			c0.block.and_then(chunks, c1.entry);
			c1.emit(Opcode::PUSH, c0.opnd);
			c1.emit(Opcode::RESUME, ctx.current_resumption.value());
			return {lir::Block::defer_last_jump(c0.block.entry, c1.index), {}};
		}
		case NodeType::MATCH: COMPILE_WITH_HANDLER(compile_match)
		default: {
			throw std::runtime_error(
				std::format("unhandled not type: {}", node_type_repr(node.type))
			);
		}
	}
	assert(false);
}

Result Compiler::compile_array_literal(NodeIndex node_idx, Context ctx) {
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
	const auto& node = ast.at(node_idx);
	const auto t1 = make_register();
	auto c0 = make_chunk();
	c0.emit(
		lir::Opcode::ALLOCA, t1, lir::Immediate(static_cast<int>(node.size()))
	);
	std::vector<lir::Block> blocks {};
	for (const auto& [i, elt_idx] : std::ranges::views::enumerate(node)) {
		auto ri = compile(elt_idx, ctx);
		auto c2 = make_chunk();
		ri.block.and_then(chunks, c2.entry);
		c2.emit(
			lir::Opcode::STOREA, ri.opnd, lir::Immediate(static_cast<int>(i)), t1
		);
		blocks.push_back(lir::Block::defer_last_jump(ri.block.entry, c2.index));
	}
	c0.emit(lir::Opcode::JMP, blocks.at(0).entry);
	c0.finalize();
	for (const auto& [a, b] : std::ranges::views::adjacent<2>(blocks)) {
		a.and_then(chunks, b.entry);
	}
	const auto t2 = make_register();
	auto c3 = make_chunk();
	blocks.back().and_then(chunks, c3.entry);
	c3.emit(lir::Opcode::ALLOCA, t2, lir::Immediate(2));
	c3.emit(lir::Opcode::STOREA, t1, lir::Immediate(0), t2);
	c3.emit(
		lir::Opcode::STOREA,
		lir::Immediate(static_cast<int>(node.size())),
		lir::Immediate(1),
		t2
	);
	return {lir::Block::defer_last_jump(c0.entry, c3.index), t2};
}

Result Compiler::compile_record_literal(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto field_count = node.size();
	const auto record_datatype =
		std::dynamic_pointer_cast<Record>(tpc.node_to_type.at(node_idx)->datatype);
	const auto t1 = make_register();
	auto c0 = make_chunk();
	c0.emit(
		lir::Opcode::ALLOCA, t1, lir::Immediate(static_cast<int>(field_count))
	);
	std::vector<lir::Block> blocks {};
	for (auto i = 0ul; i < field_count; i++) {
		const auto& field_node = ast.at(node[i]);
		assert(field_node.type == NodeType::FIELD);
		const auto& field_name_node = ast.at(field_node[0]);
		assert(field_name_node.type == NodeType::ID);
		const auto field_name = std::string(pool.find(field_name_node.str_id));
		const auto found = std::ranges::find(
			record_datatype->fields, field_name, [](const auto& x) { return x.first; }
		);
		assert(found != record_datatype->fields.end());
		const auto offset = std::distance(record_datatype->fields.begin(), found);
		auto ri = compile(field_node[1], ctx);
		auto c2 = make_chunk();
		ri.block.and_then(chunks, c2.entry);
		c2.emit(
			lir::Opcode::STOREA, ri.opnd, lir::Immediate(static_cast<int>(offset)), t1
		);
		blocks.push_back(lir::Block::defer_last_jump(ri.block.entry, c2.index));
	}
	c0.emit(lir::Opcode::JMP, blocks.at(0).entry);
	c0.finalize();
	for (const auto& [a, b] : std::ranges::views::adjacent<2>(blocks)) {
		a.and_then(chunks, b.entry);
	}
	lir::Block blk {
		.entry = c0.entry,
		.patches = blocks.back().patches,
	};
	return {blk, t1};
}

Result Compiler::compile_match(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	const auto r0 = make_register();
	const auto ty = tpc.node_to_type.at(node[0]);
	const auto dty = ty->datatype;
	auto c0 = compile(node[0], ctx);
	auto c2 = make_chunk();
	std::vector<lir::Block> blocks {};
	for (const auto case_idx : ast.at(node[1])) {
		const auto ci = compile_case(case_idx, ctx, c2.entry, r0, dty, c0.opnd);
		blocks.push_back(ci);
	}

	//         +-------+-------+-------+------+
	//         |       |       |       |      |
	//         |       |       |       |      v
	// c0 --> ci0 --> ci1 --> ... --> cin --> c2 --> *
	//
	for (const auto& [a, b] : std::ranges::views::adjacent<2>(blocks)) {
		a.and_then(chunks, b.entry);
	}
	c0.block.and_then(chunks, blocks.at(0).entry);
	blocks.back().and_then(chunks, c2.entry);
	return {lir::Block::defer_last_jump(c0.block.entry, c2.index), r0};
}

lir::Block Compiler::compile_handler_decl(
	NodeIndex node_idx, Context ctx, lir::Label exit
) {
	const auto& node = ast.at(node_idx);
	const auto k = make_register();
	const auto id_idx = node[0];
	const auto& id_node = ast.at(id_idx);
	assert(id_node.type == NodeType::GUID);
	const auto param_list_idx = node[1];
	const auto& param_list_node = ast.at(param_list_idx);
	assert(param_list_node.type == NodeType::METALIST);
	const auto tag = effect_tags.at(id_node.guid);
	const Context handler_ctx {
		.handlers = ctx.handlers,
		.current_closure = ctx.current_closure,
		.current_resumption = k,
	};

	auto c0 = compile(node[3], handler_ctx);
	auto c1 = make_chunk();
	auto c2 = make_chunk();
	auto c3 = make_chunk();

	// c1 --> c0 --> c2 --> exit
	// |
	// v
	// c3 --> *

	c1.emit(Opcode::CATCH, tag, c3.entry);
	c1.emit(Opcode::POP, k);
	for (auto param_idx : param_list_node) {
		const auto& param_node = ast.at(param_idx);
		assert(param_node.type == NodeType::PARAM);
		const auto& param_name = ast.at(param_node[1]);
		assert(param_name.type == NodeType::GUID);
		c1.emit(Opcode::POP, variables.at(param_name.guid));
	}
	c1.emit(lir::Opcode::JMP, c0.block.entry);
	c1.finalize();

	c0.block.and_then(chunks, c2.entry);

	c2.emit(lir::Opcode::JMP, exit);
	c2.finalize();

	return lir::Block::defer_last_jump(c1.entry, c3.index);
}

Result Compiler::compile_constant(lir::Operand opnd) {
	auto c0 = make_chunk();
	return {lir::Block::defer_last_jump(c0.entry, c0.index), opnd};
}

lir::ChunkRef Compiler::make_chunk() {
	auto l = make_label();
	lir::Chunk chunk {};
	chunk.add_label(l);
	chunks.push_back(chunk);
	return lir::ChunkRef {
		.chunks = chunks,
		.index = chunks.size() - 1,
		.entry = l,
	};
}

} // namespace compiler
