#include "hir_compiler.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>

#include "hir.hpp"

#define err(MSG) fprintf(stderr, "HIR_COMPILER_ERROR: " MSG "\n")

namespace hir_compiler {

constexpr auto builtins = {
	"read_int", "read_char", "write_int", "write_char", "write_str", "make_array"
};

hir::Code Compiler::compile() {
	auto node_idx = ast.root_index;
	SignalHandlers handlers {};
	auto scope_id = env.root_scope_id;
	auto result = compile(node_idx, handlers, scope_id);
	return result.code;
}

Result Compiler::compile_app(
	NodeIndex node_idx, SignalHandlers handlers,
	Env<hir::Operand>::ScopeID scope_id
) {
	hir::Code code {};
	const auto& node = ast.at(node_idx);

	auto result_register = make_register();

	std::vector<hir::Operand> operands;
	operands.push_back(hir::Operand {result_register});

	// Resolve function
	const auto& func_node = ast.at(node[0]);

	assert(func_node.type == NodeType::ID);

	const auto func_name = pool.find(func_node.str_id);

	hir::Register function {};
	bool builtin_function_found = false;

	for (const auto& builtin : builtins) {
		if (strcmp(func_name, builtin) == 0) {
			auto func_register = make_register();
			code.builtin(func_register, hir::String {func_node.str_id});
			function = func_register;
			builtin_function_found = true;
			break;
		}
	}

	if (!builtin_function_found) {
		auto func_result = compile(node[0], handlers, scope_id);
		code = code + func_result.code;
		function = func_result.result_register;
	}

	operands.push_back(hir::Operand {function});

	// Resolve arguments
	const auto& args_node = ast.at(node[1]);

	assert(args_node.type == NodeType::BLK);

	std::vector<hir::Operand> args {};

	for (size_t i = 0; i < args_node.branch.children_count; i++) {
		auto arg_idx = args_node[i];
		const auto& arg_node = ast.at(arg_idx);
		auto arg_result = compile(arg_idx, handlers, scope_id);
		code = code + arg_result.code;
		args.push_back(hir::Operand {arg_result.result_register});
	}

	code.call(result_register, function, args);

	return Result {code, result_register};
}

Result Compiler::compile_if(
	NodeIndex node_idx, SignalHandlers handlers,
	Env<hir::Operand>::ScopeID scope_id
) {
	hir::Code code {};
	const auto& node = ast.at(node_idx);

	auto cond_idx = node[0];
	auto then_idx = node[1];
	auto else_idx = node[2];

	auto result_register = make_register();

	auto cond_res = compile(cond_idx, handlers, scope_id);
	code = code + cond_res.code;
	auto cond_opnd = cond_res.result_register;
	assert(not cond_opnd.contains_pointer());

	auto then_res = compile(then_idx, handlers, scope_id);
	then_res.code.copy(result_register, then_res.result_register);

	auto else_res = compile(else_idx, handlers, scope_id);
	else_res.code.copy(result_register, else_res.result_register);

	auto if_true_code = std::make_shared<hir::Code>(then_res.code);
	auto if_false_code = std::make_shared<hir::Code>(else_res.code);

	code.if_true(cond_opnd, hir::Block {if_true_code});
	code.if_false(cond_opnd, hir::Block {if_false_code});

	return Result {code, result_register};
}

bool Compiler::is_simple_path(NodeIndex node_idx) {
	return ast.at(node_idx).type == NodeType::ID;
}

hir::Code Compiler::find_aggregate_indexes(
	NodeIndex node_idx, hir::Register& aggregate,
	std::vector<hir::Operand>& indexes, SignalHandlers handlers,
	Env<hir::Operand>::ScopeID scope_id
) {
	const auto& node = ast.at(node_idx);
	if (node.type == NodeType::AT) {
		hir::Code code {};
		auto base_idx = node[0];
		auto index_idx = node[1];
		auto base_code =
			find_aggregate_indexes(base_idx, aggregate, indexes, handlers, scope_id);
		auto index_result = compile(index_idx, handlers, scope_id);
		indexes.push_back(index_result.result_register);
		code = code + base_code + index_result.code;
		return code;
	} else if (node.type == NodeType::ID) {
		auto maybe_var = env.find(scope_id, node.str_id);
		if (maybe_var == nullptr
		    || maybe_var->kind != hir::Operand::Kind::REGISTER) {
			assert(false);
		}
		aggregate = maybe_var->registuhr;
		return {};
	} else if (node.type == NodeType::PATH) {
		return find_aggregate_indexes(
			ast.at(node_idx)[0], aggregate, indexes, handlers, scope_id
		);
	} else {
		fprintf(stderr, "NODEID: %d\n", node_idx.index);
		assert(false);
	}
}

Result Compiler::compile(
	NodeIndex node_idx, SignalHandlers handlers,
	Env<hir::Operand>::ScopeID scope_id
) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::EMPTY: assert(false && "empty node shouldn't be evaluated");
		case NodeType::APP: return compile_app(node_idx, handlers, scope_id);
		case NodeType::NUM: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_num = hir::Integer {node.num};
			code.copy(result_register, literal_num);
			return Result {code, result_register};
		}
		case NodeType::BLK: {
			hir::Code code {};
			auto inner_scope_id = env.create_child_scope(scope_id);
			hir::Register result_register {};
			for (auto idx : node) {
				auto opnd_res = compile(idx, handlers, inner_scope_id);
				code = code + opnd_res.code;
				result_register = opnd_res.result_register;
			}
			return Result {code, result_register};
		}
		case NodeType::IF: return compile_if(node_idx, handlers, scope_id);
		case NodeType::WHEN: {
			hir::Code code {};
			const auto& node = ast.at(node_idx);

			auto cond_idx = node[0];
			auto then_idx = node[1];

			auto result_register = make_register();

			auto cond_res = compile(cond_idx, handlers, scope_id);
			code = code + cond_res.code;
			auto cond_opnd = cond_res.result_register;
			assert(not cond_opnd.contains_pointer());

			auto then_res = compile(then_idx, handlers, scope_id);
			then_res.code.copy(result_register, then_res.result_register);

			auto if_true_code = std::make_shared<hir::Code>(then_res.code);

			code.if_true(cond_opnd, hir::Block {if_true_code});

			return Result {code, result_register};
		}
		case NodeType::FOR: {
			hir::Code code {};
			auto decl_idx = node[0];
			auto to_idx = node[1];
			auto step_idx = node[2];
			auto then_idx = node[3];

			const auto& step_node = ast.at(step_idx);

			SignalHandlers new_handlers {
				{}, {}, {}, true, true, handlers.has_return_handler
			};

			hir::Operand step_value {};

			if (step_node.type != NodeType::EMPTY) {
				auto step_result = compile(step_idx, handlers, scope_id);
				code = code + step_result.code;
				step_value = step_result.result_register;
			} else {
				step_value = hir::Integer(1);
			}

			auto inner_scope_id = env.create_child_scope(scope_id);

			auto var_result = compile(decl_idx, handlers, inner_scope_id);
			code = code + var_result.code;
			auto var_opnd = var_result.result_register;

			auto to_result = compile(to_idx, handlers, inner_scope_id);
			code = code + to_result.code;
			auto to_opnd = to_result.result_register;

			auto then_result = compile(then_idx, handlers, inner_scope_id);
			auto then_opnd = then_result.result_register;

			hir::Code if_break {};
			if_break.brake();

			hir::Code loop_body {};
			auto cond = make_register();
			loop_body.equals(cond, var_opnd, to_opnd);
			loop_body.if_true(
				cond, hir::Block {std::make_shared<hir::Code>(if_break)}
			);
			loop_body = loop_body + then_result.code;
			loop_body.inc(var_opnd);

			code.loop(hir::Block {std::make_shared<hir::Code>(loop_body)});

			return {code, then_opnd};
		}
		case NodeType::WHILE: {
			hir::Code code {};
			auto cond_idx = node[0];
			auto then_idx = node[1];

			SignalHandlers new_handlers {
				{}, {}, {}, true, true, handlers.has_return_handler
			};

			hir::Operand step_value {};

			auto cond_result = compile(cond_idx, handlers, scope_id);
			auto cond_opnd = cond_result.result_register;

			auto then_result = compile(then_idx, handlers, scope_id);
			auto then_opnd = then_result.result_register;

			hir::Code if_break {};
			if_break.brake();

			hir::Code loop_body {};
			loop_body = loop_body + cond_result.code;
			loop_body.if_false(
				cond_opnd, hir::Block {std::make_shared<hir::Code>(if_break)}
			);
			loop_body = loop_body + then_result.code;

			code.loop(hir::Block {std::make_shared<hir::Code>(loop_body)});

			return {code, then_opnd};
		}
		case NodeType::BREAK: assert(false);
		case NodeType::CONTINUE: assert(false);
		case NodeType::ASS: {
			hir::Code code {};

			auto place_idx = node[0];
			auto value_idx = node[1];

			auto base_typ = checker.node_to_type.at(place_idx);
			assert(checker.is_ref(base_typ));

			auto value_result = compile(value_idx, handlers, scope_id);
			code = code + value_result.code;

			if (is_simple_path(ast.at(place_idx)[0])) {
				auto place_result = compile(place_idx, handlers, scope_id);
				code = code + place_result.code;
				code.copy(place_result.result_register, value_result.result_register);
			} else {
				hir::Register aggregate;
				std::vector<hir::Operand> indexes;
				auto indexes_code = find_aggregate_indexes(
					place_idx, aggregate, indexes, handlers, scope_id
				);
				code = code + indexes_code;
				code.set_element(aggregate, indexes, value_result.result_register);
			}

			return {code, value_result.result_register};
		}

