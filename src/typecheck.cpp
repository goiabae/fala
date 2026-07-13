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
// m       A mode

using M = std::shared_ptr<Mode>;
using D = std::shared_ptr<Datatype>;
using T = std::shared_ptr<Type>;

const auto VAR_ = std::make_shared<Mode>(ConcreteMode::VAR);
const auto VAL_ = std::make_shared<Mode>(ConcreteMode::VAL);
const auto OUT_ = std::make_shared<Mode>(ConcreteMode::OUT);

const auto NIL_ = std::make_shared<Nil>();
const auto VOID_ = std::make_shared<Void>();
const auto TOAT_ = std::make_shared<Toat>();

static M make_modevar(std::size_t index);
static D make_datavar(std::size_t index);
static D make_integer(int bit_count, Sign sign);
static D make_array(D item);
static D make_function(std::vector<T> inputs, D output);
static D make_bool();
static D make_general(std::vector<D> var, D body);
static T make_type(M mode, D datatype);
static bool cast_to(M, M);
static bool cast_to(D, D);

D make_general(std::vector<D> var, D body) {
	return std::make_shared<General>(var, body);
}

D make_bool() { return make_integer(1, Sign::UNSIGNED); }

M make_modevar(std::size_t index) {
	return std::make_shared<Mode>(Mode {.data = Variable<Mode>(index)});
}

D make_datavar(std::size_t index) {
	return std::make_shared<TypeVariable>(index);
}

D make_integer(int bit_count, Sign sign) {
	return std::make_shared<Integer>(bit_count, sign);
}

D make_array(D item_type) { return std::make_shared<Array>(item_type); }

D make_function(std::vector<T> inputs, D output) {
	return std::make_shared<Function>(inputs, output);
}

T make_type(M mode, D datatype) {
	return std::make_shared<Type>(mode, datatype);
}

DATATYPE Typechecker::make_datavar() { return ::make_datavar(next_var_id++); }

M Typechecker::make_modevar() { return ::make_modevar(next_modevar_id++); }

template<typename D>
	requires std::derived_from<D, Datatype>
bool is(std::shared_ptr<Datatype> typ) {
	return std::dynamic_pointer_cast<D>(typ) != nullptr;
}

template<typename D>
	requires std::derived_from<D, Datatype>
bool is(std::shared_ptr<Type> typ) {
	return is<D>(typ->datatype);
}

template<typename D>
	requires std::derived_from<D, Datatype>
std::shared_ptr<D> to(std::shared_ptr<Datatype> typ) {
	return dynamic_pointer_cast<D>(typ);
}

// m t !> ERROR
// out t -> val t
// var t -> val t
// val t -> val t
//
std::shared_ptr<Type> Typechecker::deref(std::shared_ptr<Type> typ) {
	auto om = typ->mode->to_concrete();
	if (om.has_value()) {
		auto m = om.value();
		if (m == ConcreteMode::OUT || m == ConcreteMode::VAR) {
			return std::make_shared<Type>(Type {
				.mode = std::make_shared<Mode>(Mode {.data = ConcreteMode::VAL}),
				.datatype = typ->datatype,
			});
		} else {
			assert(m == ConcreteMode::VAL);
			return typ;
		}
	} else {
		throw std::runtime_error(
			"cannot dereference type because mode variable has not been instatiated "
			"into a concrete mode"
		);
	}
}

DATATYPE Typechecker::substitute(DATATYPE gen, std::vector<DATATYPE> args) {
	auto general = to<General>(gen);
	std::vector<TYPE_VARIABLE> vars;
	std::ranges::transform(args, vars.begin(), to<TypeVariable>);
	auto body = general->body;
	return substitute_aux(body, vars, args);
}

