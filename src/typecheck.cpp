#include "typecheck.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <concepts>
#include <cstring>
#include <memory>
#include <ranges>
#include <span>

#include "ast.hpp"
#include "logger.hpp"
#include "str_pool.h"
#include "type.hpp"
#include "utils.hpp"

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
TYPE Typechecker::make_toat() { return std::shared_ptr<Toat>(new Toat()); }

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

TYPE Typechecker::make_general(std::vector<TYPE> var, TYPE body) {
	return std::shared_ptr<General>(new General {var, body});
}

template<typename D>
	requires std::derived_from<D, Type>
bool is(std::shared_ptr<Type> typ) {
	return std::dynamic_pointer_cast<D>(typ) != nullptr;
}

template<typename D>
	requires std::derived_from<D, Type>
std::shared_ptr<D> to(std::shared_ptr<Type> typ) {
	return dynamic_pointer_cast<D>(typ);
}

TYPE Typechecker::deref(TYPE typ) {
	if (is<Ref>(typ))
		return to<Ref>(typ)->ref_type;
	else
		return typ;
}

TYPE Typechecker::substitute(TYPE gen, std::vector<TYPE> args) {
	auto general = to<General>(gen);
	std::vector<TYPE_VARIABLE> vars;
	std::ranges::transform(args, vars.begin(), to<TypeVariable>);
	auto body = general->body;
	return substitute_aux(body, vars, args);
}

TYPE Typechecker::substitute_aux(
	TYPE body, std::vector<TYPE_VARIABLE> vars, std::vector<TYPE> args
) {
	if (is<Nil>(body)) {
		return body;
	} else if (is<Bool>(body)) {
		return body;
	} else if (is<Void>(body)) {
		return body;
	} else if (is<Toat>(body)) {
		return body;
	} else if (is<Integer>(body)) {
		// Should be handled separately as an intrinsic type
		assert(false);
	} else if (is<Array>(body)) {
		auto arr_typ = to<Array>(body);
		return make_array(substitute_aux(arr_typ->item_type, vars, args));
	} else if (is<Function>(body)) {
		auto func_typ = to<Function>(body);
		std::vector<TYPE> inputs;
		for (auto typ : func_typ->inputs)
			inputs.push_back(substitute_aux(typ, vars, args));
		return make_function(inputs, substitute_aux(func_typ->output, vars, args));
	} else if (is<TypeVariable>(body)) {
		auto var = to<TypeVariable>(body);
		if (var->is_bound) {
			return substitute_aux(var->bound_type, vars, args);
		} else {
			for (const auto& [a, v] : std::ranges::views::zip(args, vars))
				if (var->unbound_name == v->unbound_name) return a;
			return var;
		}

	} else if (is<Ref>(body)) {
		auto ref_typ = to<Ref>(body);
		return make_ref(substitute_aux(ref_typ->ref_type, vars, args));
	} else if (is<General>(body)) {
		auto gen = to<General>(body);
		std::vector<TYPE_VARIABLE> not_overriden;
		std::vector<TYPE> new_args;
		for (const auto& [a, v] : std::ranges::views::zip(args, vars)) {
			auto overriden = std::ranges::any_of(gen->vars, [&](const auto& gen_var) {
				auto var = to<TypeVariable>(gen_var);
				return not var->is_bound and v->unbound_name == var->unbound_name;
			});
			if (not overriden) {
				not_overriden.push_back(v);
				new_args.push_back(a);
			}
		}
		return make_general(
			gen->vars, substitute_aux(gen->body, not_overriden, new_args)
		);
	} else {
		assert(false);
	}
}

