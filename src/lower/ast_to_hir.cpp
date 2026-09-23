#include "lower/ast_to_hir.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>

#include "ast.hpp"
#include "hir/hir.hpp"
#include "symbol_table.hpp"
#include "type.hpp"

namespace lower::ast_to_hir {

constexpr auto builtins = {
	"read_int",
	"read_char",
	"write_int",
	"write_char",
	"write_str",
};

std::optional<Variable> Compiler::classify_variable(Guid sym) {
	const auto scope = symtab.declaration_scope_of(sym).value();
	if (scope == symtab.builtin_scope) {
		return std::nullopt;
	}
	const auto name = std::string(pool.find(symtab.name_of(sym).value()));
	auto poly = checker.variables.at(sym);
	// FIXME: don't access the body directly. polymorphic type might have
	// quantified variables!
	auto mono = poly->scheme->body;
	if (std::dynamic_pointer_cast<Function>(mono)) {
		return LocalVariable {
			.register_ = make_register(),
		};
	} else if (auto closure = std::dynamic_pointer_cast<Closure>(mono)) {
		if (closure->captures.size() > 0) {
			return LocalVariable {
				.register_ = make_register(),
			};
		} else {
			return StaticFunction {
				.label = make_label(name),
			};
		}
	} else {
		return LocalVariable {
			.register_ = make_variable(name),
		};
	}
}

hir::Module Compiler::start_module() {
	hir::Label write_int_tag;
	hir::Label write_char_tag;
	hir::Label write_str_tag;

	for (const auto& [name, tag] : effect_tags) {
		const auto name_ =
			std::string_view(pool.find(symtab.name_of(name).value()));
		if (name_ == "write_int") {
			write_int_tag = hir::Label {std::to_string(tag)};
		} else if (name_ == "write_char") {
			write_char_tag = hir::Label {std::to_string(tag)};
		} else if (name_ == "write_str") {
			write_str_tag = hir::Label {std::to_string(tag)};
		}
	}

	hir::Module current_module {
		.static_symbols = {},
	};

	auto nil = current_module.register_constant(
		make_label("nil"), hir::Visibility::HIDDEN, hir::Nil {}
	);

	auto main_module = current_module.register_static_initialization(
		make_label("Main"),
		hir::Visibility::HIDDEN,
		hir::ModuleImport {.name = "Main"}
	);
	auto main_function = current_module.register_static_initialization(
		make_label("main"),
		hir::Visibility::HIDDEN,
		hir::MemberImport {
			.label = main_module,
			.name = "main",
		}
	);

	auto v0 = make_register();
	auto v1 = make_register();
	auto v2 = make_register();
	auto v3 = make_register();
	auto v4 = make_register();
	auto v5 = make_register();
	auto v6 = make_register();
	auto v7 = make_register();
	auto v8 = make_register();
	auto v9 = make_register();
	auto v10 = make_register();
	auto v11 = make_register();

	hir::Code c0 {};
	c0.printv(v2);
	c0.get_global(v4, nil);
	c0.resume(v3, v1, v4);

	hir::Block b0 {
		.body_code = std::make_shared<hir::Code>(c0),
		.child_blocks = {},
	};
	assert(b0.body_code != nullptr);

	hir::Region write_int_handler {
		.blocks = {{"0", b0}}, .entry_block = "0", .parameter_registers = {v1, v2}
	};

	hir::Code c4 {};
	c4.printc(v2);
	c4.get_global(v4, nil);
	c4.resume(v3, v1, v4);

	hir::Block b1 {
		.body_code = std::make_shared<hir::Code>(c4),
		.child_blocks = {},
	};
	assert(b1.body_code != nullptr);

	hir::Region write_char_handler {
		.blocks = {{"0", b1}}, .entry_block = "0", .parameter_registers = {v1, v2}
	};

	hir::Code c6 {};
	c6.printf(v2);
	c6.get_global(v4, nil);
	c6.resume(v3, v1, v4);

	auto a0 = std::make_shared<hir::Code>(c6);

	hir::Block b4 {
		.body_code = a0,
		.child_blocks = {},
	};
	assert(b4.body_code != nullptr);

	hir::Region write_str_handler {
		.blocks = {{"0", b4}}, .entry_block = "0", .parameter_registers = {v1, v2}
	};

	hir::Code c7 {};
	c7.get_global(v9, nil);
	c7.make_tuple(v10, {});
	c7.enclose(v11, main_function, v10);
	c7.call(v7, v11, {v9});

	auto a1 = std::make_shared<hir::Code>(c7);

	hir::Block write_str_expression {
		.body_code = a1,
		.child_blocks = {},
	};
	assert(write_str_expression.body_code != nullptr);

	hir::Code c5 {};
	c5.catch_(v6, write_str_tag, write_str_handler, write_str_expression);

	auto a2 = std::make_shared<hir::Code>(c5);

	hir::Block write_char_expression {
		.body_code = a2,
		.child_blocks = {},
	};
	assert(write_char_expression.body_code != nullptr);

	hir::Code c1 {};
	c1.catch_(v5, write_char_tag, write_char_handler, write_char_expression);

	auto a3 = std::make_shared<hir::Code>(c1);

	hir::Block write_int_expression {
		.body_code = a3,
		.child_blocks = {},
	};
	assert(write_int_expression.body_code != nullptr);

	hir::Code code {};
	code.catch_(v0, write_int_tag, write_int_handler, write_int_expression);

	auto a4 = std::make_shared<hir::Code>(code);

	hir::Block blk {
		.body_code = a4,
		.child_blocks = {},
	};
	assert(blk.body_code != nullptr);

	hir::Function start {
		.blocks = {{"0", blk}},
		.entry_block = "0",
		.parameter_registers = {},
		.is_builtin = false,
		.builtin_name = {},
	};

	current_module.register_constant(
		make_label("start"), hir::Visibility::EXPORTED, start
	);

	return current_module;
}

hir::Module Compiler::compile() {
	const auto syms = symtab.all_symbols(symtab.value_namespace);
	for (const auto& sym : syms) {
		if (not checker.variables.contains(sym)) continue;
		const auto variable = classify_variable(sym);
		if (not variable.has_value()) continue;
		variables[sym] = variable.value();
	}
	auto i = 0ul;
	for (const auto& [name, eff] : checker.name_to_effect) {
		effect_tags[name] = i++;
	}
	auto node_idx = ast.root_index;
	SignalHandlers handlers {};
	hir::Module current_module {
		.static_symbols = {},
	};
	Context ctx {
		.handlers = handlers,
		.current_module = current_module,
		.current_closure = std::nullopt,
	};
	[[maybe_unused]]
	auto result = compile(node_idx, ctx);
	return current_module;
}

Result Compiler::compile_app(NodeIndex node_idx, Context ctx) {
	hir::Code code {};
	const auto& node = ast.at(node_idx);

	auto result_register = make_register();

	std::vector<hir::Operand> operands;
	operands.push_back(hir::Operand {result_register});

	// Resolve function
	const auto& func_node = ast.at(node[0]);

	assert(func_node.type == NodeType::GUID);

	const auto func_name =
		std::string_view(pool.find(symtab.name_of(func_node.guid).value()));

	hir::Register function {};
	bool builtin_function_found = false;

	for (const auto& builtin : builtins) {
		if (func_name == std::string_view(builtin)) {
			auto func_register = make_register();
			const auto l = hir::Label {.name = std::string(func_name)};
			ctx.current_module.load_builtin(l);
			code.get_global(func_register, l);
			function = func_register;
			builtin_function_found = true;
			break;
		}
	}

	if (!builtin_function_found) {
		auto func_result = compile(node[0], ctx);
		code = code + func_result.code;
		function = func_result.result_register;
	}

	operands.push_back(hir::Operand {function});

	// Resolve arguments
	const auto& args_node = ast.at(node[1]);

	assert(args_node.type == NodeType::METALIST);

	std::vector<hir::Operand> args {};

	for (size_t i = 0; i < args_node.branch.children_count; i++) {
		auto arg_idx = args_node[i];
		auto arg_result = compile(arg_idx, ctx);
		code = code + arg_result.code;
		args.push_back(arg_result.result_register);
	}

	code.call(result_register, function, args);

	return Result {code, result_register};
}

Result Compiler::compile_perform(NodeIndex node_idx, Context ctx) {
	hir::Code code {};
	const auto& node = ast.at(node_idx);

	auto result_register = make_register();

	const auto& func_node = ast.at(node[0]);
	assert(func_node.type == NodeType::GUID);

	hir::Label effect_name {std::to_string(effect_tags.at(func_node.guid))};

	const auto& args_node = ast.at(node[1]);
	assert(args_node.type == NodeType::METALIST);

	std::vector<hir::Operand> args {};

	for (size_t i = 0; i < args_node.branch.children_count; i++) {
		auto arg_idx = args_node[i];
		auto arg_result = compile(arg_idx, ctx);
		code = code + arg_result.code;
		args.push_back(arg_result.result_register);
	}

	code.perform(result_register, effect_name, args);

	return Result {code, result_register};
}

Result Compiler::compile_if(NodeIndex node_idx, Context ctx) {
	hir::Code code {};
	const auto& node = ast.at(node_idx);

	auto cond_idx = node[0];
	auto then_idx = node[1];
	auto else_idx = node[2];

	auto result_register = make_register();

	auto cond_res = compile(cond_idx, ctx);
	code = code + cond_res.code;
	auto cond_opnd = cond_res.result_register;

	auto then_res = compile(then_idx, ctx);
	then_res.code.yield(then_res.result_register);

	auto else_res = compile(else_idx, ctx);
	else_res.code.yield(else_res.result_register);

	auto if_true_code = std::make_shared<hir::Code>(then_res.code);
	auto if_false_code = std::make_shared<hir::Code>(else_res.code);

	code.if_(
		result_register,
		cond_opnd,
		hir::Block {
			.body_code = if_true_code,
			.child_blocks = {},
		},
		hir::Block {
			.body_code = if_false_code,
			.child_blocks = {},
		}
	);

	return Result {code, result_register};
}

static bool is_declaration_node(NodeType type) {
	return type == NodeType::TYPE_DECL or type == NodeType::HANDLER_DECL
	    or type == NodeType::VAR_DECL or type == NodeType::FUN_DECL
	    or type == NodeType::EFF_DECL or type == NodeType::EFF_ALIAS;
}

auto Compiler::compile_declaration(NodeIndex node_idx, Context ctx)
	-> std::optional<
		std::tuple<hir::Code, hir::Operand, std::optional<hir::Code*>>> {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::TYPE_DECL: return std::nullopt;
		case NodeType::HANDLER_DECL: {
			const auto id_idx = node[0];
			const auto& id_node = ast.at(id_idx);
			assert(id_node.type == NodeType::GUID);
			const auto param_list_idx = node[1];
			const auto& param_list_node = ast.at(param_list_idx);
			assert(param_list_node.type == NodeType::METALIST);
			const auto body_idx = node[3];

			hir::Code code {};
			hir::Region region {};

			auto v2 = make_register();
			region.parameter_registers.push_back(v2);
			for (auto param_idx : param_list_node) {
				const auto& param_node = ast.at(param_idx);
				assert(param_node.type == NodeType::PARAM);
				const auto& param_name = ast.at(param_node[1]);
				assert(param_name.type == NodeType::GUID);
				auto r =
					std::get<LocalVariable>(variables.at(param_name.guid)).register_;
				region.parameter_registers.push_back(r);
			}

			Context body_ctx {
				.handlers = ctx.handlers,
				.current_module = ctx.current_module,
				.current_closure = ctx.current_closure,
			};
			body_ctx.handlers.has_resume_handler = true;
			body_ctx.handlers.resume_handler = v2;
			auto r = compile(body_idx, body_ctx);
			hir::Block blk {
				.body_code = std::make_shared<hir::Code>(r.code),
				.child_blocks = {},
			};
			region.blocks["0"] = blk;
			region.entry_block = "0";

			// auto l = make_label();
			// l = ctx.current_module.register_constant(
			// 	l,
			// 	hir::Visibility::HIDDEN,
			// 	hir::Integer(static_cast<int>(effect_tags.at(id_node.guid)))
			// );
			hir::Label l {std::to_string(effect_tags.at(id_node.guid))};
			auto b = std::make_shared<hir::Code>();
			auto v1 = make_register();
			code.instructions.push_back(
				hir::Instruction {
					.opcode = hir::Opcode::CATCH,
					.operands = {
						v1,
						l,
						region,
						hir::Block {
							.body_code = b,
							.child_blocks = {},
						}
					}
				}
			);

			return std::make_tuple(code, v1, b.get());
		}
		case NodeType::VAR_DECL: {
			hir::Code code {};

			auto id_idx = node[1];
			auto exp_idx = node[3];

			const auto& id_node = ast.at(id_idx);
			assert(id_node.type == NodeType::GUID);
			const auto name =
				std::string(pool.find(symtab.name_of(id_node.guid).value()));

			auto initial_result = compile(exp_idx, ctx);
			code = code + initial_result.code;
			auto initial = initial_result.result_register;

			auto r = std::get<LocalVariable>(variables.at(id_node.guid)).register_;
			if (initial.is_mutable) {
				code.clone(r, initial);
			} else {
				code.alloc(r, initial);
			}

			return std::make_tuple(code, r, std::nullopt);
		}
		case NodeType::FUN_DECL: {
			hir::Code code {};

			auto id_idx = node[0];
			auto params_idx = node[1];
			auto body_idx = node[4];

			const auto& id_node = ast.at(id_idx);
			const auto& params_node = ast.at(params_idx);
			assert(id_node.type == NodeType::GUID);

			auto variable_register = make_variable(
				std::string(pool.find(symtab.name_of(id_node.guid).value()))
			);

			auto variable = variables.at(id_node.guid);

			const auto ty = checker.variables.at(id_node.guid);
			// FIXME
			const auto dty = ty->scheme->body;
			const auto cty = std::dynamic_pointer_cast<Closure>(dty);
			assert(cty != nullptr);

			std::vector<hir::Register> parameter_registers {};

			for (auto param_id : params_node) {
				const auto& param_node = ast.at(param_id);
				assert(param_node.type == NodeType::PARAM);
				const auto& param_name = ast.at(param_node[1]);
				assert(param_name.type == NodeType::GUID);
				const auto name =
					std::string(pool.find(symtab.name_of(param_name.guid).value()));
				auto param_register =
					std::get<LocalVariable>(variables.at(param_name.guid)).register_;
				parameter_registers.push_back(param_register);
			}

			if (auto static_function = std::get_if<StaticFunction>(&variable)) {
				assert(cty->captures.size() == 0);
				Context body_ctx {
					.handlers =
						{
							.continue_handler = {},
							.break_handler = {},
							.return_handler = {},
							.resume_handler = {},
							.has_continue_handler = false,
							.has_break_handler = false,
							.has_return_handler = false,
							.has_resume_handler = false,
						},
					.current_module = ctx.current_module,
					.current_closure = std::nullopt,
				};

				auto body_result = compile(body_idx, body_ctx);

				body_result.code.instructions.push_back(
					hir::Instruction {hir::Opcode::RET, {body_result.result_register}}
				);

				hir::Block block {
					.body_code = std::make_shared<hir::Code>(body_result.code),
					.child_blocks = {},
				};
				auto entry = make_label();
				hir::Function function {
					.blocks = {{entry.name, block}},
					.entry_block = entry.name,
					.parameter_registers = parameter_registers,
					.is_builtin = false,
					.builtin_name = {0},
				};

				auto t1 = make_register();
				auto l2 = ctx.current_module.register_constant(
					static_function->label, hir::Visibility::HIDDEN, function
				);

				code.get_global(variable_register, l2);

				return std::make_tuple(code, variable_register, std::nullopt);
			} else if (auto local_variable = std::get_if<LocalVariable>(&variable)) {
				auto t4 = make_register();
				auto t3 = make_variable("env");
				auto t5 = local_variable->register_;

				ClosureInfo clinfo {
					.function_name = id_node.guid,
					.object = t3,
					.offsets = {},
				};

				std::vector<hir::Operand> closure_values {};
				{
					auto i = 0ul;
					for (const auto& [a, b] : cty->captures) {
						const auto ti = make_register();
						code.clone(ti, std::get<LocalVariable>(variables.at(a)).register_);
						closure_values.push_back(ti);
						clinfo.offsets[a] = i;
						i++;
					}
				}

				Context body_ctx {
					.handlers =
						{
							.continue_handler = {},
							.break_handler = {},
							.return_handler = {},
							.resume_handler = {},
							.has_continue_handler = false,
							.has_break_handler = false,
							.has_return_handler = false,
							.has_resume_handler = false,
						},
					.current_module = ctx.current_module,
					.current_closure = clinfo,
				};

				auto body_result = compile(body_idx, body_ctx);

				body_result.code.instructions.push_back(
					hir::Instruction {hir::Opcode::RET, {body_result.result_register}}
				);

				hir::Block block {
					.body_code = std::make_shared<hir::Code>(body_result.code),
					.child_blocks = {},
				};
				std::vector<hir::Register> new_parameter_registers {t3};
				for (auto p : parameter_registers) {
					new_parameter_registers.push_back(p);
				}
				auto entry = make_label();
				hir::Function function {
					.blocks = {{entry.name, block}},
					.entry_block = entry.name,
					.parameter_registers = new_parameter_registers,
					.is_builtin = false,
					.builtin_name = {0},
				};

				auto t1 = make_register();
				auto l = make_label();
				auto l2 = ctx.current_module.register_constant(
					l, hir::Visibility::HIDDEN, function
				);

				code.make_tuple(t4, closure_values);
				code.enclose(t5, l2, t4);

				return std::make_tuple(code, t5, std::nullopt);
			} else {
				assert(false);
			}
		}
		case NodeType::EFF_DECL: return std::nullopt;
		case NodeType::EFF_ALIAS: return std::nullopt;
		default: {
			throw std::runtime_error(
				std::format("unhandled node type {}", node_type_repr(node.type))
			);
		}
	}
}

