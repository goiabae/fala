#include "typecheck.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>

#include "ast.h"
#include "str_pool.h"
#include "type.hpp"

Type* typecheck(Typechecker& checker, const Node& node);

static void err(Location loc, const char* msg) {
	fprintf(
		stderr,
		"TYPECHECK ERROR at line %d, from %d to %d: %s\n",
		loc.first_line + 1,
		loc.first_column + 1,
		loc.last_column + 1,
		msg
	);
	exit(1);
}

void print_type(FILE* fd, Type* t);

static void type_mismatch_err(
	Location loc, const char* msg, Type* got, Type* expected
) {
	fprintf(
		stderr,
		"TYPE ERROR(%d, %d-%d): %s. Expected type ",
		loc.first_line + 1,
		loc.first_column + 1,
		loc.last_column + 1,
		msg
	);
	print_type(stderr, expected);
	fprintf(stderr, " but got ");
	print_type(stderr, got);
	fprintf(stderr, " instead.\n");
}

void print_type(FILE* fd, Type* t) {
	if (dynamic_cast<Integer*>(t) != nullptr) {
		auto i = dynamic_cast<Integer*>(t);
		if (i->sign == SIGNED)
			fprintf(fd, "Int %d", i->bit_count);
		else
			fprintf(fd, "UInt %d", i->bit_count);
	} else if (dynamic_cast<Nil*>(t) != nullptr) {
		fprintf(fd, "Nil");
	} else if (dynamic_cast<Bool*>(t) != nullptr) {
		fprintf(fd, "Bool");
	} else if (dynamic_cast<Void*>(t) != nullptr) {
		fprintf(fd, "Void");
	} else if (dynamic_cast<Function*>(t) != nullptr) {
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
	} else if (dynamic_cast<TypeVariable*>(t) != nullptr) {
		auto v = dynamic_cast<TypeVariable*>(t);
		fprintf(fd, "t%zu", v->name);
	} else if (dynamic_cast<Array*>(t) != nullptr) {
		auto a = dynamic_cast<Array*>(t);
		fprintf(fd, "[: ");
		print_type(fd, a->item_type);
		fprintf(fd, " ]");
	}
}

bool typecheck(const AST& ast, StringPool& pool) {
	Typechecker checker {pool};
	auto scope = checker.env.make_scope();

	{
		Type* typ = new Function({new Nil()}, new Integer(8, UNSIGNED));
		*checker.env.insert(pool.intern(strdup("read"))) = typ;
	}

	{
		Type* typ = new Function({new Nil()}, new Integer(64, SIGNED));
		*checker.env.insert(pool.intern(strdup("read_int"))) = typ;
	}

	{
		Type* typ = new Function({new Integer(8, UNSIGNED)}, new Nil());
		*checker.env.insert(pool.intern(strdup("write"))) = typ;
	}

	{
		Type* typ = new Function({new Integer(64, SIGNED)}, new Nil());
		*checker.env.insert(pool.intern(strdup("write_int"))) = typ;
	}

	{
		Type* typ = new Function({new Array(new Integer(8, UNSIGNED))}, new Nil());
		*checker.env.insert(pool.intern(strdup("write_str"))) = typ;
	}

	{
		Type* typ = new Function(
			{new Integer(64, SIGNED)}, new Array(new Integer(64, SIGNED))
		);
		*checker.env.insert(pool.intern(strdup("array"))) = typ;
	}

	typecheck(checker, ast.root);
	return true;
}