void Typechecker::mismatch_error(
	Location loc, const char* msg, TYPE got, TYPE expected
) {
	std::cerr << ANSI_STYLE_BOLD << "<FIXME.fala>:" << loc.begin.line + 1 << ":"
						<< loc.begin.column + 1 << ": " << ANSI_COLOR_RED
						<< "TYPECHECK ERROR:" ANSI_COLOR_RESET << ' ' << msg << '\n'
						<< "\tExpected: " << *expected << '\n'
						<< "\t     Got: " << *got << '\n';
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
	if (auto ra = to<Ref>(a)) return unify(ra->ref_type, b);

	// if b is a ref. unify a and ref b'
	if (auto rb = to<Ref>(b)) return unify(a, rb->ref_type);

	// a is bound to 'a. unify a' and b
	if (auto va = to<TypeVariable>(a); va and va->is_bound)
		return unify(va->bound_type, b);

	// b is bound to 'b. unify a and b'
	if (auto vb = to<TypeVariable>(b); vb and vb->is_bound)
		return unify(a, vb->bound_type);

	// a is unbound. bind a to b
	if (auto va = to<TypeVariable>(a))
		if (not va->is_bound) return va->bind_to(b), true;

	// b is unbound typevar. bind b to a
	if (auto vb = to<TypeVariable>(b))
		if (not vb->is_bound) return vb->bind_to(a), true;

	// a and b are functions of same arity.
	// unify their inputs. unify their outputs
	if (auto fa = to<Function>(a), fb = to<Function>(b); fa and fb) {
		if (fa->inputs.size() != fb->inputs.size()) return false;
		auto unified_inputs = std::ranges::all_of(
			std::ranges::views::zip(fa->inputs, fb->inputs),
			[&](const auto& p) {
				const auto& [a, b] = p;
				return unify(a, b);
			}
		);
		return unified_inputs and unify(fa->output, fb->output);
	}

	// a and b are arrays. unify element types
	if (auto aa = to<Array>(a), ab = to<Array>(b); aa and ab)
		return unify(aa->item_type, ab->item_type);

	if (auto ia = to<Integer>(a), ib = to<Integer>(b); ia and ib)
		return ia->bit_count == ib->bit_count and ia->sign == ib->sign;

	if (is<Nil>(a) and is<Nil>(b)) return true;
	if (is<Bool>(a) and is<Bool>(b)) return true;
	if (is<Void>(a) and is<Void>(b)) return true;

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

			return ASSOC_TYPE(node_idx, to<Function>(deref(func_type))->output);
		}
			// |
			// +------
			// | |- n : Int<64>

		case NodeType::NUM: {
			return ASSOC_TYPE(node_idx, make_integer(64, SIGNED));
		}
			// FIXME: add typing rules
		case NodeType::BLK: {
			for (auto exp_idx : std::span(node.begin(), node.end() - 1))
				typecheck(exp_idx, scope_id);
			return ASSOC_TYPE(node_idx, typecheck(node[node.size() - 1], scope_id));
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
				logger.err(
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
				logger.err(node.loc, "Assignment with value of wrong type");
			if (not is<Ref>(path))
				logger.err(node.loc, "Left side of assignment must be a reference");
			return ASSOC_TYPE(node_idx, val);
		}
			// FIXME: add typing rules
		case NodeType::EQ: {
			const auto left = typecheck(node[0], scope_id);
			const auto right = typecheck(node[1], scope_id);
			if (not unify(left, right))
				mismatch_error(
					node.loc,
					"Equality comparison of values of different types is always "
					"false",
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
				logger.err(
					node.loc, "Comparison operator arguments must be of numeric type"
				);
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
				logger.err(left_node.loc, "Left-hand side of operator is not numeric");
			if (!unify(right, num))
				logger.err(
					right_node.loc,
					"Arithmetic operator arguments must be of numeric type"
				);
			return ASSOC_TYPE(node_idx, int64_typ);
		}

			// If e1 is an array of t and e2 is an integer, then e1[e2] has type t

			// FIXME: Since there is no way to construct a literal array right
			// now, the first rule is useless.

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

			if (not unify(any_arr_typ, arr_typ))
				mismatch_error(node.loc, "Not an array", any_arr_typ, arr_typ);

			if (not is<Ref>(arr_typ))
				logger.err(node.loc, "Array expression is not a reference");

			auto off_typ = typecheck(off_idx, scope_id);

			if (not unify(off_typ, int64_typ))
				mismatch_error(
					node.loc,
					"Index expression must be of integer type",
					off_typ,
					int64_typ
				);

			auto actual_arr_typ = to<Array>(deref(arr_typ));
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

			// If a variable of name x of type t was previously declared, then x
			// is a reference to a t

			// | x : t in E
			// +-------
			// | E |- x : Ref<t>

		case NodeType::ID: {
			auto found_typ = env.find(scope_id, node.str_id);
			if (found_typ == nullptr)
				logger.err(
					node.loc,
					"Variable \"%s\" not previously declared",
					pool.find(node.str_id)
				);
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
				auto annot = eval(opt_type_idx, scope_id);
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
					output = eval(opt_type_idx, scope_id);

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
						auto opt_type = eval(opt_type_idx, new_scope_id);
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

			for (auto decl_idx : decls) typecheck(decl_idx, new_scope_id);

			return ASSOC_TYPE(node_idx, typecheck(exp_idx, new_scope_id));
		}

			// |
			// +---------
			// | |- c : Uint<8>

		case NodeType::CHAR: return ASSOC_TYPE(node_idx, uint8_typ);

		case NodeType::PATH:
			return ASSOC_TYPE(node_idx, typecheck(node[0], scope_id));
			// FIXME: add typing rules
		case NodeType::AS: {
			auto exp = typecheck(node[0], scope_id);
			auto typ_ = eval(node[1], scope_id);

			if (is<Integer>(exp) and is<Integer>(typ_)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else if (is<Bool>(exp) and is<Integer>(typ_)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else if (unify(exp, typ_)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else {
				mismatch_error(node.loc, "Can't cast value to type", typ_, exp);
			}

			break;
		}

		case NodeType::INSTANCE: assert(false);
	}

	assert(false && "unreachable");
}

TYPE Typechecker::eval(NodeIndex node_idx, Env<TYPE>::ScopeID scope_id) {
	const auto& node = ast.at(node_idx);
	node_to_type[node_idx] = make_toat();
	switch (node.type) {
		case NodeType::ID: {
			if (node.str_id.idx == pool.intern(strdup("Bool")).idx) {
				return make_bool();
			}
			if (node.str_id.idx == pool.intern(strdup("Nil")).idx) {
				return make_nil();
			}
			assert(false);
		}

			// | x : type
			// | x = forall (x1, x2, ..., x2). t
			// | e1 : type
			// | e2 : type
			// | ...
			// | en : type
			// +-------------------------
			// | x<e1, e2, ..., en> = t

		case NodeType::INSTANCE: {
			assert(node.size() == 2);
			auto name_idx = node[0];
			auto args_idx = node[1];

			auto name_node = ast.at(name_idx);
			auto args_node = ast.at(args_idx);

			if (name_node.str_id.idx == pool.intern(strdup("Array")).idx) {
				auto elt_typ = eval(args_node[0], scope_id);
				return make_array(elt_typ);
			}

			if (name_node.str_id.idx == pool.intern(strdup("Int")).idx) {
				auto bit_count = ast.at(args_node[0]).num;
				return make_integer(bit_count, Sign::SIGNED);
			}

			if (name_node.str_id.idx == pool.intern(strdup("Uint")).idx) {
				auto bit_count = ast.at(args_node[0]).num;
				return make_integer(bit_count, Sign::UNSIGNED);
			}

			auto general_typ = typecheck(name_idx, scope_id);
			std::vector<TYPE> arguments;
			for (auto idx : ast.at(args_idx)) {
				arguments.push_back(eval(idx, scope_id));
			}

			return substitute(general_typ, arguments);
		}
		default: assert(false);
	}
}