Result Compiler::compile(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::APP: return compile_app(node_idx, ctx);
		case NodeType::NUM: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_num = hir::Integer {node.num};
			auto l = make_label();
			auto l2 = ctx.current_module.register_constant(
				l, hir::Visibility::HIDDEN, {literal_num}
			);
			code.get_global(result_register, l2);
			return Result {code, result_register};
		}
		case NodeType::BLK: {
			hir::Code code {};
			hir::Register result_register {};
			Context inner_ctx {ctx};
			for (auto idx : node) {
				const auto& node = ast.at(idx);
				if (is_declaration_node(node.type)) {
					if (auto r = compile_declaration(idx, inner_ctx)) {
						auto [c, v, b] = *r;
						if (b.has_value())
							throw std::runtime_error("can't have handler on block");
						code = code + c;
					}
				} else {
					auto opnd_res = compile(idx, inner_ctx);
					code = code + opnd_res.code;
					result_register = opnd_res.result_register;
				}
			}
			return Result {code, result_register};
		}
		case NodeType::IF: return compile_if(node_idx, ctx);
		case NodeType::WHEN: {
			hir::Code code {};
			const auto& node = ast.at(node_idx);

			auto cond_idx = node[0];
			auto then_idx = node[1];

			auto result_register = make_register();

			auto cond_res = compile(cond_idx, ctx);
			code = code + cond_res.code;
			auto cond_opnd = cond_res.result_register;

			auto then_res = compile(then_idx, ctx);
			then_res.code.yield(then_res.result_register);

			hir::Code if_false {};

			auto r = make_register();
			auto l = ctx.current_module.register_constant(
				make_label("nil"), hir::Visibility::HIDDEN, hir::Nil {}
			);
			if_false.get_global(r, l);
			if_false.yield(r);

			auto if_true_code = std::make_shared<hir::Code>(then_res.code);
			auto if_false_code = std::make_shared<hir::Code>(if_false);

			code.if_(
				result_register,
				cond_opnd,
				hir::Block {
					.body_code = if_true_code,
					.child_blocks = {},
				},
				hir::Block {
					.body_code = if_false_code,
					.child_blocks = {},
				}
			);

			return Result {code, result_register};
		}
		case NodeType::FOR: {
			hir::Code code {};
			auto decl_idx = node[0];
			auto to_idx = node[1];
			auto step_idx = node[2];
			auto then_idx = node[3];

			const auto& step_node = ast.at(step_idx);

			auto next_label = make_label();
			auto stop_label = make_label();

			SignalHandlers new_handlers {
				{.label = next_label},
				{.label = stop_label},
				ctx.handlers.return_handler,
				ctx.handlers.resume_handler,
				true,
				true,
				ctx.handlers.has_return_handler,
				ctx.handlers.has_resume_handler,
			};

			hir::Operand step_value {};

			if (step_node.type != NodeType::EMPTY) {
				auto step_result = compile(step_idx, ctx);
				code = code + step_result.code;
				step_value = step_result.result_register;
			} else {
				auto l = make_label();
				auto l2 = ctx.current_module.register_constant(
					l, hir::Visibility::HIDDEN, hir::Integer(1)
				);
				auto r = make_register();
				code.get_global(r, l2);
				step_value = r;
			}

			Context header_ctx {ctx};

			Context body_ctx {
				.handlers = new_handlers,
				.current_module = ctx.current_module,
				.current_closure = ctx.current_closure,
			};

			hir::Register var_opnd;
			if (auto res = compile_declaration(decl_idx, header_ctx)) {
				auto [c, v, b] = *res;
				assert(not b.has_value());
				code = code + c;
				var_opnd = v.registuhr;
			} else {
				throw std::runtime_error("");
			}

			auto to_result = compile(to_idx, header_ctx);
			code = code + to_result.code;
			auto to_opnd = to_result.result_register;

			auto then_result = compile(then_idx, body_ctx);
			auto then_opnd = then_result.result_register;

			hir::Code if_break {};
			if_break.brake(stop_label);

			hir::Code loop_body {};
			auto cond = make_register();
			loop_body.equals(cond, var_opnd, to_opnd);
			loop_body.if_true(
				cond,
				hir::Block {
					.body_code = std::make_shared<hir::Code>(if_break),
					.child_blocks = {},
				}
			);
			loop_body = loop_body + then_result.code;
			auto t1 = make_register();
			auto l = make_label();
			auto l2 = ctx.current_module.register_constant(
				l, hir::Visibility::HIDDEN, hir::Integer(1)
			);
			auto t2 = make_register();
			loop_body.get_global(t2, l2);
			loop_body.add(t1, var_opnd, t2);
			loop_body.store(var_opnd, t1);

			code.loop(
				next_label,
				stop_label,
				hir::Block {
					.body_code = std::make_shared<hir::Code>(loop_body),
					.child_blocks = {},
				}
			);

			return {code, then_opnd};
		}
		case NodeType::WHILE: {
			hir::Code code {};
			auto cond_idx = node[0];
			auto then_idx = node[1];

			const auto next_label = make_label();
			const auto stop_label = make_label();

			SignalHandlers new_handlers {
				{.label = next_label},
				{.label = stop_label},
				{},
				ctx.handlers.resume_handler,
				true,
				true,
				ctx.handlers.has_return_handler,
				ctx.handlers.has_resume_handler,
			};

			hir::Operand step_value {};

			Context body_ctx {
				.handlers = new_handlers,
				.current_module = ctx.current_module,
				.current_closure = ctx.current_closure,
			};

			auto cond_result = compile(cond_idx, ctx);
			auto cond_opnd = cond_result.result_register;

			auto then_result = compile(then_idx, body_ctx);
			auto then_opnd = then_result.result_register;

			hir::Code if_break {};
			if_break.brake(stop_label);

			hir::Code loop_body {};
			loop_body = loop_body + cond_result.code;
			loop_body.if_false(
				cond_opnd,
				hir::Block {
					.body_code = std::make_shared<hir::Code>(if_break),
					.child_blocks = {},
				}
			);
			loop_body = loop_body + then_result.code;

			code.loop(
				next_label,
				stop_label,
				hir::Block {
					.body_code = std::make_shared<hir::Code>(loop_body),
					.child_blocks = {},
				}
			);

			return {code, then_opnd};
		}
		case NodeType::BREAK: {
			hir::Code code {};
			auto value = compile(node[0], ctx);
			code = code + value.code;
			code.brake(ctx.handlers.break_handler.label);
			return {code, {}};
		}
		case NodeType::CONTINUE: {
			hir::Code code {};
			auto value = compile(node[0], ctx);
			code = code + value.code;
			code.continue_(ctx.handlers.continue_handler.label);
			return {code, {}};
		}
		case NodeType::ASS: {
			hir::Code code {};

			auto place_idx = node[0];
			auto place_result = compile_lvalue(place_idx, ctx);
			code = code + place_result.code;
			auto place = place_result.result_register;
			auto value_idx = node[1];
			auto value_result = compile(value_idx, ctx);
			code = code + value_result.code;
			auto value = value_result.result_register;
			code.store(place, value);

			return {code, value};
		}

