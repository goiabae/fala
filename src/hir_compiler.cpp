#include "hir_compiler.hpp"

#include <cstdio>
#include <cstring>
#include <memory>

#include "hir.hpp"

#define err(MSG) fprintf(stderr, "HIR_COMPILER_ERROR: " MSG "\n")

namespace hir_compiler {

constexpr auto builtins = {
	"read_int", "read_char", "write_int", "write_char", "write_str", "make_array"
};

Result Compiler::to_rvalue(Result pointer_result) {
	hir::Code code = pointer_result.code;
	hir::Register result_register = make_register();
	code.load(result_register, pointer_result.result_register);
	return {code, result_register};
}

hir::Code Compiler::compile(AST& ast, const StringPool& pool) {
	auto node_idx = ast.root_index;
	SignalHandlers handlers {};
	auto scope_id = env.root_scope_id;
	auto result = compile(ast, node_idx, pool, handlers, scope_id);
	return result.code;
}

Result Compiler::compile_app(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
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
			code.builtin(
				func_register, hir::Operand {hir::String {func_node.str_id}}
			);
			function = func_register;
			builtin_function_found = true;
			break;
		}
	}

	if (!builtin_function_found) {
		auto func_result = compile(ast, node[0], pool, handlers, scope_id);
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
		auto arg_result = compile(ast, arg_idx, pool, handlers, scope_id);
		code = code + arg_result.code;
		args.push_back(hir::Operand {arg_result.result_register});
	}

	code.call(result_register, function, args);

	return Result {code, result_register};
}

Result Compiler::compile_if(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<hir::Operand>::ScopeID scope_id
) {
	hir::Code code {};
	const auto& node = ast.at(node_idx);

	auto cond_idx = node[0];
	auto then_idx = node[1];
	auto else_idx = node[2];

	auto result_register = make_register();

	auto cond_res = compile(ast, cond_idx, pool, handlers, scope_id);
	code = code + cond_res.code;
	auto cond_opnd = cond_res.result_register;
	assert(not cond_opnd.contains_pointer());

	auto then_res = compile(ast, then_idx, pool, handlers, scope_id);
	then_res.code.copy(result_register, then_res.result_register);

	auto else_res = compile(ast, else_idx, pool, handlers, scope_id);
	else_res.code.copy(result_register, else_res.result_register);

	auto if_true_code = std::make_shared<hir::Code>(then_res.code);
	auto if_false_code = std::make_shared<hir::Code>(else_res.code);

	code.if_true(cond_opnd, hir::Block {if_true_code});
	code.if_false(cond_opnd, hir::Block {if_false_code});

	return Result {code, result_register};
}

Result Compiler::get_pointer_for(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<hir::Operand>::ScopeID scope_id
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
			hir::Code code {};

			auto base_idx = node[0];
			auto off_idx = node[1];

			auto base_result =
				get_pointer_for(ast, base_idx, pool, handlers, scope_id);
			auto off_result = compile(ast, off_idx, pool, handlers, scope_id);

			auto pointer_result = make_register();

			code = code + base_result.code + off_result.code;
			code.item_offset(
				pointer_result,
				base_result.result_register,
				off_result.result_register,
				hir::Integer(1)
			);

			return {code, pointer_result};
		}
		case NodeType::ADD: assert(false);
		case NodeType::SUB: assert(false);
		case NodeType::MUL: assert(false);
		case NodeType::DIV: assert(false);
		case NodeType::MOD: assert(false);
		case NodeType::NOT: assert(false);
		case NodeType::ID: {
			hir::Code code {};
			auto variable_ptr = env.find(scope_id, node.str_id);
			if (variable_ptr == nullptr) {
				err("Variable not previously declared");
				assert(false);
			}
			auto variable_register = *variable_ptr;
			assert(variable_register.kind == hir::Operand::Kind::REGISTER);
			auto result_ptr = make_register();
			code.ref_to(result_ptr, variable_register.registuhr);
			return Result {code, result_ptr};
		}
		case NodeType::STR: assert(false);
		case NodeType::DECL: assert(false);
		case NodeType::NIL: assert(false);
		case NodeType::TRUE: assert(false);
		case NodeType::FALSE: assert(false);
		case NodeType::LET: assert(false);
		case NodeType::CHAR: assert(false);
		case NodeType::PATH: {
			return get_pointer_for(ast, node[0], pool, handlers, scope_id);
		}
		case NodeType::PRIMITIVE_TYPE: assert(false);
		case NodeType::AS: assert(false);
	}
	assert(false);
}