#define BINARY_ARITH(OPCODE)                                    \
	{                                                             \
		hir::Code code {};                                          \
		auto left_idx = node[0];                                    \
		auto right_idx = node[1];                                   \
                                                                \
		auto left_result = compile(left_idx, handlers, scope_id);   \
		auto right_result = compile(right_idx, handlers, scope_id); \
		code = code + left_result.code + right_result.code;         \
		auto result_register = make_register();                     \
		code.instructions.push_back(hir::Instruction {              \
			hir::Opcode::OPCODE,                                      \
			{result_register,                                         \
		   left_result.result_register,                             \
		   right_result.result_register}                            \
		});                                                         \
		return {code, result_register};                             \
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
			auto exp_result = compile(node[0], handlers, scope_id);
			code = code + exp_result.code;
			code.instructions.push_back(hir::Instruction {
				hir::Opcode::NOT, {result_register, exp_result.result_register}
			});
			return {code, result_register};
		}
		case NodeType::ID: {
			hir::Code code {};
			auto variable_ptr = env.find(scope_id, node.str_id);
			if (variable_ptr == nullptr) {
				err("Variable not previously declared");
				assert(false);
			}
			assert(variable_ptr->kind == hir::Operand::Kind::REGISTER);
			auto variable_register = variable_ptr->registuhr;
			return Result {code, variable_register};
		}
		case NodeType::STR: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_string = hir::String {node.str_id};
			code.copy(result_register, literal_string);
			return Result {code, result_register};
		}
		case NodeType::VAR_DECL: {
			hir::Code code {};

			auto id_idx = node[0];
			auto opt_type_idx = node[1];
			auto exp_idx = node[2];

			(void)opt_type_idx;

			const auto& id_node = ast.at(id_idx);

			auto initial_result = compile(exp_idx, handlers, scope_id);
			code = code + initial_result.code;

			auto variable_register = make_register();

			env.insert(scope_id, id_node.str_id, variable_register);

			code.copy(variable_register, initial_result.result_register);

			return Result {code, variable_register};
		}
		case NodeType::FUN_DECL: {
			hir::Code code {};

			auto id_idx = node[0];
			auto params_idx = node[1];
			auto opt_type_idx = node[2];
			auto body_idx = node[3];

			const auto& id_node = ast.at(id_idx);
			const auto& params_node = ast.at(params_idx);

			// FIXME: do something with this
			(void)opt_type_idx;

			auto variable_register = make_register();
			env.insert(scope_id, id_node.str_id, variable_register);

			auto inner_scope_id = env.create_child_scope(scope_id);

			std::vector<hir::Register> parameter_registers {};

			for (auto param_id : params_node) {
				const auto& param_node = ast.at(param_id);
				auto param_register = make_register();
				parameter_registers.push_back(param_register);
				env.insert(inner_scope_id, param_node.str_id, param_register);
			}

			auto body_result = compile(body_idx, handlers, inner_scope_id);

			body_result.code.instructions.push_back(
				hir::Instruction {hir::Opcode::RET, {body_result.result_register}}
			);

			hir::Block block {std::make_shared<hir::Code>(body_result.code)};
			hir::Function function {parameter_registers, block, false, {0}};

			code.copy(variable_register, function);

			return {code, variable_register};
		}
		case NodeType::NIL: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_nil = hir::Nil {};
			code.copy(result_register, literal_nil);
			return Result {code, result_register};
		}
		case NodeType::TRUE: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_true = hir::Boolean {true};
			code.copy(result_register, literal_true);
			return Result {code, result_register};
		}
		case NodeType::FALSE: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_false = hir::Boolean {false};
			code.copy(result_register, literal_false);
			return Result {code, result_register};
		}
		case NodeType::LET: {
			hir::Code code {};

			auto decls_idx = node[0];
			auto exp_idx = node[1];

			const auto& decls_node = ast.at(decls_idx);

			auto inner_scope_id = env.create_child_scope(scope_id);
			for (auto idx : decls_node) {
				auto initial_result = compile(idx, handlers, inner_scope_id);
				code = code + initial_result.code;
			}
			auto expression_result = compile(exp_idx, handlers, inner_scope_id);
			code = code + expression_result.code;
			return {code, expression_result.result_register};
		}
		case NodeType::CHAR: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_character = hir::Character {node.character};
			code.copy(result_register, literal_character);
			return Result {code, result_register};
		}
		case NodeType::PATH: {
			auto place_idx = node[0];

			if (is_simple_path(place_idx)) {
				return compile(place_idx, handlers, scope_id);
			} else {
				hir::Code code {};
				hir::Register aggregate;
				std::vector<hir::Operand> indexes;
				auto result_register = make_register();
				auto indexes_code = find_aggregate_indexes(
					place_idx, aggregate, indexes, handlers, scope_id
				);
				code = code + indexes_code;
				code.get_element(result_register, aggregate, indexes);
				return {code, result_register};
			}
		}
		case NodeType::INT_TYPE: assert(false);
		case NodeType::UINT_TYPE: assert(false);
		case NodeType::BOOL_TYPE: assert(false);
		case NodeType::NIL_TYPE: assert(false);
		case NodeType::AS: {
			return compile(node[0], handlers, scope_id);
		}
	}
	assert(false);
}

hir::Register Compiler::make_register() {
	return hir::Register {register_count++, true};
}
} // namespace hir_compiler