#define BINARY_ARITH(OPCODE)                            \
	{                                                     \
		hir::Code code {};                                  \
		auto left_idx = node[0];                            \
		auto right_idx = node[1];                           \
                                                        \
		auto left_result = compile(left_idx, ctx);          \
		auto right_result = compile(right_idx, ctx);        \
		code = code + left_result.code + right_result.code; \
		auto result_register = make_register();             \
		code.instructions.push_back(                        \
			hir::Instruction {                                \
				hir::Opcode::OPCODE,                            \
				{result_register,                               \
		     left_result.result_register,                   \
		     right_result.result_register}                  \
			}                                                 \
		);                                                  \
		return {code, result_register};                     \
	}

		case NodeType::OR: BINARY_ARITH(OR);
		case NodeType::AND: BINARY_ARITH(AND);
		case NodeType::GTN: BINARY_ARITH(GREATER);
		case NodeType::LTN: BINARY_ARITH(LESS);
		case NodeType::GTE: BINARY_ARITH(GREATER_EQ);
		case NodeType::LTE: BINARY_ARITH(LESS_EQ);
		case NodeType::EQ:
			BINARY_ARITH(EQ);

			// a[b]
			//
			// a must be an lvalue
			// b must be an rvalue
			// return an lvalue

		case NodeType::AT: assert(false);
		case NodeType::ADD: BINARY_ARITH(ADD);
		case NodeType::SUB: BINARY_ARITH(SUB);
		case NodeType::MUL: BINARY_ARITH(MUL);
		case NodeType::DIV: BINARY_ARITH(DIV);
		case NodeType::MOD: BINARY_ARITH(MOD);
		case NodeType::NOT: {
			hir::Code code {};
			auto result_register = make_register();
			auto exp_result = compile(node[0], ctx);
			code = code + exp_result.code;
			code.instructions.push_back(
				hir::Instruction {
					hir::Opcode::NOT, {result_register, exp_result.result_register}
				}
			);
			return {code, result_register};
		}
		case NodeType::STR: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_string = hir::String {node.str_id};
			auto l = ctx.current_module.register_constant(
				make_label(), hir::Visibility::HIDDEN, literal_string
			);
			code.get_global(result_register, l);
			return Result {code, result_register};
		}
		case NodeType::NIL: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_nil = hir::Nil {};
			auto l = make_label();
			auto l2 = ctx.current_module.register_constant(
				l, hir::Visibility::HIDDEN, literal_nil
			);
			code.get_global(result_register, l2);
			return Result {code, result_register};
		}
		case NodeType::TRUE: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_true = hir::Boolean {true};
			auto l = make_label();
			auto l2 = ctx.current_module.register_constant(
				l, hir::Visibility::HIDDEN, literal_true
			);
			code.get_global(result_register, l2);
			return Result {code, result_register};
		}
		case NodeType::FALSE: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_false = hir::Boolean {false};
			auto l = make_label();
			auto l2 = ctx.current_module.register_constant(
				l, hir::Visibility::HIDDEN, literal_false
			);
			code.get_global(result_register, l2);
			return Result {code, result_register};
		}
		case NodeType::LET: {
			hir::Code code_ {};
			hir::Code* code = &code_;

			auto decls_idx = node[0];
			auto exp_idx = node[1];

			const auto& decls_node = ast.at(decls_idx);

			Context inner_ctx {ctx};
			for (auto idx : decls_node) {
				if (auto r = compile_declaration(idx, inner_ctx)) {
					auto [c, v, b] = *r;
					if (b.has_value()) {
						*code = *code + c;
						code = *b;
					} else {
						*code = *code + c;
					}
				} else {
					const auto& node = ast.at(idx);
					throw std::runtime_error(
						std::format(
							"can't compile declaration {} inside let binding",
							node_type_repr(node.type)
						)
					);
				}
			}
			auto expression_result = compile(exp_idx, inner_ctx);
			*code = *code + expression_result.code;
			return {code_, expression_result.result_register};
		}
		case NodeType::CHAR: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_character = hir::Character {node.character};
			auto l = ctx.current_module.register_constant(
				make_label(), hir::Visibility::HIDDEN, literal_character
			);
			code.get_global(result_register, l);
			return Result {code, result_register};
		}
		case NodeType::PATH: {
			hir::Code code {};
			auto v_res = compile_lvalue(node[0], ctx);
			code = code + v_res.code;
			auto v = v_res.result_register;
			auto a = make_register();
			code.load(a, v);
			return {code, a};
		}
		case NodeType::INSTANCE:
			assert(false && "used only in typechecking. should not be evaluated");
		case NodeType::AS: {
			return compile(node[0], ctx);
		}
		case NodeType::GUID: {
			hir::Code code {};
			auto v = make_register();
			auto r = compile_lvalue(node_idx, ctx);
			code = code + r.code;
			code.load(v, r.result_register);
			return {code, v};
		}
		case NodeType::RECORD_LITERAL: {
			const auto field_count = node.size();
			hir::Code code {};
			auto t1 = make_register();
			const auto l1 = ctx.current_module.register_static_initialization(
				make_label(),
				hir::Visibility::HIDDEN,
				hir::StaticAllocation {field_count}
			);
			code.get_global(t1, l1);
			const auto t2 = make_register();
			code.alloc(t2, t1);
			for (auto i = 0ul; i < field_count; i++) {
				const auto& field_node = ast.at(node[i]);
				assert(field_node.type == NodeType::FIELD);
				const auto ti = make_register();
				const auto li = ctx.current_module.register_constant(
					make_label(),
					hir::Visibility::HIDDEN,
					hir::Integer(static_cast<int>(i))
				);
				const auto ti2 = make_register();
				code.get_global(ti2, li);
				code.get_element(ti, t2, ti2);
				const auto c0 = compile(field_node[1], ctx);
				code = code + c0.code;
				code.store(ti, c0.result_register);
			}
			return {code, t2};
		}
		case NodeType::MODULE_BODY: {
			hir::Code code_ {};
			hir::Code* code = &code_;
			const auto& decl_list = ast.at(node[0]);
			assert(decl_list.type == NodeType::METALIST);
			for (auto decl : decl_list) {
				if (auto r = compile_declaration(decl, ctx)) {
					auto [c, v, b] = *r;
					if (b.has_value()) {
						*code = *code + c;
						code = *b;
					} else {
						*code = *code + c;
					}
				}
			}
			return {code_, {}};
		}
		case NodeType::PERFORM: return compile_perform(node_idx, ctx);
		case NodeType::RESUME: {
			assert(ctx.handlers.has_resume_handler);
			hir::Code code {};
			auto r = compile(node[0], ctx);
			auto v = make_register();
			code = code + r.code;
			code.resume(v, ctx.handlers.resume_handler, r.result_register);
			return {code, v};
		}
		case NodeType::REPEAT: {
			hir::Code code {};
			auto r0 = compile(node[0], ctx);
			auto r1 = compile(node[1], ctx);
			auto v = make_register();
			code = code + r0.code;
			code = code + r1.code;
			code.instructions.push_back(
				hir::Instruction {
					hir::Opcode::REPEAT, {v, r0.result_register, r1.result_register}
				}
			);
			return {code, v};
		}
		case NodeType::LEN: {
			hir::Code code {};
			auto r0 = compile(node[0], ctx);
			auto v = make_register();
			code = code + r0.code;
			code.get_element(v, r0.result_register, hir::Immediate(1ul));
			return {code, v};
		}
		case NodeType::ARRAY_LITERAL: {
			hir::Code code {};
			auto v = make_register();
			std::vector<hir::Operand> elements {};
			for (auto elt_idx : node) {
				auto r = compile(elt_idx, ctx);
				code = code + r.code;
				elements.push_back(r.result_register);
			}
			code.make_array(v, elements);
			return {code, v};
		}
		default: {
			throw std::runtime_error(
				std::format("unhandled node type {}", node_type_repr(node.type))
			);
		}
	}
	assert(false);
}

