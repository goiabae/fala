#include "typecheck.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>
#include <memory>

#include "ast.hpp"
#include "str_pool.h"
#include "type.hpp"

// Meta-variables:

// E       The environment
// e       An expression
// x       A variable or name
// n       A literal number
// s       A literal string
// c       A literal character
// t       A type

TYPE Typechecker::make_nil() { return std::shared_ptr<Nil>(new Nil()); }
TYPE Typechecker::make_bool() { return std::shared_ptr<Bool>(new Bool()); }
TYPE Typechecker::make_void() { return std::shared_ptr<Void>(new Void()); }

TYPE Typechecker::make_integer(int bit_count, Sign sign) {
	return std::shared_ptr<Integer>(new Integer(bit_count, sign));
}

TYPE Typechecker::make_array(TYPE item_type) {
	return std::shared_ptr<Array>(new Array(item_type));
}

TYPE Typechecker::make_function(std::vector<TYPE> inputs, TYPE output) {
	return std::shared_ptr<Function>(new Function(inputs, output));
}

TYPE Typechecker::make_typevar() {
	return std::shared_ptr<TypeVariable>(new TypeVariable {next_var_id++});
}

TYPE Typechecker::make_ref(TYPE ref_type) {
	return std::shared_ptr<Ref>(new Ref {ref_type});
}

bool Typechecker::is_nil(TYPE typ) {
	return dynamic_pointer_cast<Nil>(typ) != nullptr;
}
bool Typechecker::is_bool(TYPE typ) {
	return dynamic_pointer_cast<Bool>(typ) != nullptr;
}
bool Typechecker::is_void(TYPE typ) {
	return dynamic_pointer_cast<Void>(typ) != nullptr;
}

bool Typechecker::is_integer(TYPE typ) {
	return dynamic_pointer_cast<Integer>(typ) != nullptr;
}

bool Typechecker::is_array(TYPE typ) {
	return dynamic_pointer_cast<Array>(typ) != nullptr;
}

bool Typechecker::is_function(TYPE typ) {
	return dynamic_pointer_cast<Function>(typ) != nullptr;
}

bool Typechecker::is_typevar(TYPE typ) {
	return dynamic_pointer_cast<TypeVariable>(typ) != nullptr;
}

bool Typechecker::is_ref(TYPE typ) {
	return dynamic_pointer_cast<Ref>(typ) != nullptr;
}

INTEGER Typechecker::to_integer(TYPE typ) {
	return dynamic_pointer_cast<Integer>(typ);
}

ARRAY Typechecker::to_array(TYPE typ) {
	return dynamic_pointer_cast<Array>(typ);
}

FUNCTION Typechecker::to_function(TYPE typ) {
	return dynamic_pointer_cast<Function>(typ);
}

TYPE_VARIABLE Typechecker::to_typevar(TYPE typ) {
	return dynamic_pointer_cast<TypeVariable>(typ);
}

REF Typechecker::to_ref(TYPE typ) { return dynamic_pointer_cast<Ref>(typ); }

TYPE Typechecker::deref(TYPE typ) {
	if (not is_ref(typ))
		return typ;
	else
		return to_ref(typ)->ref_type;
}

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