Result Compiler::compile(
	AST& ast, NodeIndex node_idx, const StringPool& pool, SignalHandlers handlers,
	Env<hir::Operand>::ScopeID scope_id
) {
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::EMPTY: assert(false && "empty node shouldn't be evaluated");
		case NodeType::APP:
			return compile_app(ast, node_idx, pool, handlers, scope_id);
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
				auto opnd_res = compile(ast, idx, pool, handlers, inner_scope_id);
				code = code + opnd_res.code;
				result_register = opnd_res.result_register;
			}
			return Result {code, result_register};
		}
		case NodeType::IF:
			return compile_if(ast, node_idx, pool, handlers, scope_id);
		case NodeType::WHEN: {
			hir::Code code {};
			const auto& node = ast.at(node_idx);

			auto cond_idx = node[0];
			auto then_idx = node[1];

			auto result_register = make_register();

			auto cond_res = compile(ast, cond_idx, pool, handlers, scope_id);
			code = code + cond_res.code;
			auto cond_opnd = cond_res.result_register;
			assert(not cond_opnd.contains_pointer());

			auto then_res = compile(ast, then_idx, pool, handlers, scope_id);
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

			auto result_register = make_register();

			SignalHandlers new_handlers {
				{}, {}, {}, true, true, handlers.has_return_handler
			};

			hir::Operand step_value {};

			if (step_node.type != NodeType::EMPTY) {
				auto step_result = compile(ast, step_idx, pool, handlers, scope_id);
				code = code + step_result.code;
				step_value = step_result.result_register;
			} else {
				step_value = hir::Integer(1);
			}

			auto inner_scope_id = env.create_child_scope(scope_id);

			auto var_result = compile(ast, decl_idx, pool, handlers, inner_scope_id);
			code = code + var_result.code;
			auto var_opnd = var_result.result_register;

			auto to_result = compile(ast, to_idx, pool, handlers, inner_scope_id);
			code = code + to_result.code;
			auto to_opnd = to_result.result_register;

			auto then_result = compile(ast, then_idx, pool, handlers, inner_scope_id);
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

			auto result_register = make_register();

			SignalHandlers new_handlers {
				{}, {}, {}, true, true, handlers.has_return_handler
			};

			hir::Operand step_value {};

			auto cond_result = compile(ast, cond_idx, pool, handlers, scope_id);
			auto cond_opnd = cond_result.result_register;

			auto then_result = compile(ast, then_idx, pool, handlers, scope_id);
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

			auto pointer_idx = node[0];
			auto value_idx = node[1];

			auto pointer_result =
				get_pointer_for(ast, pointer_idx, pool, handlers, scope_id);
			auto value_result = compile(ast, value_idx, pool, handlers, scope_id);

			code = code + pointer_result.code + value_result.code;

			code.store(pointer_result.result_register, value_result.result_register);

			return {code, value_result.result_register};
		}

