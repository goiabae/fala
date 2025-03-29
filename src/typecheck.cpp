#include "typecheck.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>

#include "ast.hpp"
#include "str_pool.h"
#include "type.hpp"

// whether PTR points to an instance fo class CLS
#define IS(PTR, CLS) (dynamic_cast<CLS*>(PTR) != nullptr)

Type* typecheck(
	Typechecker& checker, AST& ast, NodeIndex node_idx,
	Env<Type*>::ScopeID scope_id
);

static void err(Location loc, const char* msg) {
	fprintf(
		stderr,
		"TYPECHECK ERROR at line %d, from %d to %d: %s\n",
		loc.begin.line + 1,
		loc.begin.column + 1,
		loc.end.column + 1,
		msg
	);
	exit(1);
}

static void type_mismatch_err(
	Location loc, const char* msg, Type* got, Type* expected
) {
	fprintf(
		stderr,
		"TYPE ERROR(%d, %d-%d): %s. Expected type ",
		loc.begin.line + 1,
		loc.begin.column + 1,
		loc.end.column + 1,
		msg
	);
	print_type(stderr, expected);
	fprintf(stderr, " but got ");
	print_type(stderr, got);
	fprintf(stderr, " instead.\n");
}

void print_type(FILE* fd, Type* t) {
	if (IS(t, Integer)) {
		auto i = dynamic_cast<Integer*>(t);
		if (i->sign == SIGNED)
			fprintf(fd, "Int %d", i->bit_count);
		else
			fprintf(fd, "UInt %d", i->bit_count);
	} else if (IS(t, Nil)) {
		fprintf(fd, "Nil");
	} else if (IS(t, Bool)) {
		fprintf(fd, "Bool");
	} else if (IS(t, Void)) {
		fprintf(fd, "Void");
	} else if (IS(t, Function)) {
		auto f = dynamic_cast<Function*>(t);
		fprintf(fd, "[");
		for (size_t i = 0; i < f->inputs.size() - 1; i++) {
			print_type(fd, f->inputs[i]);
			fprintf(fd, ", ");
		}
		print_type(fd, f->inputs[f->inputs.size() - 1]);
		fprintf(fd, "]");
		fprintf(fd, " -> ");
		print_type(fd, f->output);
	} else if (IS(t, TypeVariable)) {
		auto v = dynamic_cast<TypeVariable*>(t);
		fprintf(fd, "t%zu", v->name);
	} else if (IS(t, Array)) {
		auto a = dynamic_cast<Array*>(t);
		fprintf(fd, "[: ");
		print_type(fd, a->item_type);
		fprintf(fd, " ]");
	}
}

Typechecker typecheck(AST& ast, StringPool& pool) {
	Typechecker checker {pool};
	auto scope_id = checker.env.root_scope_id;

	{
		Type* typ = new Function({new Nil()}, new Integer(8, UNSIGNED));
		*checker.env.insert(scope_id, pool.intern(strdup("read"))) = typ;
	}

	{
		Type* typ = new Function({new Nil()}, new Integer(64, SIGNED));
		*checker.env.insert(scope_id, pool.intern(strdup("read_int"))) = typ;
	}

	{
		Type* typ = new Function({new Integer(8, UNSIGNED)}, new Nil());
		*checker.env.insert(scope_id, pool.intern(strdup("write"))) = typ;
	}

	{
		Type* typ = new Function({new Integer(64, SIGNED)}, new Nil());
		*checker.env.insert(scope_id, pool.intern(strdup("write_int"))) = typ;
	}

	{
		Type* typ = new Function({new Array(new Integer(8, UNSIGNED))}, new Nil());
		*checker.env.insert(scope_id, pool.intern(strdup("write_str"))) = typ;
	}

	{
		Type* typ = new Function(
			{new Integer(64, SIGNED)}, new Array(new Integer(64, SIGNED))
		);
		*checker.env.insert(scope_id, pool.intern(strdup("make_array"))) = typ;
	}

	typecheck(checker, ast, ast.root_index, scope_id);
	return checker;
}

#define ASSOC_TYPE(NODE, TYP)         \
	[&]() {                             \
		Type* typ = TYP;                  \
		checker.node_to_type[NODE] = typ; \
		return typ;                       \
	}()

