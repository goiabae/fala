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
#include <sstream>
#include <stdexcept>

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

const auto VAR_ = std::make_shared<Mode>(Mode::Concrete::VAR);
const auto VAL_ = std::make_shared<Mode>(Mode::Concrete::VAL);
const auto OUT_ = std::make_shared<Mode>(Mode::Concrete::OUT);

const auto NIL_ = std::make_shared<Nil>();
const auto VOID_ = std::make_shared<Void>();
const auto TOAT_ = std::make_shared<Toat>();

static bool cast_to(M, M);
static bool cast_to(D, D);

D Typechecker::make_datavar() { return ::make_datavar(next_var_id++); }

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
		if (m == Mode::Concrete::OUT || m == Mode::Concrete::VAR) {
			return std::make_shared<Type>(Type {
				.mode = std::make_shared<Mode>(Mode {.data = Mode::Concrete::VAL}),
				.datatype = typ->datatype,
			});
		} else {
			assert(m == Mode::Concrete::VAL);
			return typ;
		}
	} else {
		throw std::runtime_error(
			"cannot dereference type because mode variable has not been instatiated "
			"into a concrete mode"
		);
	}
}

D Typechecker::substitute(D gen, std::vector<D> args) {
	auto general = to<General>(gen);
	std::vector<std::shared_ptr<TypeVariable>> vars;
	std::ranges::transform(args, vars.begin(), to<TypeVariable>);
	auto body = general->body;
	return substitute_aux(body, vars, args);
}

