#include "scope_analyser.hpp"

#include <cassert>

void ScopeAnalyser::define_builtin(const char* name) {
	m_symtab.define_builtin(m_pool.intern(name), m_symtab.value_namespace);
}

void ScopeAnalyser::define_builtin_type(const char* name) {
	m_symtab.define_builtin(m_pool.intern(name), m_symtab.type_namespace);
}

SymbolTable ScopeAnalyser::analyse() {
	define_builtin("read_int");
	define_builtin("read_char");
	define_builtin("write_int");
	define_builtin("write_char");
	define_builtin("write_str");
	define_builtin_type("Int");
	define_builtin_type("Uint");
	define_builtin_type("Nil");
	analyse(m_ast.root_index, m_symtab.root_scope);
	return m_symtab;
}

void ScopeAnalyser::analyse_type(NodeIndex node_idx, ScopeIndex scope_idx) {
	const auto& node = m_ast.at(node_idx);
	switch (node.type) {
		case NodeType::EMPTY: {
			return;
		}
		case NodeType::APP: assert(false);
		case NodeType::NUM: {
			return;
		}
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
		case NodeType::AT: assert(false);
		case NodeType::ADD: assert(false);
		case NodeType::SUB: assert(false);
		case NodeType::MUL: assert(false);
		case NodeType::DIV: assert(false);
		case NodeType::MOD: assert(false);
		case NodeType::NOT: assert(false);
		case NodeType::ID: {
			const auto guid = m_symtab.use_of(
				node.str_id, scope_idx, node_idx, m_symtab.type_namespace
			);
			auto& m = m_ast.at(node_idx);
			m.type = NodeType::GUID;
			m.guid = guid;
			return;
		}
		case NodeType::STR: assert(false);
		case NodeType::VAR_DECL: assert(false);
		case NodeType::FUN_DECL: assert(false);
		case NodeType::NIL: assert(false);
		case NodeType::TRUE: assert(false);
		case NodeType::FALSE: assert(false);
		case NodeType::LET: assert(false);
		case NodeType::CHAR: assert(false);
		case NodeType::PATH: assert(false);
		case NodeType::AS: assert(false);
		case NodeType::INSTANCE: {
			analyse_type(node[0], scope_idx);
			analyse_type(node[1], scope_idx);
			return;
		}
		case NodeType::METALIST: {
			for (auto idx : node) {
				analyse_type(idx, scope_idx);
			}
			return;
		}
		case NodeType::GUID: assert(false);
		case NodeType::MODE: assert(false);
		case NodeType::PARAM: assert(false);
		case NodeType::RECORD_LITERAL: assert(false);
		case NodeType::FIELD: assert(false);
		case NodeType::FIELD_ACCESS: assert(false);
		case NodeType::ARRAY_LITERAL: assert(false);
		case NodeType::REPEAT: assert(false);
		case NodeType::LEN: assert(false);
		case NodeType::MODULE_BODY: assert(false);
		case NodeType::ARRAY_TYPE_LITERAL: {
			analyse_type(node[0], scope_idx);
			return;
		}
		case NodeType::RECORD_TYPE_LITERAL: {
			for (auto idx : node) {
				analyse_type(idx, scope_idx);
			}
			return;
		}
		case NodeType::FIELD_TYPE: {
			analyse_type(node[1], scope_idx);
			return;
		}
	}
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
			ScopeIndex scope = scope_idx;
			for (const auto exp_idx : node) {
				const auto& exp_node = m_ast.at(exp_idx);
				if (exp_node.type == NodeType::VAR_DECL) {
					scope = m_symtab.make_child_scope(scope);
				}
				analyse(exp_idx, scope);
			}
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
		case NodeType::AS: {
			analyse(node[0], scope_idx);
			analyse_type(node[1], scope_idx);
			return;
		}
		case NodeType::PATH: UNOP;
		case NodeType::ID: {
			const auto guid = m_symtab.use_of(
				node.str_id, scope_idx, node_idx, m_symtab.value_namespace
			);
			auto& m = m_ast.at(node_idx);
			m.type = NodeType::GUID;
			m.guid = guid;
			return;
		}
		case NodeType::STR: return;
		case NodeType::VAR_DECL: {
			const auto id_idx = node[1];
			const auto opt_type_idx = node[2];
			const auto exp_idx = node[3];
			const auto& id_node = m_ast.at(id_idx);
			assert(id_node.type == NodeType::ID);
			const auto guid = m_symtab.define(
				id_node.str_id, scope_idx, id_idx, m_symtab.value_namespace
			);
			auto& m = m_ast.at(id_idx);
			m.type = NodeType::GUID;
			m.guid = guid;
			analyse_type(opt_type_idx, scope_idx);
			analyse(exp_idx, scope_idx);
			return;
		}
		case NodeType::FUN_DECL: {
			const auto id_idx = node[0];
			const auto& id_node = m_ast.at(id_idx);
			assert(id_node.type == NodeType::ID);
			const auto params_idx = node[1];
			const auto opt_type_idx = node[2];
			const auto body_idx = node[3];
			const auto guid = m_symtab.define(
				id_node.str_id, scope_idx, id_idx, m_symtab.value_namespace
			);
			auto& m = m_ast.at(id_idx);
			m.type = NodeType::GUID;
			m.guid = guid;
			const auto inner_scope_idx =
				m_symtab.make_function_scope(scope_idx, body_idx);
			const auto& params_node = m_ast.at(params_idx);
			for (const auto param_idx : params_node) {
				const auto& param_node = m_ast.at(param_idx);
				assert(param_node.type == NodeType::PARAM);
				const auto& param_id_node = m_ast.at(param_node[1]);
				assert(param_id_node.type == NodeType::ID);
				analyse_type(param_node[2], inner_scope_idx);
				const auto guid = m_symtab.define(
					param_id_node.str_id,
					inner_scope_idx,
					param_node[1],
					m_symtab.value_namespace
				);
				auto& m = m_ast.at(param_node[1]);
				m.type = NodeType::GUID;
				m.guid = guid;
			}
			analyse_type(opt_type_idx, inner_scope_idx);
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
		case NodeType::RECORD_LITERAL: {
			for (auto field_idx : node) analyse(field_idx, scope_idx);
			return;
		}
		case NodeType::FIELD: {
			analyse(node[1], scope_idx);
			return;
		}
		case NodeType::FIELD_ACCESS: {
			analyse(node[0], scope_idx);
			return;
		}
		case NodeType::ARRAY_LITERAL: {
			for (auto elt_idx : node) analyse(elt_idx, scope_idx);
			return;
		}
		case NodeType::REPEAT: {
			analyse(node[0], scope_idx);
			analyse(node[1], scope_idx);
			return;
		}
		case NodeType::LEN: {
			analyse(node[0], scope_idx);
			return;
		}
		case NodeType::MODULE_BODY: {
			const auto& decl_list = m_ast.at(node[0]);
			for (auto decl : decl_list) analyse(decl, scope_idx);
			return;
		}
		case NodeType::ARRAY_TYPE_LITERAL: assert(false);
		case NodeType::RECORD_TYPE_LITERAL: assert(false);
		case NodeType::FIELD_TYPE: assert(false);
	}
}