void Typechecker::mismatch_error(
	Location loc, const char* msg, TYPE got, TYPE expected
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

void Typechecker::print_type(FILE* fd, TYPE t) {
	if (is_integer(t)) {
		auto it = to_integer(t);
		if (it->sign == SIGNED)
			fprintf(fd, "Int<%d>", it->bit_count);
		else
			fprintf(fd, "UInt<%d>", it->bit_count);
	} else if (is_nil(t)) {
		fprintf(fd, "Nil");
	} else if (is_bool(t)) {
		fprintf(fd, "Bool");
	} else if (is_void(t)) {
		fprintf(fd, "Void");
	} else if (is_function(t)) {
		auto ft = to_function(t);
		fprintf(fd, "(");
		for (size_t i = 0; i < ft->inputs.size() - 1; i++) {
			print_type(fd, ft->inputs[i]);
			fprintf(fd, ", ");
		}
		print_type(fd, ft->inputs[ft->inputs.size() - 1]);
		fprintf(fd, ")");
		fprintf(fd, " -> ");
		print_type(fd, ft->output);
	} else if (is_typevar(t)) {
		auto vt = to_typevar(t);
		if (vt->is_bound) {
			fprintf(fd, "(");
			fprintf(fd, "t%zu", vt->unbound_name);
			fprintf(fd, " := ");
			print_type(fd, vt->bound_type);
			fprintf(fd, ")");
		} else {
			fprintf(fd, "t%zu", vt->unbound_name);
		}
	} else if (is_array(t)) {
		auto at = to_array(t);
		fprintf(fd, "Array<");
		print_type(fd, at->item_type);
		fprintf(fd, ">");
	} else if (is_ref(t)) {
		auto rt = to_ref(t);
		fprintf(fd, "&");
		print_type(fd, rt->ref_type);
	}
}

#define BIND_FUNC(NAME, TYP) \
	env.insert(scope_id, pool.intern(strdup(NAME)), TYP)

bool Typechecker::typecheck() {
	auto scope_id = env.root_scope_id;

	auto uint8_typ = make_integer(8, UNSIGNED);
	auto int64_typ = make_integer(64, SIGNED);
	auto nil_typ = make_nil();
	auto uint8_arr_typ = make_array(uint8_typ);
	auto int64_arr_typ = make_array(int64_typ);

	auto nil_to_uint8_typ = make_function({nil_typ}, uint8_typ);
	auto nil_to_int64_typ = make_function({nil_typ}, int64_typ);
	auto uint8_to_nil_typ = make_function({uint8_typ}, nil_typ);
	auto int64_to_nil_typ = make_function({int64_typ}, nil_typ);
	auto uint8_arr_to_nil_typ = make_function({uint8_arr_typ}, nil_typ);
	auto int64_to_int64_arr_typ = make_function({int64_typ}, int64_arr_typ);

	BIND_FUNC("read", nil_to_uint8_typ);
	BIND_FUNC("read_int", nil_to_int64_typ);
	BIND_FUNC("write", uint8_to_nil_typ);
	BIND_FUNC("write_int", int64_to_nil_typ);
	BIND_FUNC("write_str", uint8_arr_to_nil_typ);
	BIND_FUNC("make_array", int64_to_int64_arr_typ);

	typecheck(ast.root_index, scope_id);
	return true;
}

bool Typechecker::unify(TYPE a, TYPE b) {
	// if a is a ref. unify ref a' and b
	if (is_ref(a)) {
		auto ra = to_ref(a);
		return unify(ra->ref_type, b);
	}

	// if b is a ref. unify a and ref b'
	if (is_ref(b)) {
		auto rb = to_ref(b);
		return unify(a, rb->ref_type);
	}

	// a is a bound typevar. unify bound a' and b
	if (is_typevar(a)) {
		auto var_a = to_typevar(a);
		if (var_a->is_bound) {
			return unify(var_a->bound_type, b);
		}
	}

	// b is a bound typevar. unify a and bound b'
	if (is_typevar(b)) {
		auto var_b = to_typevar(b);
		if (var_b->is_bound) {
			return unify(a, var_b->bound_type);
		}
	}

	// a is unbound typevar. bind a to b
	if (is_typevar(a)) {
		auto var_a = to_typevar(a);
		if (not var_a->is_bound) {
			var_a->bind_to(b);
			return true;
		}
	}

	// b is unbound typevar. bind b to a
	if (is_typevar(b)) {
		auto var_b = to_typevar(b);
		if (not var_b->is_bound) {
			var_b->bind_to(a);
			return true;
		}
	}

	if (is_function(a) and is_function(b)) {
		auto fa = to_function(a);
		auto fb = to_function(b);

		if (fa->inputs.size() != fb->inputs.size()) return false;

		for (size_t i = 0; i < fa->inputs.size(); i++) {
			if (not unify(fa->inputs[i], fb->inputs[i])) return false;
		}

		return unify(fa->output, fb->output);
	}

	if (is_array(a) and is_array(b)) {
		auto aa = to_array(a);
		auto ab = to_array(b);

		return unify(aa->item_type, ab->item_type);
	}

	if (is_integer(a) and is_integer(b)) {
		auto ia = to_integer(a);
		auto ib = to_integer(b);

		if (ia->bit_count != ib->bit_count) return false;
		if (ia->sign != ib->sign) return false;

		return true;
	}

	if (is_nil(a) and is_nil(b)) return true;
	if (is_bool(a) and is_bool(b)) return true;
	if (is_void(a) and is_void(b)) return true;

	return false;
}

#define ASSOC_TYPE(NODE, TYP) \
	[&]() {                     \
		TYPE typ = TYP;           \
		node_to_type[NODE] = typ; \
		return typ;               \
	}()

// If an expression has type of lvalue, it can also be used as an rvalue

// | e1 : Ref<t1>
// +------------
// | |- e1 : t1

TYPE Typechecker::typecheck(NodeIndex node_idx, Env<TYPE>::ScopeID scope_id) {
	node_to_scope_id[node_idx] = scope_id;

	auto int64_typ = make_integer(64, SIGNED);
	auto uint8_typ = make_integer(8, UNSIGNED);

	const auto& node = ast.at(node_idx);
	switch (node.type) {
			// |
			// +------
			// | |- Void

		case NodeType::EMPTY: {
			return ASSOC_TYPE(node_idx, make_void());
		}

			// | f : (t1 t2 ... tn) -> t0
			// | a1 : t1
			// | a2 : t2
			// | ...
			// | an : tn
			// +-----------
			// | |- f a1 a2 ... an : t0

		case NodeType::APP: {
			auto func_idx = node[0];
			auto args_idx = node[1];

			const auto& args_node = ast.at(args_idx);

			vector<TYPE> inputs {};
			for (auto arg_idx : args_node) {
				auto arg_typ = typecheck(arg_idx, scope_id);
				inputs.push_back(arg_typ);
			}

			auto expected_func_type = make_function(inputs, make_typevar());
			auto func_type = typecheck(func_idx, scope_id);

			if (not unify(func_type, expected_func_type)) {
				mismatch_error(
					node.loc,
					"Function and arguments don't match",
					func_type,
					expected_func_type
				);
			}

			return ASSOC_TYPE(node_idx, to_function(deref(func_type))->output);
		}
			// |
			// +------
			// | |- n : Int<64>

		case NodeType::NUM: {
			return ASSOC_TYPE(node_idx, make_integer(64, SIGNED));
		}
			// FIXME: add typing rules
		case NodeType::BLK: {
			for (size_t i = 0; i < node.branch.children_count - 1; i++)
				typecheck(node[i], scope_id);
			return ASSOC_TYPE(
				node_idx, typecheck(node[node.branch.children_count - 1], scope_id)
			);
		}

			// | E |- e1 : Bool
			// | E |- e2 : t
			// | E |- e3 : t
			// +----------
			// | E |- if e1 then e2 else e3 : t

		case NodeType::IF: {
			auto cond_idx = node[0];
			auto then_idx = node[1];
			auto else_idx = node[2];

			auto cond_typ = typecheck(cond_idx, scope_id);

			if (not unify(cond_typ, make_bool())) {
				mismatch_error(
					node.loc,
					"Condition expression of if expression is not of type boolean",
					cond_typ,
					make_bool()
				);
			}

			auto then_typ = typecheck(then_idx, scope_id);
			auto else_typ = typecheck(else_idx, scope_id);

			if (not unify(then_typ, else_typ))
				mismatch_error(
					node.loc,
					"If expression has \"then\" and \"else\" branches with different "
					"types",
					then_typ,
					else_typ
				);

			return ASSOC_TYPE(node_idx, then_typ);
		}

			// | e1 : Bool
			// | e2 : t2
			// +---------
			// | |- when e1 then e2 : Nil

		case NodeType::WHEN: {
			auto cond_idx = node[0];
			auto then_idx = node[1];

			auto cond_typ = typecheck(cond_idx, scope_id);
			if (not unify(cond_typ, make_bool()))
				err(
					node.loc,
					"Condition expression of when expression is not of type boolean"
				);

			auto then_typ = typecheck(then_idx, scope_id);
			return ASSOC_TYPE(node_idx, make_nil());
		}

			// Rules for for loop with and without step increment

			// | E |- x : t1
			// | E |- e1 : t1
			// | E |- e2 : t2
			// +------------
			// | E |- for var x1 to e1 then e2 : t2

			// | E |- x : t1
			// | E |- e1 : t1
			// | E |- e2 : t1
			// | E |- e3 : t3
			// +------------
			// | E |- for var x1 to e1 step e2 then e3 : t3

		case NodeType::FOR: {
			auto decl_idx = node[0];
			auto to_idx = node[1];
			auto step_idx = node[2];
			auto then_idx = node[3];

			auto new_scope_id = env.create_child_scope(scope_id);

			auto var_typ = typecheck(decl_idx, new_scope_id);
			auto to_typ = typecheck(to_idx, new_scope_id);

			auto step_typ = (ast.at(step_idx).type == NodeType::EMPTY)
			                ? int64_typ
			                : typecheck(step_idx, new_scope_id);

			if (not unify(var_typ, to_typ)) {
				mismatch_error(
					node.loc,
					"For loop declaration and bound types don't match",
					var_typ,
					to_typ
				);
			}

			if (not unify(to_typ, step_typ)) {
				mismatch_error(
					node.loc,
					"For loop bound and step types don't match",
					to_typ,
					step_typ
				);
			}

			auto then_typ = typecheck(then_idx, new_scope_id);
			return ASSOC_TYPE(node_idx, then_typ);
		}

			// | e1 : Bool
			// | e2 : t2
			// +---------------
			// | |- while e1 then e2 : t2

		case NodeType::WHILE: {
			auto cond_idx = node[0];
			auto then_idx = node[1];

			auto cond_typ = typecheck(cond_idx, scope_id);
			auto bool_typ = make_bool();

			if (not unify(cond_typ, bool_typ)) {
				mismatch_error(
					node.loc,
					"While loop condition must have type boolean",
					cond_typ,
					bool_typ
				);
			}

			auto then_typ = typecheck(then_idx, scope_id);
			return ASSOC_TYPE(node_idx, then_typ);
		}

			// | e1 : t1
			// +----------
			// | |- break e1 : t1

			// | e1 : t1
			// +----------
			// | |- continue e1 : t1

		case NodeType::BREAK:
		case NodeType::CONTINUE: {
			auto exp_idx = node[0];
			auto exp_typ = typecheck(exp_idx, scope_id);
			return ASSOC_TYPE(node_idx, exp_typ);
		}
			// FIXME: add typing rules
		case NodeType::ASS: {
			auto path = typecheck(node[0], scope_id);
			auto val = typecheck(node[1], scope_id);
			if (not unify(path, val))
				err(node.loc, "Assignment with value of wrong type");
			if (not is_ref(path))
				err(node.loc, "Left side of assignment must be a reference");
			return ASSOC_TYPE(node_idx, val);
		}
			// FIXME: add typing rules
		case NodeType::EQ: {
			const auto left = typecheck(node[0], scope_id);
			const auto right = typecheck(node[1], scope_id);
			if (not unify(left, right))
				mismatch_error(
					node.loc,
					"Equality comparison of values of different types is always false",
					left,
					right
				);
			return ASSOC_TYPE(node_idx, make_bool());
		}

			// | e1 : Bool
			// | e2 : Bool
			// +----------
			// | |- e1 or e1 : Bool

			// | e1 : Bool
			// | e2 : Bool
			// +----------
			// | |- e1 and e1 : Bool

		case NodeType::OR:
		case NodeType::AND: {
			const auto left_typ = typecheck(node[0], scope_id);
			const auto right_typ = typecheck(node[1], scope_id);
			auto bool_typ = make_bool();

			if (not unify(left_typ, bool_typ)) {
				mismatch_error(
					node.loc,
					"Left side of logical combinator does not have boolean type",
					left_typ,
					bool_typ
				);
			}

			if (not unify(right_typ, bool_typ)) {
				mismatch_error(
					node.loc,
					"Right side of logical combinator does not have boolean type",
					right_typ,
					bool_typ
				);
			}

			return ASSOC_TYPE(node_idx, bool_typ);
		}
			// FIXME: add typing rules
		case NodeType::GTN:
		case NodeType::LTN:
		case NodeType::GTE:
		case NodeType::LTE: {
			const auto left = typecheck(node[0], scope_id);
			const auto right = typecheck(node[1], scope_id);
			if (!(unify(left, int64_typ) && unify(right, int64_typ)))
				err(node.loc, "Comparison operator arguments must be of numeric type");
			return ASSOC_TYPE(node_idx, make_bool());
		}
			// FIXME: add typing rules
		case NodeType::ADD:
		case NodeType::SUB:
		case NodeType::MUL:
		case NodeType::DIV:
		case NodeType::MOD: {
			const auto left = typecheck(node[0], scope_id);
			const auto right = typecheck(node[1], scope_id);
			const auto num = int64_typ;
			const auto& left_node = ast.at(node[0]);
			const auto& right_node = ast.at(node[1]);
			if (!unify(left, num))
				err(left_node.loc, "Left-hand side of operator is not numeric");
			if (!unify(right, num))
				err(
					right_node.loc,
					"Arithmetic operator arguments must be of numeric type"
				);
			return ASSOC_TYPE(node_idx, int64_typ);
		}

			// If e1 is an array of t and e2 is an integer, then e1[e2] has type t

			// FIXME: Since there is no way to construct a literal array right now,
			// the first rule is useless.

			// | E |- e1 : Array<t>
			// | E |- e2 : Int<64>
			// +------------
			// | E |- e1[e2] : t

			// | E |- e1 : Ref<Array<t>>
			// | E |- e2 : Int<64>
			// +------------
			// | E |- e1[e2] : Ref<t>

		case NodeType::AT: {
			auto arr_idx = node[0];
			auto off_idx = node[1];

			auto any_arr_typ = make_ref(make_array(make_typevar()));
			auto arr_typ = typecheck(arr_idx, scope_id);

			if (not unify(any_arr_typ, arr_typ)) {
				mismatch_error(node.loc, "Not an array", any_arr_typ, arr_typ);
			}

			if (not is_ref(arr_typ)) {
				err(node.loc, "Array expression is not a reference");
			}

			auto off_typ = typecheck(off_idx, scope_id);

			if (not unify(off_typ, int64_typ)) {
				mismatch_error(
					node.loc,
					"Index expression must be of integer type",
					off_typ,
					int64_typ
				);
			}

			auto actual_arr_typ = to_array(deref(arr_typ));
			return ASSOC_TYPE(node_idx, make_ref(actual_arr_typ->item_type));
		}

			// | E |- e : Bool
			// +-------------
			// | E |- not e : Bool

		case NodeType::NOT: {
			auto exp_typ = typecheck(node[0], scope_id);
			auto bool_typ = make_bool();

			if (not unify(exp_typ, bool_typ))
				mismatch_error(
					node.loc, "Expression is not of type boolean", exp_typ, bool_typ
				);

			return ASSOC_TYPE(node_idx, exp_typ);
		}

			// If a variable of name x of type t was previously declared, then x is
			// a reference to a t

			// | x : t in E
			// +-------
			// | E |- x : Ref<t>

		case NodeType::ID: {
			auto found_typ = env.find(scope_id, node.str_id);
			if (found_typ == nullptr)
				err(node.loc, "Variable not previously declared");
			auto ref_typ = make_ref(*found_typ);
			return ASSOC_TYPE(node_idx, ref_typ);
		}

			// |
			// +------
			// | |- s : Array<Uint<8>>

		case NodeType::STR: {
			return ASSOC_TYPE(node_idx, make_array(uint8_typ));
		}

			// FIXME: add typing rules
		case NodeType::VAR_DECL: {
			auto id_idx = node[0];
			auto opt_type_idx = node[1];
			auto exp_idx = node[2];

			const auto& id_node = ast.at(id_idx);
			const auto& opt_type_node = ast.at(opt_type_idx);

			auto exp = typecheck(exp_idx, scope_id);

			if (opt_type_node.type != NodeType::EMPTY) {
				auto annot = typecheck(opt_type_idx, scope_id);
				if (!unify(annot, exp))
					mismatch_error(
						node.loc,
						"Expression does not have type described in the annotation",
						exp,
						annot
					);
			}

			env.insert(scope_id, id_node.str_id, exp);
			return exp;
		}
			// FIXME: add typing rules
		case NodeType::FUN_DECL: {
			auto opt_type_idx = node[2];
			auto body_idx = node[3];

			const auto& id_node = ast.at(node[0]);
			const auto& params_node = ast.at(node[1]);
			const auto& opt_type_node = ast.at(opt_type_idx);

			// Start with parameters and output types and variables (or concrete
			// type, if provided)
			//   [t1, ..., tn] -> t
			auto typ = [&]() {
				vector<TYPE> inputs {};
				for (auto param_idx : params_node) inputs.push_back(make_typevar());

				// if no return type is provided, must be infered
				TYPE output = nullptr;
				if (opt_type_node.type == NodeType::EMPTY)
					output = make_typevar();
				else
					output = typecheck(opt_type_idx, scope_id);

				return ASSOC_TYPE(node_idx, make_function(inputs, output));
			}();

			auto var = env.insert(scope_id, id_node.str_id, typ);

			{
				auto new_scope_id = env.create_child_scope(scope_id);

				vector<TYPE> param_types {};

				for (auto param_idx : params_node) {
					const auto& param = ast.at(param_idx);
					auto var = make_typevar();
					param_types.push_back(var);
					env.insert(new_scope_id, param.str_id, var);
				}

				auto output = [&]() {
					auto body_type = typecheck(body_idx, new_scope_id);
					if (opt_type_node.type != NodeType::EMPTY) {
						auto opt_type = typecheck(opt_type_idx, new_scope_id);
						if (!unify(body_type, opt_type))
							mismatch_error(
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

				typ = make_function(param_types, output);
			}

			*var = typ;

			auto typ_ = typ;
			return ASSOC_TYPE(node_idx, typ_);
		}
			// |
			// +---------
			// | |- nil : Nil

		case NodeType::NIL:
			return ASSOC_TYPE(node_idx, make_nil());

			// |
			// +---------
			// | |- true : Bool

		case NodeType::TRUE:
			return ASSOC_TYPE(node_idx, make_bool());

			// |
			// +---------
			// | |- false : Bool

		case NodeType::FALSE:
			return ASSOC_TYPE(node_idx, make_bool());

			// FIXME: add typing rules
		case NodeType::LET: {
			auto decls_idx = node[0];
			auto exp_idx = node[1];

			const auto& decls = ast.at(decls_idx);

			auto new_scope_id = env.create_child_scope(scope_id);

			for (size_t i = 0; i < decls.branch.children_count; i++)
				typecheck(decls[i], new_scope_id);

			return ASSOC_TYPE(node_idx, typecheck(exp_idx, new_scope_id));
		}

			// |
			// +---------
			// | |- c : Uint<8>

		case NodeType::CHAR:
			return ASSOC_TYPE(node_idx, uint8_typ);

			// FIXME: add typing rules
		case NodeType::PATH:
			return ASSOC_TYPE(node_idx, typecheck(node[0], scope_id));
			// FIXME: add typing rules
		case NodeType::AS: {
			auto exp = typecheck(node[0], scope_id);
			auto typ_ = typecheck(node[1], scope_id);

			if (is_integer(exp) and is_integer(typ_)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else if (is_bool(exp) and is_integer(typ_)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else if (unify(exp, typ_)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else {
				mismatch_error(node.loc, "Can't cast value to type", typ_, exp);
			}

			break;
		}

			// FIXME: These are kinda wrong as having a type is not the same as
			// representing or evaluating to a type.

			// | e1 : Int<64>
			// +------------
			// | |- int e1 : Int<e1>

			// | e1 : Int<64>
			// +------------
			// | |- uint e1 : Uint<e1>

			// |
			// +------------
			// | |- bool : Bool

			// |
			// +------------
			// | |- nil : Nil

		case NodeType::INT_TYPE: {
			const auto& type_size_node = ast.at(node[0]);
			return ASSOC_TYPE(node_idx, make_integer(type_size_node.num, SIGNED));
		}
		case NodeType::UINT_TYPE: {
			const auto& type_size_node = ast.at(node[0]);
			return ASSOC_TYPE(node_idx, make_integer(type_size_node.num, UNSIGNED));
		}
		case NodeType::BOOL_TYPE: {
			return ASSOC_TYPE(node_idx, make_bool());
		}
		case NodeType::NIL_TYPE: {
			return ASSOC_TYPE(node_idx, make_nil());
		}
	}

	assert(false && "unreachable");
}