D Typechecker::substitute_aux(
	D body, std::vector<std::shared_ptr<TypeVariable>> vars, std::vector<D> args
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
		std::vector<std::shared_ptr<TypeVariable>> not_overriden;
		std::vector<D> new_args;
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
			[&from_]([[maybe_unused]] Mode::Concrete& from, Variable<Mode>& to) {
				to.bind_to(from_);
				return true;
			},
			[&to_](Variable<Mode>& from, [[maybe_unused]] Mode::Concrete& to) {
				from.bind_to(to_);
				return true;
			},

			[&to_](Variable<Mode>& from, [[maybe_unused]] Variable<Mode>& to) {
				from.bind_to(to_);
				return true;
			},
			[](Mode::Concrete& from, Mode::Concrete& to) {
				if (from == Mode::Concrete::VAL and to == Mode::Concrete::OUT)
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
	auto uint8_typ = make_integer(8, Integer::Sign::UNSIGNED);
	auto int64_typ = make_integer(64, Integer::Sign::SIGNED);
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

	const auto guids = symtab.symbols_in(symtab.builtin_scope);
	for (const auto guid : guids) {
		const auto name = std::string_view(pool.find(symtab.name_of(guid).value()));
		if (name == "read_char") {
			variables[guid] = make_type(VAL_, nil_to_uint8_typ);
		} else if (name == "read_int") {
			variables[guid] = make_type(VAL_, nil_to_int64_typ);
		} else if (name == "write_char") {
			variables[guid] = make_type(VAL_, uint8_to_nil_typ);
		} else if (name == "write_int") {
			variables[guid] = make_type(VAL_, int64_to_nil_typ);
		} else if (name == "write_str") {
			variables[guid] = make_type(VAL_, uint8_arr_to_nil_typ);
		} else {
			std::cerr << std::format("unexpected builtin function {}", name) << '\n';
			assert(false);
		}
	}

	typecheck(ast.root_index);
	return true;
}

bool Typechecker::unify(std::shared_ptr<Mode> a, std::shared_ptr<Mode> b) {
	auto a_is_concrete = std::holds_alternative<Mode::Concrete>(a->data);
	auto b_is_concrete = std::holds_alternative<Mode::Concrete>(b->data);
	if (a_is_concrete and not b_is_concrete) {
		auto& bv = std::get<Variable<Mode>>(b->data);
		if (bv.is_bound()) {
			return unify(a, bv.get_bound());
		} else {
			bv.bind_to(a);
			return true;
		}
	} else if (b_is_concrete and not a_is_concrete) {
		auto& av = std::get<Variable<Mode>>(a->data);
		if (av.is_bound()) {
			return unify(b, av.get_bound());
		} else {
			av.bind_to(b);
			return true;
		}
	} else if (a_is_concrete and b_is_concrete) {
		// FIXME: how to deal with submoding when creating new variables?
		return true;
	} else if ((not a_is_concrete) and (not b_is_concrete)) {
		auto& bv = std::get<Variable<Mode>>(b->data);
		auto& av = std::get<Variable<Mode>>(a->data);
		if (bv.is_bound() and av.is_bound()) {
			return unify(av.get_bound(), bv.get_bound());
		} else if (bv.is_bound()) {
			av.bind_to(b);
			return true;
		} else if (av.is_bound()) {
			bv.bind_to(a);
			return true;
		} else {
			bv.bind_to(a);
			return true;
		}
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

	if (auto ra = to<Record>(a), rb = to<Record>(b); ra and rb) {
		if (ra->fields.size() != rb->fields.size()) return false;
		return std::ranges::all_of(
			std::ranges::views::zip(ra->fields, rb->fields), [&](const auto& p) {
				const auto& [a, b] = p;
				return a.first == b.first and unify(a.second, b.second);
			}
		);
	}

	if (auto ra = to<Record>(a)) {
		if (auto sb = to<RowSet>(b)) {
			for (const auto& [n, d] : sb->rows) {
				static constexpr const auto get_first = [](const auto& x) {
					return x.first;
				};
				auto found = std::ranges::find(ra->fields, n, get_first);
				if (found != ra->fields.end()) {
					if (not unify((*found).second, d)) return false;
				} else {
					return false;
				}
			}
			return true;
		}
	}

	if (auto sa = to<RowSet>(a)) {
		if (auto rb = to<Record>(b)) {
			return unify(rb, sa);
		}
	}

	if (auto sa = to<RowSet>(a), sb = to<RowSet>(b); sa and sb) {
		std::map<std::string, D> new_rows = sa->rows;
		for (const auto& [nb, db] : sb->rows) {
			if (new_rows.contains(nb)) {
				if (not unify(new_rows.at(nb), db)) return false;
			} else {
				new_rows[nb] = db;
			}
		}
		return true;
	}

	return false;
}

bool Typechecker::unify(T a, T b) {
	return unify(a->mode, b->mode) and unify(a->datatype, b->datatype);
}

bool Typechecker::submode(M sub, M sup) {
	auto sub_is_concrete = std::holds_alternative<Mode::Concrete>(sub->data);
	auto sup_is_concrete = std::holds_alternative<Mode::Concrete>(sup->data);
	if (sub_is_concrete and not sup_is_concrete) {
		auto& bv = std::get<Variable<Mode>>(sup->data);
		if (bv.is_bound()) {
			return submode(sub, bv.get_bound());
		} else {
			bv.bind_to(sub);
			return true;
		}
	} else if (sup_is_concrete and not sub_is_concrete) {
		auto& av = std::get<Variable<Mode>>(sub->data);
		if (av.is_bound()) {
			return submode(av.get_bound(), sup);
		} else {
			av.bind_to(sup);
			return true;
		}
	} else if (sub_is_concrete and sup_is_concrete) {
		auto subc = std::get<Mode::Concrete>(sub->data);
		auto supc = std::get<Mode::Concrete>(sup->data);
		std::ostringstream ss {};
		ss << subc << ", " << supc;
		logger.log(LogLevel::INFO, {}, "submode: {}", ss.str());
		if (subc == Mode::Concrete::VAL
		    and (supc == Mode::Concrete::OUT or supc == Mode::Concrete::VAR))
			return false;
		return true;
	} else if ((not sub_is_concrete) and (not sup_is_concrete)) {
		auto& bv = std::get<Variable<Mode>>(sup->data);
		auto& av = std::get<Variable<Mode>>(sub->data);
		if (bv.is_bound() and av.is_bound()) {
			return submode(av.get_bound(), bv.get_bound());
		} else if (bv.is_bound()) {
			av.bind_to(sup);
			return true;
		} else if (av.is_bound()) {
			bv.bind_to(sub);
			return true;
		} else {
			bv.bind_to(sub);
			return true;
		}
	}
	assert(false);
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

T Typechecker::typecheck(NodeIndex node_idx) {
	auto int64_typ = make_integer(64, Integer::Sign::SIGNED);
	auto uint8_typ = make_integer(8, Integer::Sign::UNSIGNED);

	const auto& node = ast.at(node_idx);
	switch (node.type) {
			// |
			// +------
			// | |- Void

		case NodeType::EMPTY: {
			return ASSOC_TYPE(node_idx, make_type(VAL_, VOID_));
		}

			// | f : _ (m1 t1, m2 t2, ..., mn tn) -> t0
			// |
			// | a1 : m1' t1
			// | a2 : m2' t2
			// | ...
			// | an : mn' tn
			// |
			// | m1' <: m1
			// | m2' <: m2
			// | ...
			// | mn' <: mn
			// +-----------
			// | |- f a1 a2 ... an : val t0
			//
		case NodeType::APP: {
			auto func_idx = node[0];
			auto args_idx = node[1];

			const auto& args_node = ast.at(args_idx);

			vector<T> inputs {};
			for (auto arg_idx : args_node) {
				auto arg_typ = typecheck(arg_idx);
				inputs.push_back(arg_typ);
			}

			auto outdata = make_datavar();

			auto expected_func_type =
				make_type(make_modevar(), make_function(inputs, outdata));
			auto func_type = typecheck(func_idx);

			if (not unify(func_type, expected_func_type)) {
				mismatch_error(
					node.loc,
					"Function and arguments don't match",
					func_type,
					expected_func_type
				);
			}

			std::ostringstream oss {};
			for (auto input_ty : inputs) {
				oss << ", " << *input_ty;
			}
			std::ostringstream a {};
			a << *func_type;
			logger.log(
				LogLevel::INFO, node.loc, "func: {}, args: {}", a.str(), oss.str()
			);

			return ASSOC_TYPE(node_idx, make_type(VAL_, outdata));
		}
			// |
			// +------
			// | |- n : Int<64>

		case NodeType::NUM: {
			return ASSOC_TYPE(
				node_idx, make_type(VAL_, make_integer(64, Integer::Sign::SIGNED))
			);
		}
			// FIXME: add typing rules
		case NodeType::BLK: {
			for (auto exp_idx : std::span(node.begin(), node.end() - 1))
				typecheck(exp_idx);
			return ASSOC_TYPE(node_idx, typecheck(node[node.size() - 1]));
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

			auto cond_typ = typecheck(cond_idx);

			auto t1 = make_type(make_modevar(), make_bool());

			if (not unify(cond_typ, t1)) {
				mismatch_error(
					node.loc,
					"Condition expression of if expression is not of type boolean",
					cond_typ,
					t1
				);
			}

			auto then_typ = typecheck(then_idx);
			auto else_typ = typecheck(else_idx);

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

			auto cond_typ = typecheck(cond_idx);
			if (not unify(cond_typ, make_type(make_modevar(), make_bool())))
				logger.err(
					node.loc,
					"Condition expression of when expression is not of type boolean"
				);

			auto then_typ = typecheck(then_idx);
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

			auto var_typ = typecheck(decl_idx);
			auto to_typ = typecheck(to_idx);

			auto step_typ = (ast.at(step_idx).type == NodeType::EMPTY)
			                ? make_type(VAL_, int64_typ)
			                : typecheck(step_idx);

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

			auto then_typ = typecheck(then_idx);
			return ASSOC_TYPE(node_idx, then_typ);
		}

			// | e1 : Bool
			// | e2 : t2
			// +---------------
			// | |- while e1 then e2 : t2

		case NodeType::WHILE: {
			auto cond_idx = node[0];
			auto then_idx = node[1];

			auto cond_typ = typecheck(cond_idx);
			auto bool_typ = make_type(VAL_, make_bool());

			if (not unify(cond_typ, bool_typ)) {
				mismatch_error(
					node.loc,
					"While loop condition must have type boolean",
					cond_typ,
					bool_typ
				);
			}

			auto then_typ = typecheck(then_idx);
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
			auto exp_typ = typecheck(exp_idx);
			return ASSOC_TYPE(node_idx, exp_typ);
		}

			// | e1 : m1 t1
			// | e2 : m2 t2
			// | m1 <: var
			// +--------------
			// | |- e1 = e2 : m2 t2

			// FIXME: add typing rules
		case NodeType::ASS: {
			auto path = typecheck(node[0]);
			auto val = typecheck(node[1]);
			auto t1 = make_type(VAR_, make_datavar());
			std::ostringstream ss {};
			ss << *path << ", " << *t1;
			logger.log(LogLevel::INFO, node.loc, "assignment: {}", ss.str());
			if (not submode(path->mode, t1->mode))
				logger.err(node.loc, "Left side of assignment must var or out");
			if (not unify(path, t1))
				logger.err(node.loc, "Left side of assignment must be an lvalue");
			if (not unify(path, val))
				logger.err(node.loc, "Assignment with value of wrong type");
			return ASSOC_TYPE(node_idx, val);
		}
			// FIXME: add typing rules
		case NodeType::EQ: {
			const auto left = typecheck(node[0]);
			const auto right = typecheck(node[1]);
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
			const auto left_typ = typecheck(node[0]);
			const auto right_typ = typecheck(node[1]);
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
			const auto left = typecheck(node[0]);
			const auto right = typecheck(node[1]);
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
			const auto left = typecheck(node[0]);
			const auto right = typecheck(node[1]);
			const auto num = make_type(VAL_, int64_typ);
			const auto& left_node = ast.at(node[0]);
			const auto& right_node = ast.at(node[1]);
			if (not cast_to(left, num))
				logger.err(left_node.loc, "Left-hand side of operator is not numeric");
			if (not cast_to(right, num))
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
			auto arr_typ = typecheck(arr_idx);

			if (not unify(any_arr_typ, arr_typ))
				mismatch_error(node.loc, "Not an array", any_arr_typ, arr_typ);

			auto off_typ = typecheck(off_idx);

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
			auto exp_typ = typecheck(node[0]);
			auto bool_typ = make_type(VAL_, make_bool());

			if (not unify(exp_typ, bool_typ))
				mismatch_error(
					node.loc, "Expression is not of type boolean", exp_typ, bool_typ
				);

			return ASSOC_TYPE(node_idx, exp_typ);
		}

		case NodeType::ID:
			assert(false);

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
			auto mode_idx = node[0];
			auto id_idx = node[1];
			auto opt_type_idx = node[2];
			auto exp_idx = node[3];

			const auto& mode_node = ast.at(mode_idx);
			const auto& id_node = ast.at(id_idx);
			const auto& opt_type_node = ast.at(opt_type_idx);
			assert(mode_node.type == NodeType::MODE);

			if (mode_node.mode == ModeLiteral::OUT) {
				logger.err(node.loc, "Cannot defined variable as out");
			}

			auto exp = typecheck(exp_idx);

			if (opt_type_node.type != NodeType::EMPTY) {
				auto annot = make_type(make_modevar(), eval(opt_type_idx));
				if (!unify(annot, exp))
					mismatch_error(
						node.loc,
						"Expression does not have type described in the annotation",
						exp,
						annot
					);
			}

			const auto m1 = [&]() {
				if (mode_node.mode == ModeLiteral::VAL)
					return VAL_;
				else if (mode_node.mode == ModeLiteral::VAR)
					return VAR_;
				assert(mode_node.mode != ModeLiteral::OUT);
				return VAL_;
			}();

			auto t1 = make_type(m1, exp->datatype);
			assert(id_node.type == NodeType::GUID);
			variables[id_node.guid] = t1;
			return exp;
		}
			// FIXME: add typing rules
		case NodeType::FUN_DECL: {
			auto opt_type_idx = node[2];
			auto body_idx = node[3];

			const auto& id_node = ast.at(node[0]);
			const auto& params_node = ast.at(node[1]);
			const auto& opt_type_node = ast.at(opt_type_idx);

			assert(id_node.type == NodeType::GUID);

			// Start with parameters and output types and variables (or concrete
			// type, if provided)
			//   [t1, ..., tn] -> t
			auto typ = [&]() {
				vector<T> inputs {};
				for ([[maybe_unused]] auto param_idx : params_node) {
					inputs.push_back(make_type(make_modevar(), make_datavar()));
				}

				// if no return type is provided, must be infered
				D output = nullptr;
				if (opt_type_node.type == NodeType::EMPTY)
					output = make_datavar();
				else
					output = eval(opt_type_idx);

				return ASSOC_TYPE(
					node_idx, make_type(VAL_, make_function(inputs, output))
				);
			}();

			{
				vector<T> param_types {};

				for (auto param_idx : params_node) {
					const auto& param = ast.at(param_idx);
					assert(param.type == NodeType::PARAM);
					const auto& param_name = ast.at(param[1]);
					assert(param_name.type == NodeType::GUID);
					const auto& param_mode = ast.at(param[0]);
					assert(
						param_mode.type == NodeType::MODE
						|| param_mode.type == NodeType::EMPTY
					);
					const auto modety = [&]() {
						if (param_mode.type == NodeType::EMPTY)
							return make_modevar();
						else if (param_mode.mode == ModeLiteral::VAL)
							return VAL_;
						else if (param_mode.mode == ModeLiteral::VAR)
							return VAR_;
						else if (param_mode.mode == ModeLiteral::OUT)
							return OUT_;
						else
							assert(false);
					}();
					auto var = make_type(modety, make_datavar());
					param_types.push_back(var);
					variables[param_name.guid] = var;
				}

				auto output = [&]() {
					auto body_type = typecheck(body_idx);
					if (opt_type_node.type != NodeType::EMPTY) {
						auto opt_type = eval(opt_type_idx);
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

			variables[id_node.guid] = typ;

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

			for (auto decl_idx : decls) typecheck(decl_idx);

			return ASSOC_TYPE(node_idx, typecheck(exp_idx));
		}

			// |
			// +---------
			// | |- c : Uint<8>

		case NodeType::CHAR:
			return ASSOC_TYPE(node_idx, make_type(VAL_, uint8_typ));

		case NodeType::PATH:
			return ASSOC_TYPE(node_idx, typecheck(node[0]));
			// FIXME: add typing rules
		case NodeType::AS: {
			auto exp = typecheck(node[0]);
			auto typ_ = make_type(make_modevar(), eval(node[1]));
			if (cast_to(exp, typ_)) {
				return ASSOC_TYPE(node_idx, typ_);
			} else {
				mismatch_error(node.loc, "Can't cast value to type", typ_, exp);
				break;
			}
		}

		case NodeType::INSTANCE: assert(false);
		case NodeType::METALIST:
			assert(false);

			// If a variable of name x of type t was previously declared, then x
			// is a reference to a t
			//
			// | m x : t in E
			// +-------
			// | E |- m x : t
			//
		case NodeType::GUID: {
			assert(variables.contains(node.guid));
			auto found_typ = variables.at(node.guid);
			assert(found_typ != nullptr);
			return ASSOC_TYPE(node_idx, found_typ);
		}
		case NodeType::MODE: assert(false);
		case NodeType::PARAM: assert(false);
		case NodeType::RECORD_LITERAL: {
			std::vector<std::pair<std::string, D>> field_tys {};
			for (auto field_idx : node) {
				const auto& field_node = ast.at(field_idx);
				assert(field_node.type == NodeType::FIELD);
				const auto t1 = typecheck(field_node[1]);
				if (not submode(t1->mode, VAL_))
					throw std::runtime_error("submoding error");
				const auto& name_node = ast.at(field_node[0]);
				assert(name_node.type == NodeType::ID);
				const auto name = std::string(pool.find(name_node.str_id));
				field_tys.push_back(std::make_pair(name, t1->datatype));
			}
			std::sort(
				field_tys.begin(), field_tys.end(), [](const auto& x, const auto& y) {
					return x.first < y.first;
				}
			);
			const auto d2 = make_record(field_tys);
			const auto t2 = make_type(VAL_, d2);
			return ASSOC_TYPE(node_idx, t2);
		}
		case NodeType::FIELD: assert(false);
		case NodeType::FIELD_ACCESS: {
			const auto t1 = typecheck(node[0]);
			const auto& field_node = ast.at(node[1]);
			assert(field_node.type == NodeType::ID);
			const auto field_name = std::string(pool.find(field_node.str_id));
			const auto d3 = make_datavar();
			const auto d2 = make_row_set(
				std::map<std::string, std::shared_ptr<Datatype>> {{field_name, d3}}
			);
			const auto t2 = make_type(make_modevar(), d2);
			std::ostringstream ss1 {};
			ss1 << *t1;
			std::ostringstream ss2 {};
			ss2 << *t2;
			logger.log(
				LogLevel::INFO,
				node.loc,
				"unifying value (t1:{}) with row set (t2:{})",
				ss1.str(),
				ss2.str()
			);
			if (not unify(t1, t2))
				logger.err(
					node.loc, "cannot access value without field \"{}\"", field_name
				);
			const auto t4 = make_type(t1->mode, d3);
			return ASSOC_TYPE(node_idx, t4);
		}
		case NodeType::ARRAY_LITERAL: {
			auto t1 = make_datavar();
			std::vector<D> elt_dtys {t1};
			for (auto elt_idx : node) {
				const auto elt_ty = typecheck(elt_idx);
				elt_dtys.push_back(elt_ty->datatype);
			}
			for (auto [x, y] : std::views::adjacent<2>(elt_dtys)) {
				if (not unify(x, y))
					logger.err(
						node.loc,
						"array literal contains element expression of different types"
					);
			}
			auto t2 = make_type(VAL_, make_array(t1));
			return ASSOC_TYPE(node_idx, t2);
		}
		case NodeType::REPEAT: {
			const auto t0 = typecheck(node[0]);
			const auto t1 = typecheck(node[1]);
			const auto t2 =
				make_type(make_modevar(), make_integer(64, Integer::Sign::UNSIGNED));
			if (not cast_to(t1, t2)) {
				logger.err(
					node.loc, "array expansion second argument must be an integer"
				);
			}
			const auto t3 = make_type(VAL_, make_array(t1->datatype));
			return ASSOC_TYPE(node_idx, t3);
		}
		case NodeType::LEN: {
			const auto t0 = typecheck(node[0]);
			const auto d0 = make_datavar();
			const auto t1 = make_type(VAL_, make_array(d0));
			if (not unify(t0, t1))
				logger.err(
					node.loc, "operand to array length operator (#) was not an array"
				);
			const auto t2 =
				make_type(VAL_, make_integer(64, Integer::Sign::UNSIGNED));
			return ASSOC_TYPE(node_idx, t2);
		}
	}
	assert(false && "unreachable");
}

std::shared_ptr<Datatype> Typechecker::eval(NodeIndex node_idx) {
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
				auto elt_typ = eval(args_node[0]);
				return make_array(elt_typ);
			}

			if (name_node.str_id == pool.intern(strdup("Int"))) {
				auto bit_count = ast.at(args_node[0]).num;
				return make_integer(bit_count, Integer::Sign::SIGNED);
			}

			if (name_node.str_id == pool.intern(strdup("Uint"))) {
				auto bit_count = ast.at(args_node[0]).num;
				return make_integer(bit_count, Integer::Sign::UNSIGNED);
			}

			auto general_typ = typecheck(name_idx);
			std::vector<D> arguments;
			for (auto idx : ast.at(args_idx)) {
				arguments.push_back(eval(idx));
			}

			return substitute(general_typ->datatype, arguments);
		}
		default: assert(false);
	}
}