Type* typecheck(Typechecker& checker, const Node& node) {
	switch (node.type) {
		case AST_EMPTY: return checker.add_type(new Void());
		case AST_APP: {
			Node& func = node.branch.children[0];
			Node& args = node.branch.children[1];

			vector<Type*> inputs {};

			for (size_t i = 0; i < args.branch.children_count; i++)
				inputs.push_back(typecheck(checker, args.branch.children[i]));

			Type* expected_func_type = new Function(inputs, checker.make_var());
			Type* func_type = typecheck(checker, func);

			if (!equiv(func_type, expected_func_type)) {
				type_mismatch_err(
					node.loc,
					"Function and arguments don't match",
					func_type,
					expected_func_type
				);
			}

			return ((Function*)func_type)->output;
		}
		case AST_NUM: return checker.add_type(new Integer(64, SIGNED));
		case AST_BLK: {
			for (size_t i = 0; i < node.branch.children_count - 1; i++)
				typecheck(checker, node.branch.children[i]);
			return typecheck(
				checker, node.branch.children[node.branch.children_count - 1]
			);
		}
		case AST_IF: {
			const auto t = typecheck(checker, node.branch.children[1]);
			const auto f = typecheck(checker, node.branch.children[2]);
			if (!equiv(t, f))
				err(
					node.branch.children[1].loc,
					"If expression has \"then\" and \"else\" branches with different "
					"types"
				);
			return t;
		}
		case AST_WHEN: {
			auto tc = typecheck(checker, node.branch.children[0]);
			if (!equiv(tc, checker.add_type(new Bool())))
				err(
					node.branch.children[0].loc,
					"Condition expression of when expression is not of type boolean"
				);
			typecheck(checker, node.branch.children[1]);
			return checker.add_type(new Nil());
		}
		case AST_FOR: {
			auto scope = checker.env.make_scope();
			typecheck(checker, node.branch.children[0]);
			typecheck(checker, node.branch.children[1]);
			if (node.branch.children[2].type != AST_EMPTY)
				typecheck(checker, node.branch.children[2]);
			return typecheck(checker, node.branch.children[3]);
		}
		case AST_WHILE: {
			typecheck(checker, node.branch.children[0]);
			return typecheck(checker, node.branch.children[1]);
		}
		case AST_BREAK:
		case AST_CONTINUE: return typecheck(checker, node.branch.children[0]);
		case AST_ASS: {
			auto path = typecheck(checker, node.branch.children[0]);
			auto val = typecheck(checker, node.branch.children[1]);
			if (!equiv(path, val))
				err(node.loc, "Assignment with value of wrong type");
			return val;
		}
		case AST_EQ: {
			const auto left = typecheck(checker, node.branch.children[0]);
			const auto right = typecheck(checker, node.branch.children[1]);
			if (!equiv(left, right))
				err(
					node.branch.children[0].loc,
					"Equality comparison of values of different types is always false"
				);
			return checker.add_type(new Bool());
		}
		case AST_OR:
		case AST_AND: {
			const auto left = typecheck(checker, node.branch.children[0]);
			const auto right = typecheck(checker, node.branch.children[1]);
			if (!(equiv(left, checker.add_type(new Bool()))
			      && equiv(right, checker.add_type(new Bool()))))
				err(
					node.branch.children[0].loc,
					"Logical combinator arguments must be of boolean type"
				);
			return checker.add_type(new Bool());
		}
		case AST_GTN:
		case AST_LTN:
		case AST_GTE:
		case AST_LTE: {
			const auto left = typecheck(checker, node.branch.children[0]);
			const auto right = typecheck(checker, node.branch.children[1]);
			if (!(equiv(left, checker.add_type(new Integer(64, SIGNED)))
			      && equiv(right, checker.add_type(new Integer(64, SIGNED)))))
				err(
					node.branch.children[0].loc,
					"Comparison operator arguments must be of numeric type"
				);
			return checker.add_type(new Bool());
		}
		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		case AST_MOD: {
			const auto left = typecheck(checker, node.branch.children[0]);
			const auto right = typecheck(checker, node.branch.children[1]);
			const auto num = checker.add_type(new Integer(64, SIGNED));
			if (!equiv(left, num))
				err(
					node.branch.children[0].loc,
					"Left-hand side of operator is not numeric"
				);
			if (!equiv(right, num))
				err(
					node.branch.children[1].loc,
					"Arithmetic operator arguments must be of numeric type"
				);
			return checker.add_type(new Integer(64, SIGNED));
		}
		case AST_AT: {
			auto index = typecheck(checker, node.branch.children[1]);
			auto inti = checker.add_type(new Integer(64, SIGNED));
			if (!equiv(index, inti)) {
				type_mismatch_err(
					node.loc, "Index expression must be of integer type", index, inti
				);
			}

			auto arr = typecheck(checker, node.branch.children[0]);
			if (dynamic_cast<Array*>(arr) == nullptr)
				err(node.loc, "Cannot index expression which is not of array type");

			return ((Array*)arr)->item_type;
		}
		case AST_NOT: {
			auto exp = typecheck(checker, node.branch.children[0]);
			auto bull = checker.add_type(new Bool());
			if (!equiv(exp, bull)) err(node.loc, "Expression is not of type boolean");
			return bull;
		}
		case AST_ID: {
			auto typ = checker.env.find(node.str_id);
			if (typ == nullptr) err(node.loc, "Variable not previously declared");
			return *typ;
		}
		case AST_STR:
			return checker.add_type(
				new Array(checker.add_type(new Integer(8, UNSIGNED)))
			);
		case AST_DECL: {
			// "var" id opt-type "=" exp
			if (node.branch.children_count == 3) {
				const auto& id_node = node.branch.children[0];
				const auto& opt_type = node.branch.children[1];
				const auto& exp_node = node.branch.children[2];

				Type* exp = typecheck(checker, exp_node);

				if (opt_type.type != AST_EMPTY) {
					Type* annot = typecheck(checker, opt_type);
					if (!equiv(annot, exp))
						type_mismatch_err(
							node.loc,
							"Expression does not have type described in the annotation",
							exp,
							annot
						);
				}

				checker.env.insert(id_node.str_id, exp);
			}
			// "fun" id params opt-type "=" exp
			else if (node.branch.children_count == 4) {
				const auto& name = node.branch.children[0];
				const auto& params = node.branch.children[1];
				const auto& opt_type_node = node.branch.children[2];
				const auto& body_node = node.branch.children[3];

				// Start with parameters and output types and variables (or concrete
				// type, if provided)
				//   [t1, ..., tn] -> t
				Function* typ = (Function*)[&]() {
					vector<Type*> inputs {};
					for (size_t i = 0; i < params.branch.children_count; i++)
						inputs.push_back(checker.make_var());
					Type* output = nullptr;
					if (opt_type_node.type != AST_EMPTY)
						output = typecheck(checker, opt_type_node);
					else
						output = checker.make_var();
					return checker.add_type(new Function(inputs, output));
				}
				();

				Type** var = checker.env.insert(name.str_id, typ);

				{
					auto scope = checker.env.make_scope();

					vector<Type*> param_types {};

					for (size_t i = 0; i < params.branch.children_count; i++) {
						Type* var = checker.make_var();
						param_types.push_back(var);
						checker.env.insert(params.branch.children[i].str_id, var);
					}

					Type* output = [&]() {
						auto body_type = typecheck(checker, body_node);
						if (opt_type_node.type != AST_EMPTY) {
							auto opt_type = typecheck(checker, opt_type_node);
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

				return typ;
			}

			return checker.add_type(new Nil());
		}
		case AST_NIL: return checker.add_type(new Nil());
		case AST_TRUE: return checker.add_type(new Bool());
		case AST_LET: {
			const Node& decls = node.branch.children[0];
			const Node& exp = node.branch.children[1];

			auto scope = checker.env.make_scope();

			for (size_t i = 0; i < decls.branch.children_count; i++)
				typecheck(checker, decls.branch.children[i]);
			return typecheck(checker, exp);
		}
		case AST_CHAR: return checker.add_type(new Integer(8, UNSIGNED));
		case AST_PATH: return typecheck(checker, node.branch.children[0]);
		case AST_AS: {
			auto exp = typecheck(checker, node.branch.children[0]);
			auto typ = typecheck(checker, node.branch.children[1]);

			if (dynamic_cast<Integer*>(exp) != nullptr and dynamic_cast<Integer*>(typ) != nullptr) {
				return typ;
			} else if (dynamic_cast<Bool*>(exp) != nullptr and dynamic_cast<Integer*>(typ) != nullptr) {
				return typ;
			} else if (equiv(exp, typ)) {
				return typ;
			} else {
				type_mismatch_err(node.loc, "Can't cast value to type", typ, exp);
			}

			break;
		}
		case AST_PRIMITIVE_TYPE: {
			switch (node.branch.children[0].num) {
				case 0:
					return checker.add_type(
						new Integer(node.branch.children[1].num, SIGNED)
					);
				case 1:
					return checker.add_type(
						new Integer(node.branch.children[1].num, UNSIGNED)
					);
				case 2: return checker.add_type(new Bool());
				case 3: return checker.add_type(new Nil());
			}
		}
	}

	assert(false && "unreachable");
}