hir::Register Compiler::make_register() {
	return hir::Register {
		.id = register_count++,
		.name = std::nullopt,
		.is_mutable = false,
	};
}

hir::Label Compiler::make_label(std::optional<std::string> name) {
	if (name.has_value()) {
		return hir::Label {name.value()};
	} else {
		return hir::Label {std::format("{}", label_count++)};
	}
}

hir::Register Compiler::make_variable(std::optional<std::string> name) {
	return hir::Register {
		.id = register_count++,
		.name = name,
		.is_mutable = true,
	};
}

Result Compiler::compile_lvalue(NodeIndex node_idx, Context ctx) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::AT: {
			auto place_idx = node[0];
			auto offset_idx = node[1];
			hir::Code code {};
			auto place_res = compile_lvalue(place_idx, ctx);
			code = code + place_res.code;
			auto place = place_res.result_register;
			auto offset_res = compile(offset_idx, ctx);
			code = code + offset_res.code;
			auto offset = offset_res.result_register;
			auto a = make_variable(std::nullopt);
			code.get_element(a, place, offset);
			return {code, a};
		}
		case NodeType::PATH: {
			return compile_lvalue(node[0], ctx);
		}
		case NodeType::GUID: {
			const auto maybe_rel = symtab.relative_scope_of(node.guid, node_idx);
			assert(maybe_rel.has_value());
			const auto rel = maybe_rel.value();

			auto var = variables.at(node.guid);
			hir::Code code {};
			if (auto static_function = std::get_if<StaticFunction>(&var)) {
				auto r0 = make_register();
				auto r1 = make_register();
				code.make_tuple(r0, {});
				code.enclose(r1, static_function->label, r0);
				return {code, r1};
			} else if (auto local_variable = std::get_if<LocalVariable>(&var)) {
				if (rel == RelativeScope::FREE) {
					if (auto closure = ctx.current_closure) {
						auto offset = closure->offsets.at(node.guid);
						auto r0 = make_variable(std::nullopt);
						code.get_element(r0, closure->object, hir::Immediate(offset));
						return {code, r0};
					} else {
						assert(false);
					}
				} else {
					return {code, local_variable->register_};
				}
			}
			assert(false);
		}
		case NodeType::FIELD_ACCESS: {
			hir::Code code {};
			auto r = compile_lvalue(node[0], ctx);
			const auto t0 = std::dynamic_pointer_cast<Record>(
				get_datatype(get_datatype(checker.node_to_type.at(node[0])))
			);
			assert(t0 != nullptr);
			const auto& field_node = ast.at(node[1]);
			assert(field_node.type == NodeType::ID);
			const auto field_name = std::string(pool.find(field_node.str_id));
			auto found = std::ranges::find(t0->fields, field_name, [](const auto& x) {
				return x.first;
			});
			assert(found != t0->fields.end());
			auto d = std::distance(t0->fields.begin(), found);
			const auto field_ty = found->second;
			const auto v0 = make_variable();
			code = code + r.code;
			code.get_element(v0, r.result_register, hir::Immediate(d));
			return {code, v0};
		}
		default: {
			throw std::runtime_error(
				std::format("unhandled node type {}", node_type_repr(node.type))
			);
		}
	}
	assert(false);
}
} // namespace lower::ast_to_hir