DATATYPE Typechecker::substitute_aux(
	DATATYPE body, std::vector<TYPE_VARIABLE> vars, std::vector<DATATYPE> args
) {
	if (is<Nil>(body)) {
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
		std::vector<std::shared_ptr<Type>> inputs;
		for (auto typ : func_typ->inputs)
			inputs.push_back(
				make_type(typ->mode, substitute_aux(typ->datatype, vars, args))
			);
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
	} else if (is<General>(body)) {
		auto gen = to<General>(body);
		std::vector<TYPE_VARIABLE> not_overriden;
		std::vector<DATATYPE> new_args;
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

bool cast_to(M from_, M to_) {
	return std::visit(
		overloaded {
			[&from_](ConcreteMode& from, Variable<Mode>& to) {
				to.bind_to(from_);
				return true;
			},
			[&to_](Variable<Mode>& from, ConcreteMode& to) {
				from.bind_to(to_);
				return true;
			},

			[&to_](Variable<Mode>& from, Variable<Mode>& to) {
				from.bind_to(to_);
				return true;
			},
			[](ConcreteMode& from, ConcreteMode& to) {
				if (from == ConcreteMode::VAL and to == ConcreteMode::OUT)
					return false;
				else
					return true;
			},
		},
		from_->data,
		to_->data
	);
}

bool cast_to(D from, D to) {
	// a is bound to 'a. unify a' and b
	if (auto va = ::to<TypeVariable>(from); va and va->is_bound)
		return cast_to(va->bound_type, to);

	// b is bound to 'b. unify a and b'
	if (auto vb = ::to<TypeVariable>(to); vb and vb->is_bound)
		return cast_to(from, vb->bound_type);

	// a is unbound. bind a to b
	if (auto va = ::to<TypeVariable>(from))
		if (not va->is_bound) return va->bind_to(to), true;

	// b is unbound typevar. bind b to a
	if (auto vb = ::to<TypeVariable>(to))
		if (not vb->is_bound) return vb->bind_to(from), true;

	// a and b are arrays. unify element types
	if (auto aa = ::to<Array>(from), ab = ::to<Array>(to); aa and ab)
		return cast_to(aa->item_type, ab->item_type);

	if (auto ifrom = ::to<Integer>(from), ito = ::to<Integer>(to);
	    ifrom and ito) {
		return ito->bit_count >= ifrom->bit_count;
	}

	if (is<Nil>(from) and is<Nil>(to)) return true;
	if (is<Void>(from) and is<Void>(to)) return true;

	return false;
}

bool Typechecker::cast_to(T from, T to) {
	return ::cast_to(from->mode, to->mode)
	   and ::cast_to(from->datatype, to->datatype);
}

void Typechecker::mismatch_error(
	Location loc, const char* msg, T got, T expected
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
	auto nil_typ = NIL_;
	auto uint8_arr_typ = make_array(uint8_typ);
	auto int64_arr_typ = make_array(int64_typ);

	auto nil_to_uint8_typ = make_function({make_type(VAL_, nil_typ)}, uint8_typ);
	auto nil_to_int64_typ = make_function({make_type(VAL_, nil_typ)}, int64_typ);
	auto uint8_to_nil_typ = make_function({make_type(VAL_, uint8_typ)}, nil_typ);
	auto int64_to_nil_typ = make_function({make_type(VAL_, int64_typ)}, nil_typ);
	auto uint8_arr_to_nil_typ =
		make_function({make_type(VAL_, uint8_arr_typ)}, nil_typ);
	auto int64_to_int64_arr_typ =
		make_function({make_type(VAL_, int64_typ)}, int64_arr_typ);

	BIND_FUNC("read", make_type(VAL_, nil_to_uint8_typ));
	BIND_FUNC("read_int", make_type(VAL_, nil_to_int64_typ));
	BIND_FUNC("write", make_type(VAL_, uint8_to_nil_typ));
	BIND_FUNC("write_int", make_type(VAL_, int64_to_nil_typ));
	BIND_FUNC("write_str", make_type(VAL_, uint8_arr_to_nil_typ));
	BIND_FUNC("make_array", make_type(VAL_, int64_to_int64_arr_typ));

	typecheck(ast.root_index, scope_id);
	return true;
}

bool Typechecker::unify(std::shared_ptr<Mode> a, std::shared_ptr<Mode> b) {
	auto a_is_concrete = std::holds_alternative<ConcreteMode>(a->data);
	auto b_is_concrete = std::holds_alternative<ConcreteMode>(b->data);
	if (a_is_concrete and not b_is_concrete) {
		auto& t1 = std::get<ConcreteMode>(a->data);
		auto& t2 = std::get<Variable<Mode>>(b->data);
		if (t2.is_bound()) {
			return unify(a, t2.get_bound());
		} else {
			t2.bind_to(a);
			return true;
		}
	} else if (b_is_concrete and not a_is_concrete) {
		auto& t1 = std::get<ConcreteMode>(b->data);
		auto& t2 = std::get<Variable<Mode>>(a->data);
		if (t2.is_bound()) {
			return unify(b, t2.get_bound());
		} else {
			t2.bind_to(b);
			return true;
		}
	} else if (a_is_concrete and b_is_concrete) {
		auto& t1 = std::get<ConcreteMode>(b->data);
		auto& t2 = std::get<ConcreteMode>(a->data);
		// FIXME: how to deal with submoding when creating new variables?
		return true;
	} else if (not a_is_concrete and not b_is_concrete) {
		auto& t1 = std::get<Variable<Mode>>(b->data);
		auto& t2 = std::get<Variable<Mode>>(a->data);
		t1.bind_to(a);
		return true;
	}
	assert(false);
}

bool Typechecker::unify(D a, D b) {
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
			std::ranges::views::zip(fa->inputs, fb->inputs), [&](const auto& p) {
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
	if (is<Void>(a) and is<Void>(b)) return true;

	return false;
}

bool Typechecker::unify(T a, T b) {
	return unify(a->mode, b->mode) and unify(a->datatype, b->datatype);
}

#define ASSOC_TYPE(NODE, TYP) \
	[&]() {                     \
		T typ = TYP;              \
		node_to_type[NODE] = typ; \
		return typ;               \
	}()

// If an expression has type of lvalue, it can also be used as an rvalue

// | e1 : Ref<t1>
// +------------
// | |- e1 : t1

T Typechecker::typecheck(NodeIndex node_idx, Env<T>::ScopeID scope_id) {
	node_to_scope_id[node_idx] = scope_id;

	auto int64_typ = make_integer(64, SIGNED);
	auto uint8_typ = make_integer(8, UNSIGNED);

	const auto& node = ast.at(node_idx);
	switch (node.type) {
			// |
			// +------
			// | |- Void

		case NodeType::EMPTY: {
			return ASSOC_TYPE(node_idx, make_type(VAL_, VOID_));
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

			vector<T> inputs {};
			for (auto arg_idx : args_node) {
				auto arg_typ = typecheck(arg_idx, scope_id);
				inputs.push_back(arg_typ);
			}

			auto outdata = make_datavar();

			auto expected_func_type =
				make_type(make_modevar(), make_function(inputs, outdata));
			auto func_type = typecheck(func_idx, scope_id);

			if (not unify(func_type, expected_func_type)) {
				mismatch_error(
					node.loc,
					"Function and arguments don't match",
					func_type,
					expected_func_type
				);
			}

			return ASSOC_TYPE(node_idx, make_type(VAL_, outdata));
		}
			// |
			// +------
			// | |- n : Int<64>

		case NodeType::NUM: {
			return ASSOC_TYPE(node_idx, make_type(VAL_, make_integer(64, SIGNED)));
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

			auto t1 = make_type(make_modevar(), make_bool());

			if (not unify(cond_typ, t1)) {
				mismatch_error(
					node.loc,
					"Condition expression of if expression is not of type boolean",
					cond_typ,
					t1
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
			if (not unify(cond_typ, make_type(make_modevar(), make_bool())))
				logger.err(
					node.loc,
					"Condition expression of when expression is not of type boolean"
				);

			auto then_typ = typecheck(then_idx, scope_id);
			return ASSOC_TYPE(node_idx, make_type(VAL_, NIL_));
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
			                ? make_type(VAL_, int64_typ)
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
			auto bool_typ = make_type(VAL_, make_bool());

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
			auto t1 = make_type(VAR_, make_datavar());
			if (not unify(path, t1))
				logger.err(node.loc, "Left side of assignment must be an lvalue");
			if (not unify(path, val))
				logger.err(node.loc, "Assignment with value of wrong type");
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
			return ASSOC_TYPE(node_idx, make_type(VAL_, make_bool()));
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
			auto bool_typ = make_type(VAL_, make_bool());

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
			if (!(unify(left, make_type(VAL_, int64_typ))
			      && unify(right, make_type(VAL_, int64_typ))))
				logger.err(
					node.loc, "Comparison operator arguments must be of numeric type"
				);
			return ASSOC_TYPE(node_idx, make_type(VAL_, make_bool()));
		}
			// FIXME: add typing rules
		case NodeType::ADD:
		case NodeType::SUB:
		case NodeType::MUL:
		case NodeType::DIV:
		case NodeType::MOD: {
			const auto left = typecheck(node[0], scope_id);
			const auto right = typecheck(node[1], scope_id);
			const auto num = make_type(VAL_, int64_typ);
			const auto& left_node = ast.at(node[0]);
			const auto& right_node = ast.at(node[1]);
			if (!unify(left, num))
				logger.err(left_node.loc, "Left-hand side of operator is not numeric");
			if (!unify(right, num))
				logger.err(
					right_node.loc,
					"Arithmetic operator arguments must be of numeric type"
				);
			return ASSOC_TYPE(node_idx, num);
		}

			// If e1 is an array of t and e2 is an integer, then e1[e2] has type t
			//
			// | E |- e1 : m1 Array<t>
			// | E |- e2 : _ Int<64>
			// +------------
			// | E |- e1[e2] : m1 t
			//
		case NodeType::AT: {
			auto arr_idx = node[0];
			auto off_idx = node[1];

			auto m1 = make_modevar();
			auto t2 = make_datavar();

			auto any_arr_typ = make_type(m1, make_array(t2));
			auto arr_typ = typecheck(arr_idx, scope_id);

			if (not unify(any_arr_typ, arr_typ))
				mismatch_error(node.loc, "Not an array", any_arr_typ, arr_typ);

			auto off_typ = typecheck(off_idx, scope_id);

			if (not unify(off_typ, make_type(VAL_, int64_typ)))
				mismatch_error(
					node.loc,
					"Index expression must be of integer type",
					off_typ,
					make_type(VAL_, int64_typ)
				);

			return ASSOC_TYPE(node_idx, make_type(m1, t2));
		}

			// | E |- e : Bool
			// +-------------
			// | E |- not e : Bool

		case NodeType::NOT: {
			auto exp_typ = typecheck(node[0], scope_id);
			auto bool_typ = make_type(VAL_, make_bool());

			if (not unify(exp_typ, bool_typ))
				mismatch_error(
					node.loc, "Expression is not of type boolean", exp_typ, bool_typ
				);

			return ASSOC_TYPE(node_idx, exp_typ);
		}

			// If a variable of name x of type t was previously declared, then x
			// is a reference to a t

			// | m x : t in E
			// +-------
			// | E |- m x : t

		case NodeType::ID: {
			auto found_typ = env.find(scope_id, node.str_id);
			if (found_typ == nullptr)
				logger.err(
					node.loc,
					"Variable \"%s\" not previously declared",
					pool.find(node.str_id)
				);
			return ASSOC_TYPE(node_idx, *found_typ);
		}

			// |
			// +------
			// | |- s : Array<Uint<8>>

		case NodeType::STR: {
			return ASSOC_TYPE(node_idx, make_type(VAL_, make_array(uint8_typ)));
		}

		// | G |- e1 : m1 t1
		// +---------------
		// | G |- m x = e1 -> G, (x : t1)
		//
		case NodeType::VAR_DECL: {
			auto id_idx = node[0];
			auto opt_type_idx = node[1];
			auto exp_idx = node[2];

			const auto& id_node = ast.at(id_idx);
			const auto& opt_type_node = ast.at(opt_type_idx);

			auto exp = typecheck(exp_idx, scope_id);

			if (opt_type_node.type != NodeType::EMPTY) {
				auto annot = make_type(make_modevar(), eval(opt_type_idx, scope_id));
				if (!unify(annot, exp))
					mismatch_error(
						node.loc,
						"Expression does not have type described in the annotation",
						exp,
						annot
					);
			}

			// FIXME: should union with the mode the expression to check if they are
			// compatible. can't assign val to an out!
			auto t1 = make_type(VAR_, exp->datatype);
			env.insert(scope_id, id_node.str_id, t1);
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
				vector<T> inputs {};
				for (auto param_idx : params_node)
					inputs.push_back(make_type(make_modevar(), make_datavar()));

				// if no return type is provided, must be infered
				DATATYPE output = nullptr;
				if (opt_type_node.type == NodeType::EMPTY)
					output = make_datavar();
				else
					output = eval(opt_type_idx, scope_id);

				return ASSOC_TYPE(
					node_idx, make_type(VAL_, make_function(inputs, output))
				);
			}();

			auto var = env.insert(scope_id, id_node.str_id, typ);

			{
				auto new_scope_id = env.create_child_scope(scope_id);

				vector<T> param_types {};

				for (auto param_idx : params_node) {
					const auto& param = ast.at(param_idx);
					auto var = make_type(make_modevar(), make_datavar());
					param_types.push_back(var);
					env.insert(new_scope_id, param.str_id, var);
				}

				auto output = [&]() {
					auto body_type = typecheck(body_idx, new_scope_id);
					if (opt_type_node.type != NodeType::EMPTY) {
						auto opt_type = eval(opt_type_idx, new_scope_id);
						auto t1 = make_type(VAL_, opt_type);
						if (!unify(body_type, t1))
							mismatch_error(
								node.loc,
								"Function annotation output type and infered type don't "
								"match",
								body_type,
								t1
							);
						return opt_type;
					}
					return body_type->datatype;
				}();

				typ = make_type(VAL_, make_function(param_types, output));
			}

			*var = typ;

			auto typ_ = typ;
			return ASSOC_TYPE(node_idx, typ_);
		}
			// |
			// +---------
			// | |- nil : Nil

		case NodeType::NIL:
			return ASSOC_TYPE(node_idx, make_type(VAL_, NIL_));

			// |
			// +---------
			// | |- true : Bool

		case NodeType::TRUE:
			return ASSOC_TYPE(node_idx, make_type(VAL_, make_bool()));

			// |
			// +---------
			// | |- false : Bool

		case NodeType::FALSE:
			return ASSOC_TYPE(node_idx, make_type(VAL_, make_bool()));

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

		case NodeType::CHAR:
			return ASSOC_TYPE(node_idx, make_type(VAL_, uint8_typ));

		case NodeType::PATH:
			return ASSOC_TYPE(node_idx, typecheck(node[0], scope_id));
			// FIXME: add typing rules
		case NodeType::AS: {
			auto exp = typecheck(node[0], scope_id);
			auto typ_ = make_type(make_modevar(), eval(node[1], scope_id));
			if (cast_to(exp, typ_)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else {
				mismatch_error(node.loc, "Can't cast value to type", typ_, exp);
				break;
			}
		}

		case NodeType::INSTANCE: assert(false);
	}

	assert(false && "unreachable");
}

std::shared_ptr<Datatype> Typechecker::eval(
	NodeIndex node_idx, Env<std::shared_ptr<Type>>::ScopeID scope_id
) {
	const auto& node = ast.at(node_idx);
	node_to_type[node_idx] = make_type(VAL_, TOAT_);
	switch (node.type) {
		case NodeType::ID: {
			if (node.str_id == pool.intern(strdup("Bool"))) {
				return make_bool();
			}
			if (node.str_id == pool.intern(strdup("Nil"))) {
				return NIL_;
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

			if (name_node.str_id == pool.intern(strdup("Array"))) {
				auto elt_typ = eval(args_node[0], scope_id);
				return make_array(elt_typ);
			}

			if (name_node.str_id == pool.intern(strdup("Int"))) {
				auto bit_count = ast.at(args_node[0]).num;
				return make_integer(bit_count, Sign::SIGNED);
			}

			if (name_node.str_id == pool.intern(strdup("Uint"))) {
				auto bit_count = ast.at(args_node[0]).num;
				return make_integer(bit_count, Sign::UNSIGNED);
			}

			auto general_typ = typecheck(name_idx, scope_id);
			std::vector<DATATYPE> arguments;
			for (auto idx : ast.at(args_idx)) {
				arguments.push_back(eval(idx, scope_id));
			}

			return substitute(general_typ->datatype, arguments);
		}
		default: assert(false);
	}
}