#define BINARY_ARITH(OPCODE)                                               \
	{                                                                        \
		hir::Code code {};                                                     \
		auto left_idx = node[0];                                               \
		auto right_idx = node[1];                                              \
                                                                           \
		auto left_result = compile(ast, left_idx, pool, handlers, scope_id);   \
		auto right_result = compile(ast, right_idx, pool, handlers, scope_id); \
		code = code + left_result.code + right_result.code;                    \
		auto result_register = make_register();                                \
		code.instructions.push_back(hir::Instruction {                         \
			hir::Opcode::OPCODE,                                                 \
			{result_register,                                                    \
		   left_result.result_register,                                        \
		   right_result.result_register}                                       \
		});                                                                    \
		return {code, result_register};                                        \
	}

		case NodeType::OR: BINARY_ARITH(OR);
		case NodeType::AND: BINARY_ARITH(AND);
		case NodeType::GTN: BINARY_ARITH(GREATER);
		case NodeType::LTN: BINARY_ARITH(LESS);
		case NodeType::GTE: BINARY_ARITH(GREATER_EQ);
		case NodeType::LTE: BINARY_ARITH(LESS_EQ);
		case NodeType::EQ: BINARY_ARITH(EQ);
		case NodeType::AT: {
			hir::Code code {};

			auto result_register = make_register();

			auto pointer_idx = node[0];
			auto value_idx = node[1];

			auto pointer_result = compile(ast, pointer_idx, pool, handlers, scope_id);
			auto value_result = compile(ast, value_idx, pool, handlers, scope_id);

			code = code + pointer_result.code + value_result.code;

			code.item_offset(
				result_register,
				pointer_result.result_register,
				value_result.result_register,
				hir::Integer(1) // FIXME: assuming size of items is 1
			);

			return to_rvalue({code, result_register});
		}

		case NodeType::ADD: BINARY_ARITH(ADD);
		case NodeType::SUB: BINARY_ARITH(SUB);
		case NodeType::MUL: BINARY_ARITH(MUL);
		case NodeType::DIV: BINARY_ARITH(DIV);
		case NodeType::MOD: BINARY_ARITH(MOD);
		case NodeType::NOT: {
			hir::Code code {};
			auto result_register = make_register();
			auto exp_result = compile(ast, node[0], pool, handlers, scope_id);
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
			auto result_ptr = make_register();
			code.ref_to(result_ptr, variable_register);
			return Result {code, variable_register};
		}
		case NodeType::STR: {
			hir::Code code {};
			auto result_register = make_register();
			auto literal_string = hir::String {node.str_id};
			code.copy(result_register, literal_string);
			return Result {code, result_register};
		}
		case NodeType::DECL: {
			hir::Code code {};

			// function declaration
			if (node.branch.children_count == 4) {
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

				auto body_result =
					compile(ast, body_idx, pool, handlers, inner_scope_id);

				body_result.code.instructions.push_back(
					hir::Instruction {hir::Opcode::RET, {body_result.result_register}}
				);

				hir::Block block {std::make_shared<hir::Code>(body_result.code)};
				hir::Function function {parameter_registers, block, false, {0}};

				code.copy(variable_register, function);

				return {code, variable_register};
			}

			if (node.branch.children_count == 3) {
				auto id_idx = node[0];
				auto opt_type_idx = node[1];
				auto exp_idx = node[2];

				(void)opt_type_idx;

				const auto& id_node = ast.at(id_idx);

				auto initial_result = compile(ast, exp_idx, pool, handlers, scope_id);
				code = code + initial_result.code;

				auto variable_register = make_register();

				env.insert(scope_id, id_node.str_id, variable_register);

				code.copy(variable_register, initial_result.result_register);

				return Result {code, variable_register};
			}

			assert(false);
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
				auto initial_result = compile(ast, idx, pool, handlers, inner_scope_id);
				code = code + initial_result.code;
			}
			auto expression_result =
				compile(ast, exp_idx, pool, handlers, inner_scope_id);
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
		case NodeType::PATH: return compile(ast, node[0], pool, handlers, scope_id);
		case NodeType::PRIMITIVE_TYPE: assert(false);
		case NodeType::AS: assert(false);
	}
	assert(false);
}

hir::Register Compiler::make_register() {
	return hir::Register {register_count++, true};
}

} // namespace hir_compiler