Type* typecheck(
	Typechecker& checker, AST& ast, NodeIndex node_idx,
	Env<Type*>::ScopeID scope_id
) {
	checker.node_to_scope_id[node_idx] = scope_id;
	const auto& node = ast.at(node_idx);
	switch (node.type) {
		case NodeType::EMPTY: {
			return ASSOC_TYPE(node_idx, checker.add_type(new Void()));
		}
		case NodeType::APP: {
			auto func_idx = node[0];

			const auto& args = ast.at(node[1]);

			vector<Type*> inputs {};

			for (size_t i = 0; i < args.branch.children_count; i++)
				inputs.push_back(typecheck(checker, ast, args[i], scope_id));

			Type* expected_func_type = new Function(inputs, checker.make_var());
			Type* func_type = typecheck(checker, ast, func_idx, scope_id);

			if (!equiv(func_type, expected_func_type)) {
				type_mismatch_err(
					node.loc,
					"Function and arguments don't match",
					func_type,
					expected_func_type
				);
			}

			return ASSOC_TYPE(node_idx, ((Function*)func_type)->output);
		}
		case NodeType::NUM: {
			return ASSOC_TYPE(node_idx, checker.add_type(new Integer(64, SIGNED)));
		}
		case NodeType::BLK: {
			for (size_t i = 0; i < node.branch.children_count - 1; i++)
				typecheck(checker, ast, node[i], scope_id);
			return ASSOC_TYPE(
				node_idx,
				typecheck(checker, ast, node[node.branch.children_count - 1], scope_id)
			);
		}
		case NodeType::IF: {
			const auto t = typecheck(checker, ast, node[1], scope_id);
			const auto f = typecheck(checker, ast, node[2], scope_id);
			if (!equiv(t, f))
				err(
					node.loc,
					"If expression has \"then\" and \"else\" branches with different "
					"types"
				);
			return ASSOC_TYPE(node_idx, t);
		}
		case NodeType::WHEN: {
			auto tc = typecheck(checker, ast, node[0], scope_id);
			if (!equiv(tc, checker.add_type(new Bool())))
				err(
					node.loc,
					"Condition expression of when expression is not of type boolean"
				);
			typecheck(checker, ast, node[1], scope_id);
			return ASSOC_TYPE(node_idx, checker.add_type(new Nil()));
		}
		case NodeType::FOR: {
			auto decl_idx = node[0];
			auto to_idx = node[1];
			auto step_idx = node[2];
			auto then_idx = node[3];

			auto new_scope_id = checker.env.create_child_scope(scope_id);

			typecheck(checker, ast, decl_idx, new_scope_id);
			typecheck(checker, ast, to_idx, new_scope_id);

			if (ast.at(step_idx).type != NodeType::EMPTY)
				typecheck(checker, ast, step_idx, new_scope_id);

			return ASSOC_TYPE(
				node_idx, typecheck(checker, ast, then_idx, new_scope_id)
			);
		}
		case NodeType::WHILE: {
			typecheck(checker, ast, node[0], scope_id);
			return ASSOC_TYPE(node_idx, typecheck(checker, ast, node[1], scope_id));
		}
		case NodeType::BREAK:
		case NodeType::CONTINUE:
			return ASSOC_TYPE(node_idx, typecheck(checker, ast, node[0], scope_id));
		case NodeType::ASS: {
			auto path = typecheck(checker, ast, node[0], scope_id);
			auto val = typecheck(checker, ast, node[1], scope_id);
			if (!equiv(path, val))
				err(node.loc, "Assignment with value of wrong type");
			return ASSOC_TYPE(node_idx, val);
		}
		case NodeType::EQ: {
			const auto left = typecheck(checker, ast, node[0], scope_id);
			const auto right = typecheck(checker, ast, node[1], scope_id);
			if (!equiv(left, right))
				err(
					node.loc,
					"Equality comparison of values of different types is always false"
				);
			return ASSOC_TYPE(node_idx, checker.add_type(new Bool()));
		}
		case NodeType::OR:
		case NodeType::AND: {
			const auto left = typecheck(checker, ast, node[0], scope_id);
			const auto right = typecheck(checker, ast, node[1], scope_id);
			if (!(equiv(left, checker.add_type(new Bool()))
			      && equiv(right, checker.add_type(new Bool()))))
				err(node.loc, "Logical combinator arguments must be of boolean type");
			return ASSOC_TYPE(node_idx, checker.add_type(new Bool()));
		}
		case NodeType::GTN:
		case NodeType::LTN:
		case NodeType::GTE:
		case NodeType::LTE: {
			const auto left = typecheck(checker, ast, node[0], scope_id);
			const auto right = typecheck(checker, ast, node[1], scope_id);
			if (!(equiv(left, checker.add_type(new Integer(64, SIGNED)))
			      && equiv(right, checker.add_type(new Integer(64, SIGNED)))))
				err(node.loc, "Comparison operator arguments must be of numeric type");
			return ASSOC_TYPE(node_idx, checker.add_type(new Bool()));
		}
		case NodeType::ADD:
		case NodeType::SUB:
		case NodeType::MUL:
		case NodeType::DIV:
		case NodeType::MOD: {
			const auto left = typecheck(checker, ast, node[0], scope_id);
			const auto right = typecheck(checker, ast, node[1], scope_id);
			const auto num = checker.add_type(new Integer(64, SIGNED));
			const auto& left_node = ast.at(node[0]);
			const auto& right_node = ast.at(node[1]);
			if (!equiv(left, num))
				err(left_node.loc, "Left-hand side of operator is not numeric");
			if (!equiv(right, num))
				err(
					right_node.loc,
					"Arithmetic operator arguments must be of numeric type"
				);
			return ASSOC_TYPE(node_idx, checker.add_type(new Integer(64, SIGNED)));
		}
		case NodeType::AT: {
			auto index = typecheck(checker, ast, node[1], scope_id);
			auto inti = checker.add_type(new Integer(64, SIGNED));
			if (!equiv(index, inti)) {
				type_mismatch_err(
					node.loc, "Index expression must be of integer type", index, inti
				);
			}

			auto arr = typecheck(checker, ast, node[0], scope_id);
			if (not IS(arr, Array))
				err(node.loc, "Cannot index expression which is not of array type");

			return ASSOC_TYPE(node_idx, ((Array*)arr)->item_type);
		}
		case NodeType::NOT: {
			auto exp = typecheck(checker, ast, node[0], scope_id);
			auto bull = checker.add_type(new Bool());
			if (!equiv(exp, bull)) err(node.loc, "Expression is not of type boolean");
			return ASSOC_TYPE(node_idx, bull);
		}
		case NodeType::ID: {
			auto typ = checker.env.find(scope_id, node.str_id);
			if (typ == nullptr) err(node.loc, "Variable not previously declared");
			auto typ_ = *typ;
			return ASSOC_TYPE(node_idx, typ_);
		}
		case NodeType::STR:
			return checker.add_type(
				new Array(checker.add_type(new Integer(8, UNSIGNED)))
			);
		case NodeType::DECL: {
			// "var" id opt-type "=" exp
			if (node.branch.children_count == 3) {
				auto id_idx = node[0];
				auto opt_type_idx = node[1];
				auto exp_idx = node[2];

				const auto& id_node = ast.at(id_idx);
				const auto& opt_type_node = ast.at(opt_type_idx);

				Type* exp = typecheck(checker, ast, exp_idx, scope_id);

				if (opt_type_node.type != NodeType::EMPTY) {
					Type* annot = typecheck(checker, ast, opt_type_idx, scope_id);
					if (!equiv(annot, exp))
						type_mismatch_err(
							node.loc,
							"Expression does not have type described in the annotation",
							exp,
							annot
						);
				}

				checker.env.insert(scope_id, id_node.str_id, exp);
			}
			// "fun" id params opt-type "=" exp
			else if (node.branch.children_count == 4) {
				auto opt_type_idx = node[2];
				auto body_idx = node[3];

				const auto& id_node = ast.at(node[0]);
				const auto& params_node = ast.at(node[1]);
				const auto& opt_type_node = ast.at(opt_type_idx);

				// Start with parameters and output types and variables (or concrete
				// type, if provided)
				//   [t1, ..., tn] -> t
				Function* typ = (Function*)[&]() {
					vector<Type*> inputs {};
					for (size_t i = 0; i < params_node.branch.children_count; i++)
						inputs.push_back(checker.make_var());
					Type* output = nullptr;
					if (opt_type_node.type != NodeType::EMPTY)
						output = typecheck(checker, ast, opt_type_idx, scope_id);
					else
						output = checker.make_var();
					return ASSOC_TYPE(
						node_idx, checker.add_type(new Function(inputs, output))
					);
				}
				();

				Type** var = checker.env.insert(scope_id, id_node.str_id, typ);

				{
					auto new_scope_id = checker.env.create_child_scope(scope_id);

					vector<Type*> param_types {};

					for (size_t i = 0; i < params_node.branch.children_count; i++) {
						const auto& param = ast.at(params_node[i]);
						Type* var = checker.make_var();
						param_types.push_back(var);
						checker.env.insert(new_scope_id, param.str_id, var);
					}

					Type* output = [&]() {
						auto body_type = typecheck(checker, ast, body_idx, new_scope_id);
						if (opt_type_node.type != NodeType::EMPTY) {
							auto opt_type =
								typecheck(checker, ast, opt_type_idx, new_scope_id);
							if (!equiv(body_type, opt_type))
								type_mismatch_err(
									node.loc,
									"Function annotation output type and infered type don't "
									"match",
									body_type,
									opt_type
								);
							return opt_type;
						}
						return body_type;
					}();

					typ = (Function*)checker.add_type(new Function(param_types, output));
				}

				*var = typ;

				auto typ_ = typ;
				return ASSOC_TYPE(node_idx, typ_);
			}

			return ASSOC_TYPE(node_idx, checker.add_type(new Nil()));
		}
		case NodeType::NIL:
			return ASSOC_TYPE(node_idx, checker.add_type(new Nil()));
		case NodeType::TRUE:
			return ASSOC_TYPE(node_idx, checker.add_type(new Bool()));
		case NodeType::FALSE:
			return ASSOC_TYPE(node_idx, checker.add_type(new Bool()));
		case NodeType::LET: {
			auto decls_idx = node[0];
			auto exp_idx = node[1];

			const auto& decls = ast.at(decls_idx);

			auto new_scope_id = checker.env.create_child_scope(scope_id);

			for (size_t i = 0; i < decls.branch.children_count; i++)
				typecheck(checker, ast, decls[i], new_scope_id);

			return ASSOC_TYPE(
				node_idx, typecheck(checker, ast, exp_idx, new_scope_id)
			);
		}
		case NodeType::CHAR:
			return ASSOC_TYPE(node_idx, checker.add_type(new Integer(8, UNSIGNED)));
		case NodeType::PATH:
			return ASSOC_TYPE(node_idx, typecheck(checker, ast, node[0], scope_id));
		case NodeType::AS: {
			auto exp = typecheck(checker, ast, node[0], scope_id);
			auto typ_ = typecheck(checker, ast, node[1], scope_id);

			if (IS(exp, Integer) and IS(typ_, Integer)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else if (IS(exp, Bool) and IS(typ_, Integer)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else if (equiv(exp, typ_)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else {
				type_mismatch_err(node.loc, "Can't cast value to type", typ_, exp);
			}

			break;
		}
		case NodeType::PRIMITIVE_TYPE: {
			const auto& constructor_keyword_node = ast.at(node[0]);
			switch (constructor_keyword_node.num) {
				case 0: {
					const auto& type_size_node = ast.at(node[1]);
					return ASSOC_TYPE(
						node_idx, checker.add_type(new Integer(type_size_node.num, SIGNED))
					);
				}
				case 1: {
					const auto& type_size_node = ast.at(node[1]);
					return ASSOC_TYPE(
						node_idx,
						checker.add_type(new Integer(type_size_node.num, UNSIGNED))
					);
				}
				case 2: return ASSOC_TYPE(node_idx, checker.add_type(new Bool()));
				case 3: return ASSOC_TYPE(node_idx, checker.add_type(new Nil()));
			}
		}
	}

	assert(false && "unreachable");
}
