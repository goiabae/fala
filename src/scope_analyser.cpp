#include "scope_analyser.hpp"

#include <cassert>

SymbolTable ScopeAnalyser::analyse() {
	m_symtab.define_builtin(m_pool.intern("read_int"));
	m_symtab.define_builtin(m_pool.intern("read_char"));
	m_symtab.define_builtin(m_pool.intern("write_int"));
	m_symtab.define_builtin(m_pool.intern("write_char"));
	m_symtab.define_builtin(m_pool.intern("write_str"));
	m_symtab.define_builtin(m_pool.intern("make_array"));
	analyse(m_ast.root_index, m_symtab.root_scope);
	return m_symtab;
}

void ScopeAnalyser::analyse(NodeIndex node_idx, ScopeIndex scope_idx) {
	const auto& node = m_ast.at(node_idx);
	switch (node.type) {
		case NodeType::EMPTY: assert(false);
		case NodeType::APP: {
			const auto func_idx = node[0];
			const auto args_idx = node[1];
			const auto& args_node = m_ast.at(args_idx);
			analyse(func_idx, scope_idx);
			for (const auto arg_idx : args_node) analyse(arg_idx, scope_idx);
			return;
		}
		case NodeType::NUM: return;
		case NodeType::BLK: {
			const auto inner_scope_idx = m_symtab.make_child_scope(scope_idx);
			for (const auto exp_idx : node) analyse(exp_idx, inner_scope_idx);
			return;
		}
		case NodeType::IF: {
			const auto cond_idx = node[0];
			const auto then_idx = node[1];
			const auto else_idx = node[2];
			analyse(cond_idx, scope_idx);
			analyse(then_idx, scope_idx);
			analyse(else_idx, scope_idx);
			return;
		}
		case NodeType::WHEN: {
			const auto cond_idx = node[0];
			const auto then_idx = node[1];
			analyse(cond_idx, scope_idx);
			analyse(then_idx, scope_idx);
			return;
		}
		case NodeType::FOR: {
			const auto decl_idx = node[0];
			const auto to_idx = node[1];
			const auto step_idx = node[2];
			const auto then_idx = node[3];
			const auto inner_scope_idx = m_symtab.make_child_scope(scope_idx);
			analyse(decl_idx, inner_scope_idx);
			analyse(to_idx, inner_scope_idx);
			const auto& step_node = m_ast.at(step_idx);
			if (step_node.type != NodeType::EMPTY) analyse(step_idx, scope_idx);
			analyse(then_idx, inner_scope_idx);
			return;
		}
		case NodeType::WHILE: {
			analyse(node[0], scope_idx);
			analyse(node[1], scope_idx);
			return;
		}
		case NodeType::BREAK: {
			analyse(node[0], scope_idx);
			return;
		}
		case NodeType::CONTINUE: {
			analyse(node[0], scope_idx);
			return;
		}
#define BINOP                    \
	{                              \
		analyse(node[0], scope_idx); \
		analyse(node[1], scope_idx); \
		return;                      \
	}
		case NodeType::ASS: BINOP;
		case NodeType::AT: BINOP;
		case NodeType::OR: BINOP;
		case NodeType::AND: BINOP;
		case NodeType::GTN: BINOP;
		case NodeType::LTN: BINOP;
		case NodeType::GTE: BINOP;
		case NodeType::LTE: BINOP;
		case NodeType::EQ: BINOP;
		case NodeType::ADD: BINOP;
		case NodeType::SUB: BINOP;
		case NodeType::MUL: BINOP;
		case NodeType::DIV: BINOP;
		case NodeType::MOD: BINOP;
#undef BINOP
#define UNOP                     \
	{                              \
		analyse(node[0], scope_idx); \
		return;                      \
	}
		case NodeType::NOT: UNOP;
		case NodeType::AS: UNOP;
		case NodeType::PATH: UNOP;
		case NodeType::ID: {
			[[maybe_unused]]
			const auto guid = m_symtab.use_of(node.str_id, scope_idx, node_idx);
			auto& m = m_ast.at(node_idx);
			m.type = NodeType::GUID;
			m.guid = guid;
			return;
		}
		case NodeType::STR: return;
		case NodeType::VAR_DECL: {
			const auto id_idx = node[0];
			const auto exp_idx = node[2];
			const auto& id_node = m_ast.at(id_idx);
			assert(id_node.type == NodeType::ID);
			const auto guid = m_symtab.define(id_node.str_id, scope_idx, id_idx);
			auto& m = m_ast.at(id_idx);
			m.type = NodeType::GUID;
			m.guid = guid;
			analyse(exp_idx, scope_idx);
			return;
		}
		case NodeType::FUN_DECL: {
			const auto id_idx = node[0];
			const auto& id_node = m_ast.at(id_idx);
			assert(id_node.type == NodeType::ID);
			const auto params_idx = node[1];
			const auto body_idx = node[3];
			const auto guid = m_symtab.define(id_node.str_id, scope_idx, id_idx);
			auto& m = m_ast.at(id_idx);
			m.type = NodeType::GUID;
			m.guid = guid;
			const auto inner_scope_idx = m_symtab.make_function_scope(scope_idx);
			const auto& params_node = m_ast.at(params_idx);
			for (const auto param_idx : params_node) {
				const auto& param_node = m_ast.at(param_idx);
				assert(param_node.type == NodeType::PARAM);
				const auto& param_id_node = m_ast.at(param_node[1]);
				assert(param_id_node.type == NodeType::ID);
				const auto guid =
					m_symtab.define(param_id_node.str_id, inner_scope_idx, param_idx);
				auto& m = m_ast.at(param_node[1]);
				m.type = NodeType::GUID;
				m.guid = guid;
			}
			analyse(body_idx, inner_scope_idx);
			return;
		}
		case NodeType::NIL: return;
		case NodeType::TRUE: return;
		case NodeType::FALSE: return;
		case NodeType::LET: {
			const auto decls_idx = node[0];
			const auto exp_idx = node[1];
			const auto inner_scope_idx = m_symtab.make_child_scope(scope_idx);
			const auto& decls_node = m_ast.at(decls_idx);
			assert(decls_node.type == NodeType::METALIST);
			for (const auto decl_idx : decls_node) analyse(decl_idx, inner_scope_idx);
			analyse(exp_idx, inner_scope_idx);
			return;
		}
		case NodeType::CHAR: return;
		case NodeType::INSTANCE: assert(false);
		case NodeType::METALIST: assert(false);
		case NodeType::GUID: break;
		case NodeType::MODE: assert(false);
		case NodeType::PARAM: assert(false);
	}
}
