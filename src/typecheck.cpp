#include "typecheck.hpp"

#include <assert.h>
#include <stdlib.h>

#include <algorithm>
#include <concepts>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <ranges>
#include <span>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

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
using P = std::shared_ptr<PolyType>;
using V = std::shared_ptr<TypeVariable>;
using E = std::shared_ptr<EffectSet>;

const auto VAR_ = std::make_shared<Mode>(Mode::Concrete::VAR);
const auto VAL_ = std::make_shared<Mode>(Mode::Concrete::VAL);
const auto OUT_ = std::make_shared<Mode>(Mode::Concrete::OUT);

const auto NIL_ = std::make_shared<Nil>();
const auto VOID_ = std::make_shared<Void>();

const auto EMPTY_EFFSET = []() {
	std::vector<std::shared_ptr<Effect>> effs {};
	return std::make_shared<EffectSet>(effs);
}();

static bool cast_to(M, M);
static bool cast_to(D, D);
static bool cast_to(T from, T to);

D Typechecker::make_datavar() { return ::make_datavar(next_var_id++); }

M Typechecker::make_modevar() { return ::make_modevar(next_modevar_id++); }

D Typechecker::make_genvar() {
	auto x = std::dynamic_pointer_cast<TypeVariable>(make_datavar());
	x->is_generalized = true;
	return x;
}

E Typechecker::make_effsetvar() {
	return ::make_effsetvar(next_effsetvar_id++);
}

static bool operator==(const Effect& a, const Effect& b) {
	return a.name == b.name;
}

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

template<class T, class F>
	requires std::derived_from<typename std::remove_cvref<T>::type, Datatype>
auto visit(T&& obj, F&& f) {
	if (auto x = dynamic_cast<Integer*>(&obj)) {
		return f(std::forward<Integer>(*x));
	} else if (auto x = dynamic_cast<Nil*>(&obj)) {
		return f(std::forward<Nil>(*x));
	} else if (auto x = dynamic_cast<Void*>(&obj)) {
		return f(std::forward<Void>(*x));
	} else if (auto x = dynamic_cast<Function*>(&obj)) {
		return f(std::forward<Function>(*x));
	} else if (auto x = dynamic_cast<Closure*>(&obj)) {
		return f(std::forward<Closure>(*x));
	} else if (auto x = dynamic_cast<TypeVariable*>(&obj)) {
		return f(std::forward<TypeVariable>(*x));
	} else if (auto x = dynamic_cast<Array*>(&obj)) {
		return f(std::forward<Array>(*x));
	} else if (auto x = dynamic_cast<Record*>(&obj)) {
		return f(std::forward<Record>(*x));
	} else if (auto x = dynamic_cast<RowSet*>(&obj)) {
		return f(std::forward<RowSet>(*x));
	} else {
		assert(false && "no more datatype subclasses!!!");
	}
}

static NodeIndex decl_name(const Node& node) {
	if (node.type == NodeType::VAR_DECL) {
		return node[1];
	} else if (node.type == NodeType::FUN_DECL) {
		return node[0];
	} else if (node.type == NodeType::TYPE_DECL) {
		return node[0];
	} else if (node.type == NodeType::EFF_DECL) {
		return node[0];
	} else if (node.type == NodeType::EFF_ALIAS) {
		return node[0];
	} else {
		assert(false);
	}
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
			"cannot dereference type because mode variable has not been "
			"instatiated "
			"into a concrete mode"
		);
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

static bool contains(
	std::vector<std::shared_ptr<Effect>> set, std::shared_ptr<Effect> eff
) {
	for (auto x : set) {
		if (*x == *eff) return true;
	}
	return false;
}

static bool contained_in(
	std::vector<std::shared_ptr<Effect>> sub,
	std::vector<std::shared_ptr<Effect>> sup
) {
	for (auto eff : sub) {
		if (not contains(sup, eff)) return false;
	}
	return true;
}

static bool set_equal(
	std::vector<std::shared_ptr<Effect>> a, std::vector<std::shared_ptr<Effect>> b
) {
	return contained_in(a, b) and contained_in(b, a);
}

static std::vector<std::shared_ptr<Effect>> difference(
	std::vector<std::shared_ptr<Effect>> sup,
	std::vector<std::shared_ptr<Effect>> sub
) {
	if (not contained_in(sub, sup)) throw std::runtime_error("");
	std::vector<std::shared_ptr<Effect>> diff {};
	for (auto eff : sup) {
		if (not contains(sub, eff)) {
			diff.push_back(eff);
		}
	}
	return diff;
}

static std::vector<std::shared_ptr<Effect>> difference2(
	std::vector<std::shared_ptr<Effect>> sup,
	std::vector<std::shared_ptr<Effect>> sub
) {
	std::vector<std::shared_ptr<Effect>> diff {};
	for (auto eff : sup) {
		if (not contains(sub, eff)) {
			diff.push_back(eff);
		}
	}
	return diff;
}

static std::vector<std::shared_ptr<Effect>> union_of(
	std::vector<std::shared_ptr<Effect>> sup,
	std::vector<std::shared_ptr<Effect>> sub
) {
	std::vector<std::shared_ptr<Effect>> res {};
	for (auto eff : sup) res.push_back(eff);
	for (auto eff : sub) {
		if (not contains(res, eff)) res.push_back(eff);
	}
	return res;
}

static E normalize(E set) {
	if (set->variable == nullptr) return set;
	if (set->variable->is_bound()) {
		auto x = normalize(set->variable->get_bound());
		auto y = union_of(set->concrete, x->concrete);
		return std::make_shared<EffectSet>(y, x->variable);
	} else {
		return set;
	}
}

static bool unify(E a, E b) {
	if (a->variable == nullptr and b->variable == nullptr) {
		return set_equal(a->concrete, b->concrete);
	}

	if (a->variable == nullptr) {
		auto c = difference2(a->concrete, b->concrete);
		b->variable->bind_to(std::make_shared<EffectSet>(c));
		return true;
	}

	if (b->variable == nullptr) {
		auto c = difference2(b->concrete, a->concrete);
		a->variable->bind_to(std::make_shared<EffectSet>(c));
		return true;
	}

	if (a->variable->is_bound()) {
		return unify(normalize(a), b);
	}

	if (b->variable->is_bound()) {
		return unify(a, normalize(b));
	}

	if (not b->variable->is_bound() and not a->variable->is_bound()) {
		if (contained_in(a->concrete, b->concrete)) {
			auto c = difference(b->concrete, a->concrete);
			a->variable->bind_to(std::make_shared<EffectSet>(c, b->variable));
			return true;
		} else {
			auto c = difference(a->concrete, b->concrete);
			b->variable->bind_to(std::make_shared<EffectSet>(c, a->variable));
			return true;
		}
	}

	return false;
}

static bool cast_to(E from, E to) {
	if (from->variable == nullptr and to->variable == nullptr) {
		return contained_in(from->concrete, to->concrete);
	}

	if (from->variable == nullptr) {
		auto c = difference2(from->concrete, to->concrete);
		to->variable->bind_to(std::make_shared<EffectSet>(c));
		return true;
	}

	if (to->variable == nullptr) {
		auto c = difference2(to->concrete, from->concrete);
		from->variable->bind_to(std::make_shared<EffectSet>(c));
		return true;
	}

	if (from->variable->is_bound()) {
		return unify(normalize(from), to);
	}

	if (to->variable->is_bound()) {
		return unify(from, normalize(to));
	}

	if (not to->variable->is_bound() and not from->variable->is_bound()) {
		if (contained_in(from->concrete, to->concrete)) {
			auto c = difference(to->concrete, from->concrete);
			from->variable->bind_to(std::make_shared<EffectSet>(c, to->variable));
			return true;
		} else {
			auto c = difference(from->concrete, to->concrete);
			to->variable->bind_to(std::make_shared<EffectSet>(c, from->variable));
			return true;
		}
	}

	return false;
}

static E union_of(E a, E b) {
	auto x = normalize(a);
	auto y = normalize(b);
	auto z = union_of(x->concrete, y->concrete);
	if (x->variable == nullptr) {
		return std::make_shared<EffectSet>(z, y->variable);
	}
	if (y->variable == nullptr) {
		return std::make_shared<EffectSet>(z, x->variable);
	}
	assert(not x->variable->is_bound());
	assert(not y->variable->is_bound());
	if (contained_in(x->concrete, y->concrete)) {
		auto w = difference(y->concrete, x->concrete);
		x->variable->bind_to(std::make_shared<EffectSet>(w, y->variable));
		return std::make_shared<EffectSet>(z, y->variable);
	} else {
		auto w = difference(x->concrete, y->concrete);
		y->variable->bind_to(std::make_shared<EffectSet>(w, x->variable));
		return std::make_shared<EffectSet>(z, x->variable);
	}
}

bool cast_to(D from, D to) {
	if (from == to) return true;

	// a is bound to 'a. unify a' and b
	if (auto va = ::to<TypeVariable>(from); va and va->is_bound)
		return cast_to(va->bound_type, to);

	// b is bound to 'b. unify a and b'
	if (auto vb = ::to<TypeVariable>(to); vb and vb->is_bound)
		return cast_to(from, vb->bound_type);

	// a is unbound. bind a to b
	if (auto va = ::to<TypeVariable>(from)) {
		if (va->is_generalized) return true;
		if (not va->is_bound) return va->bind_to(to), true;
	}

	// b is unbound typevar. bind b to a
	if (auto vb = ::to<TypeVariable>(to)) {
		if (vb->is_generalized) return true;
		if (not vb->is_bound) return vb->bind_to(from), true;
	}

	// a and b are arrays. unify element types
	if (auto aa = ::to<Array>(from), ab = ::to<Array>(to); aa and ab)
		return cast_to(aa->item_type, ab->item_type);

	if (auto ifrom = ::to<Integer>(from), ito = ::to<Integer>(to);
	    ifrom and ito) {
		return ito->bit_count >= ifrom->bit_count;
	}

	if (is<Nil>(from) and is<Nil>(to)) return true;
	if (is<Void>(from) and is<Void>(to)) return true;

	if (auto cfrom = ::to<Closure>(from)) {
		if (auto fto = ::to<Function>(to)) {
			auto t1 = make_function(cfrom->inputs, cfrom->output, cfrom->effects);
			return cast_to(t1, fto);
		}
	}

	if (auto ffrom = ::to<Function>(from)) {
		if (auto cto = ::to<Closure>(to)) {
			auto t1 = make_function(cto->inputs, cto->output, cto->effects);
			auto no_captures = cto->captures.size() == 0;
			return no_captures and cast_to(ffrom, t1);
		}
	}

	// a and b are functions of same arity.
	// unify their inputs. unify their outputs
	if (auto fa = ::to<Function>(from), fb = ::to<Function>(to); fa and fb) {
		if (fa->inputs.size() != fb->inputs.size()) return false;
		auto unified_inputs = std::ranges::all_of(
			std::ranges::views::zip(fa->inputs, fb->inputs), [&](const auto& p) {
				const auto& [a, b] = p;
				// FIXME: might be wrong
				return cast_to(a, b);
			}
		);
		auto unified_outputs = cast_to(fa->output, fb->output);
		auto equal_effects = unify(fa->effects, fb->effects);
		return unified_inputs and unified_outputs and equal_effects;
	}

	if (auto cfrom = ::to<Closure>(from)) {
		if (auto cto = ::to<Closure>(to)) {
			auto same_captures = cfrom->captures == cto->captures;
			auto ffrom = make_function(cfrom->inputs, cfrom->output, cfrom->effects);
			auto fto = make_function(cto->inputs, cto->output, cto->effects);
			return same_captures and cast_to(ffrom, fto);
		}
	}

	return false;
}

bool cast_to(T from, T to) {
	return cast_to(from->mode, to->mode)
	   and cast_to(from->datatype, to->datatype);
}

void Typechecker::mismatch_error(
	Location loc, const char* msg, T got, T expected
) {
	std::cerr << ANSI_STYLE_BOLD << "<FIXME.fala>:" << loc.begin.line + 1 << ":"
						<< loc.begin.column + 1 << ": " << ANSI_COLOR_RED
						<< "TYPECHECK ERROR:" ANSI_COLOR_RESET << ' ' << msg << '\n'
						<< "\tExpected: " << *expected << '\n'
						<< "\t     Got: " << *got << '\n';
	std::exit(1);
}
#define BIND_FUNC(NAME, TYP) \
	env.insert(scope_id, pool.intern(strdup(NAME)), TYP)

bool Typechecker::typecheck() {
	auto uint8_typ = make_integer(8, Integer::Sign::UNSIGNED);
	auto int64_typ = make_integer(64, Integer::Sign::SIGNED);
	auto nil_typ = NIL_;
	auto uint8_arr_typ = make_array(uint8_typ);
	auto int64_arr_typ = make_array(int64_typ);

	std::vector<std::shared_ptr<Effect>> effs {std::make_shared<Effect>(Effect {
		.name = "io",
		// FIXME: what to put in these?
		.inputs = {},
		.output = {},
	})};
	auto io_effset = std::make_shared<EffectSet>(effs);

	auto nil_to_uint8_typ =
		make_function({make_type(VAL_, nil_typ)}, uint8_typ, io_effset);
	auto nil_to_int64_typ =
		make_function({make_type(VAL_, nil_typ)}, int64_typ, io_effset);
	auto uint8_to_nil_typ =
		make_function({make_type(VAL_, uint8_typ)}, nil_typ, io_effset);
	auto int64_to_nil_typ =
		make_function({make_type(VAL_, int64_typ)}, nil_typ, io_effset);
	auto uint8_arr_to_nil_typ =
		make_function({make_type(VAL_, uint8_arr_typ)}, nil_typ, io_effset);
	auto int64_to_int64_arr_typ =
		make_function({make_type(VAL_, int64_typ)}, int64_arr_typ, io_effset);

	const auto guids = symtab.symbols_declared_in(symtab.builtin_scope);
	for (const auto guid : guids) {
		const auto name = std::string_view(pool.find(symtab.name_of(guid).value()));
		const auto ns = symtab.namespace_of(guid);
		if (ns == symtab.value_namespace) {
			if (name == "read_char") {
				variables[guid] =
					make_polytype(VAL_, make_scheme({}, nil_to_uint8_typ));
			} else if (name == "read_int") {
				variables[guid] =
					make_polytype(VAL_, make_scheme({}, nil_to_int64_typ));
			} else if (name == "write_char") {
				variables[guid] =
					make_polytype(VAL_, make_scheme({}, uint8_to_nil_typ));
			} else if (name == "write_int") {
				variables[guid] =
					make_polytype(VAL_, make_scheme({}, int64_to_nil_typ));
			} else if (name == "write_str") {
				variables[guid] =
					make_polytype(VAL_, make_scheme({}, uint8_arr_to_nil_typ));
			} else {
				std::cerr << std::format("unexpected builtin function {}", name)
									<< '\n';
				assert(false);
			}
		} else if (ns == symtab.type_namespace) {
			if (name == "Nil") {
				type_names[guid] = NIL_;
			} else if (name == "Int" || name == "Uint") {
				// TODO: need to be able to have polymorphic types as type names
			} else {
				std::cerr << std::format("unexpected builtin type name {}", name)
									<< '\n';
				assert(false);
			}
		} else if (ns == symtab.effset_namespace) {
			if (name == "io") {
				effset_names[guid] = io_effset;
			} else {
				assert(false);
			}
		} else {
			assert(false);
		}
	}

	typecheck(ast.root_index);

	for (const auto& [guid, p] : variables) {
		const auto name = pool.find(symtab.name_of(guid).value());
		std::cerr << "TYPECHECK " << name << " : " << *p << '\n';
	}

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
	if (a == b) return true;

	// a is bound to 'a. unify a' and b
	if (auto va = to<TypeVariable>(a); va and va->is_bound)
		return unify(va->bound_type, b);

	// b is bound to 'b. unify a and b'
	if (auto vb = to<TypeVariable>(b); vb and vb->is_bound)
		return unify(a, vb->bound_type);

	// a is unbound. bind a to b
	if (auto va = to<TypeVariable>(a))
		if (not va->is_bound) {
			if (not va->is_generalized) va->bind_to(b);
			return true;
		}

	// b is unbound typevar. bind b to a
	if (auto vb = to<TypeVariable>(b))
		if (not vb->is_bound) {
			if (not vb->is_generalized) vb->bind_to(a);
			return true;
		}

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

E Typechecker::eval_effset(NodeIndex node_idx) {
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
		case NodeType::AT: assert(false);
		case NodeType::ADD: assert(false);
		case NodeType::SUB: assert(false);
		case NodeType::MUL: assert(false);
		case NodeType::DIV: assert(false);
		case NodeType::MOD: assert(false);
		case NodeType::NOT: assert(false);
		case NodeType::ID: assert(false);
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
		case NodeType::INSTANCE: assert(false);
		case NodeType::METALIST: {
			auto joined = EMPTY_EFFSET;
			for (auto idx : node) {
				const auto effset = eval_effset(idx);
				joined = union_of(joined, effset);
			}
			return joined;
		}
		case NodeType::GUID: {
			assert(effset_names.contains(node.guid));
			return effset_names.at(node.guid);
		}
		case NodeType::MODE: assert(false);
		case NodeType::PARAM: assert(false);
		case NodeType::RECORD_LITERAL: assert(false);
		case NodeType::FIELD: assert(false);
		case NodeType::FIELD_ACCESS: assert(false);
		case NodeType::ARRAY_LITERAL: assert(false);
		case NodeType::REPEAT: assert(false);
		case NodeType::LEN: assert(false);
		case NodeType::MODULE_BODY: assert(false);
		case NodeType::ARRAY_TYPE_LITERAL: assert(false);
		case NodeType::RECORD_TYPE_LITERAL: assert(false);
		case NodeType::FIELD_TYPE: assert(false);
		case NodeType::TYPE_DECL: assert(false);
		case NodeType::EFF_DECL: assert(false);
		case NodeType::EFF_ALIAS: assert(false);
		case NodeType::EFF_UNION: {
			const auto left = eval_effset(node[0]);
			const auto right = eval_effset(node[1]);
			return union_of(left, right);
		}
	}
	assert(false);
}

// If an expression has type of lvalue, it can also be used as an rvalue

// | e1 : Ref<t1>
// +------------
// | |- e1 : t1

TE Typechecker::typecheck(NodeIndex node_idx) {
	auto int64_typ = make_integer(64, Integer::Sign::SIGNED);
	auto uint8_typ = make_integer(8, Integer::Sign::UNSIGNED);

	const auto& node = ast.at(node_idx);
	switch (node.type) {
			// |
			// +------
			// | |- Void

		case NodeType::EMPTY: {
			auto t = make_type(VAL_, VOID_);
			ASSOC_TYPE(node_idx, t);
			return std::make_pair(t, EMPTY_EFFSET);
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

			auto joined = EMPTY_EFFSET;
			std::vector<T> inputs {};
			for (auto arg_idx : args_node) {
				auto arg_typ = typecheck(arg_idx);
				inputs.push_back(arg_typ.first);
				joined = union_of(joined, arg_typ.second);
			}

			auto outdata = make_datavar();

			auto func_effset = make_effsetvar();
			auto expected_func_type =
				make_type(make_modevar(), make_function(inputs, outdata, func_effset));
			auto func_type = typecheck(func_idx);
			logger.log(
				LogLevel::INFO,
				"Application of function with type ({})",
				*func_type.first
			);

			if (not cast_to(func_type.first, expected_func_type)) {
				mismatch_error(
					node.loc,
					"Function and arguments don't match",
					func_type.first,
					expected_func_type
				);
			}

			joined = union_of(joined, func_type.second);
			joined = union_of(joined, func_effset);

			logger.log(LogLevel::INFO, "Application with effects ({})", *joined);

			std::ostringstream oss {};
			oss << '[';
			for (const auto& [i, input_ty] : std::ranges::views::enumerate(inputs)) {
				oss << *input_ty;
				if (static_cast<std::size_t>(i) != (inputs.size() - 1)) oss << ";";
			}
			oss << ']';
			logger.log(
				LogLevel::INFO,
				node.loc,
				"Applying function ({}) to arguments ({})",
				*func_type.first,
				oss.str()
			);

			const auto t1 = make_type(VAL_, outdata);
			ASSOC_TYPE(node_idx, t1);
			return std::make_pair(t1, joined);
		}
			// |
			// +------
			// | |- n : Int<64>

		case NodeType::NUM: {
			auto t = make_type(VAL_, make_integer(64, Integer::Sign::SIGNED));
			ASSOC_TYPE(node_idx, t);
			return std::make_pair(t, EMPTY_EFFSET);
		}
			// FIXME: add typing rules
		case NodeType::BLK: {
			T ty = nullptr;
			auto joined = EMPTY_EFFSET;
			for (auto exp_idx : node) {
				auto t = typecheck(exp_idx);
				ty = t.first;
				joined = union_of(joined, t.second);
			}
			ASSOC_TYPE(node_idx, ty);
			return std::make_pair(ty, joined);
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

			if (not unify(cond_typ.first, t1)) {
				mismatch_error(
					node.loc,
					"Condition expression of if expression is not of type boolean",
					cond_typ.first,
					t1
				);
			}

			auto then_typ = typecheck(then_idx);
			auto else_typ = typecheck(else_idx);

			if (not unify(then_typ.first, else_typ.first))
				mismatch_error(
					node.loc,
					"If expression has \"then\" and \"else\" branches with different "
					"types",
					then_typ.first,
					else_typ.first
				);

			auto effset =
				union_of(cond_typ.second, union_of(then_typ.second, else_typ.second));

			ASSOC_TYPE(node_idx, then_typ.first);
			return std::make_pair(then_typ.first, effset);
		}

			// | e1 : Bool
			// | e2 : t2
			// +---------
			// | |- when e1 then e2 : Nil

		case NodeType::WHEN: {
			auto cond_idx = node[0];
			auto then_idx = node[1];

			auto cond_typ = typecheck(cond_idx);
			if (not unify(cond_typ.first, make_type(make_modevar(), make_bool())))
				logger.err(
					node.loc,
					"Condition expression of when expression is not of type boolean"
				);

			auto then_typ = typecheck(then_idx);
			auto t = make_type(VAL_, NIL_);
			ASSOC_TYPE(node_idx, t);
			return std::make_pair(t, union_of(cond_typ.second, then_typ.second));
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

			auto joined = EMPTY_EFFSET;

			auto var_typ = typecheck(decl_idx);
			auto to_typ = typecheck(to_idx);

			joined = union_of(joined, var_typ.second);
			joined = union_of(joined, to_typ.second);

			auto step_typ = [&]() {
				if (ast.at(step_idx).type == NodeType::EMPTY)
					return make_type(VAL_, int64_typ);
				else {
					auto t = typecheck(step_idx);
					joined = union_of(joined, t.second);
					return t.first;
				}
			}();

			const auto t1 = make_type(VAL_, int64_typ);

			if (not cast_to(var_typ.first, t1)) {
				mismatch_error(
					node.loc,
					"Cannot cast loop variable type into integer",
					var_typ.first,
					t1
				);
			}

			if (not cast_to(to_typ.first, t1)) {
				mismatch_error(
					node.loc, "Cannot cast loop bound type into integer", to_typ.first, t1
				);
			}

			if (not cast_to(step_typ, t1)) {
				mismatch_error(
					node.loc, "Cannot cast loop step type into integer", step_typ, t1
				);
			}

			auto then_typ = typecheck(then_idx);
			joined = union_of(joined, then_typ.second);
			logger.log(LogLevel::INFO, "For loop with type {}", *then_typ.first);
			ASSOC_TYPE(node_idx, then_typ.first);
			return std::make_pair(then_typ.first, joined);
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

			if (not unify(cond_typ.first, bool_typ)) {
				mismatch_error(
					node.loc,
					"While loop condition must have type boolean",
					cond_typ.first,
					bool_typ
				);
			}

			auto then_typ = typecheck(then_idx);
			ASSOC_TYPE(node_idx, then_typ.first);
			return std::make_pair(
				then_typ.first, union_of(cond_typ.second, then_typ.second)
			);
		}

			// | e1 : t1
			// +----------
			// | |- break e1 : t1

			// | e1 : t1
			// +----------
			// | |- continue e1 : t1

		// FIXME: wrong?? maybe return void instead
		case NodeType::BREAK:
		case NodeType::CONTINUE: {
			auto exp_idx = node[0];
			auto exp_typ = typecheck(exp_idx);
			ASSOC_TYPE(node_idx, exp_typ.first);
			return exp_typ;
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
			logger.log(
				LogLevel::INFO,
				node.loc,
				"assignment from {} to {}",
				*val.first,
				*path.first
			);
			if (not submode(path.first->mode, t1->mode))
				logger.err(node.loc, "Left side of assignment must var or out");
			if (not unify(path.first, t1))
				logger.err(node.loc, "Left side of assignment must be an lvalue");
			if (not unify(path.first, val.first))
				logger.err(node.loc, "Assignment with value of wrong type");
			ASSOC_TYPE(node_idx, val.first);
			return std::make_pair(val.first, union_of(path.second, val.second));
		}
			// FIXME: add typing rules
		case NodeType::EQ: {
			const auto left = typecheck(node[0]);
			const auto right = typecheck(node[1]);
			if (not unify(left.first, right.first))
				mismatch_error(
					node.loc,
					"Equality comparison of values of different types is always "
					"false",
					left.first,
					right.first
				);
			auto t = make_type(VAL_, make_bool());
			ASSOC_TYPE(node_idx, t);
			return std::make_pair(t, union_of(left.second, right.second));
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

			if (not unify(left_typ.first, bool_typ)) {
				mismatch_error(
					node.loc,
					"Left side of logical combinator does not have boolean type",
					left_typ.first,
					bool_typ
				);
			}

			if (not unify(right_typ.first, bool_typ)) {
				mismatch_error(
					node.loc,
					"Right side of logical combinator does not have boolean type",
					right_typ.first,
					bool_typ
				);
			}

			ASSOC_TYPE(node_idx, bool_typ);
			return std::make_pair(
				bool_typ, union_of(left_typ.second, right_typ.second)
			);
		}
			// FIXME: add typing rules
		case NodeType::GTN:
		case NodeType::LTN:
		case NodeType::GTE:
		case NodeType::LTE: {
			const auto left = typecheck(node[0]);
			const auto right = typecheck(node[1]);
			if (not(
						unify(left.first, make_type(VAL_, int64_typ))
						and unify(right.first, make_type(VAL_, int64_typ))
					))
				logger.err(
					node.loc, "Comparison operator arguments must be of numeric type"
				);
			auto t = make_type(VAL_, make_bool());
			ASSOC_TYPE(node_idx, t);
			return std::make_pair(t, union_of(left.second, right.second));
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
			if (not cast_to(left.first, num))
				logger.err(left_node.loc, "Left-hand side of operator is not numeric");
			if (not cast_to(right.first, num))
				logger.err(
					right_node.loc,
					"Arithmetic operator arguments must be of numeric type"
				);
			ASSOC_TYPE(node_idx, num);
			return std::make_pair(num, union_of(left.second, right.second));
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

			if (not unify(any_arr_typ, arr_typ.first))
				mismatch_error(node.loc, "Not an array", any_arr_typ, arr_typ.first);

			auto off_typ = typecheck(off_idx);

			if (not unify(off_typ.first, make_type(VAL_, int64_typ)))
				mismatch_error(
					node.loc,
					"Index expression must be of integer type",
					off_typ.first,
					make_type(VAL_, int64_typ)
				);

			auto t = make_type(m1, t2);
			ASSOC_TYPE(node_idx, t);
			return std::make_pair(t, union_of(arr_typ.second, off_typ.second));
		}

			// | E |- e : Bool
			// +-------------
			// | E |- not e : Bool

		case NodeType::NOT: {
			auto exp_typ = typecheck(node[0]);
			auto bool_typ = make_type(VAL_, make_bool());

			if (not unify(exp_typ.first, bool_typ))
				mismatch_error(
					node.loc, "Expression is not of type boolean", exp_typ.first, bool_typ
				);

			ASSOC_TYPE(node_idx, exp_typ.first);
			return exp_typ;
		}

		case NodeType::ID:
			assert(false);

			// |
			// +------
			// | |- s : Array<Uint<8>>

		case NodeType::STR: {
			auto t = make_type(VAL_, make_array(uint8_typ));
			ASSOC_TYPE(node_idx, t);
			return std::make_pair(t, EMPTY_EFFSET);
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
			assert(id_node.type == NodeType::GUID);

			const auto name =
				std::string_view(pool.find(symtab.name_of(id_node.guid).value()));

			logger.log(LogLevel::INFO, "Declaring \"{}\"", name);

			if (mode_node.mode == ModeLiteral::OUT) {
				logger.err(node.loc, "Cannot defined variable as out");
			}

			auto exp = typecheck(exp_idx);

			if (opt_type_node.type != NodeType::EMPTY) {
				auto annot = make_type(make_modevar(), eval(opt_type_idx));
				if (not unify(annot, exp.first))
					mismatch_error(
						node.loc,
						"Expression does not have type described in the annotation",
						exp.first,
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

			auto t1 = make_type(m1, exp.first->datatype);
			const auto s = symtab.declaration_scope_of(id_node.guid).value();
			const auto p1 = generalize(t1, s);
			variables[id_node.guid] = p1;
			logger.log(LogLevel::INFO, "Declared \"{}\" as ({})", name, *p1);
			return exp;
		}
			// FIXME: add typing rules
		case NodeType::FUN_DECL: {
			auto opt_type_idx = node[2];
			auto opt_effset_idx = node[3];
			auto body_idx = node[4];

			const auto& id_node = ast.at(node[0]);
			const auto& params_node = ast.at(node[1]);
			const auto& opt_type_node = ast.at(opt_type_idx);
			const auto& effset_node = ast.at(opt_effset_idx);

			assert(id_node.type == NodeType::GUID);

			const auto name =
				std::string_view(pool.find(symtab.name_of(id_node.guid).value()));

			// Start with parameters and output types and variables (or concrete
			// type, if provided)
			//   [t1, ..., tn] -> t
			auto typ = [&]() {
				std::vector<T> inputs {};
				for ([[maybe_unused]] auto param_idx : params_node) {
					inputs.push_back(make_type(make_modevar(), make_datavar()));
				}

				const auto output = (opt_type_node.type == NodeType::EMPTY)
				                    ? make_datavar()
				                    : eval(opt_type_idx);

				const auto effset = (effset_node.type == NodeType::EMPTY)
				                    ? EMPTY_EFFSET
				                    : eval_effset(opt_effset_idx);

				return make_type(VAL_, make_function(inputs, output, effset));
			}();

			variables[id_node.guid] = dont_generalize(typ, {});

			{
				std::vector<T> param_types {};

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
					const auto& param_type_node = ast.at(param[2]);
					const auto mode = [&]() {
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
					auto param_datatype = (param_type_node.type == NodeType::EMPTY)
					                      ? make_datavar()
					                      : eval(param[2]);
					auto var = make_type(mode, param_datatype);
					param_types.push_back(var);
					const auto p1 = dont_generalize(var, {});
					variables[param_name.guid] = p1;
				}

				auto output = [&]() {
					if (opt_type_node.type != NodeType::EMPTY) {
						return eval(opt_type_idx);
					} else {
						return make_datavar();
					}
				}();

				const auto effset = (effset_node.type == NodeType::EMPTY)
				                    ? EMPTY_EFFSET
				                    : eval_effset(opt_effset_idx);

				auto p2 = dont_generalize(
					make_type(VAL_, make_function(param_types, output, effset)), {}
				);

				variables[id_node.guid] = p2;

				auto body_type = typecheck(body_idx);
				if (!unify(body_type.first->datatype, output))
					mismatch_error(
						node.loc,
						"Function annotation output type and infered type don't "
						"match",
						body_type.first,
						make_type(VAL_, output)
					);

				if (not ::cast_to(body_type.second, effset)) {
					logger.log(
						LogLevel::ERROR,
						node.loc,
						"Function declared with ({}) effects, but body had ({})",
						*effset,
						*body_type.second
					);
				}

				const auto captures = [&]() {
					auto mf = symtab.function_of(body_idx);
					assert(mf.has_value());
					auto f = mf.value();
					auto free_symbols = symtab.free_symbols_in(f, symtab.value_namespace);
					logger.log(
						LogLevel::INFO,
						"Function index {} with {} free symbols",
						f.index,
						free_symbols.size()
					);
					std::map<Guid, D> captures {};
					for (auto symbol : free_symbols) {
						// FIXME: dont know if this is the proper way. should captures be
						// instantiated or not?
						const auto capture_name =
							std::string_view(pool.find(symtab.name_of(symbol).value()));
						logger.log(
							LogLevel::INFO,
							"Variable \"{}\" free in definition of \"{}\"",
							capture_name,
							name
						);
						auto t1 = instantiate(variables.at(symbol));
						captures[symbol] = t1->datatype;
					}
					return captures;
				}();

				typ =
					make_type(VAL_, make_closure(param_types, output, effset, captures));
			}

			const auto s = symtab.declaration_scope_of(ast.at(node[0]).guid).value();
			const auto p1 = generalize(typ, s);
			variables[id_node.guid] = p1;

			logger.log(LogLevel::INFO, "Function \"{}\" declare as ({})", name, *p1);

			auto typ_ = typ;
			ASSOC_TYPE(node_idx, typ_);
			return std::make_pair(typ_, EMPTY_EFFSET);
		}
			// |
			// +---------
			// | |- nil : Nil

		case NodeType::NIL: {
			auto t = make_type(VAL_, NIL_);
			ASSOC_TYPE(node_idx, t);
			return std::make_pair(t, EMPTY_EFFSET);
		}

			// |
			// +---------
			// | |- true : Bool
			//
			// |
			// +---------
			// | |- false : Bool
			//
		case NodeType::TRUE:
		case NodeType::FALSE: {
			auto t = make_type(VAL_, make_bool());
			ASSOC_TYPE(node_idx, t);
			return std::make_pair(t, EMPTY_EFFSET);
		}

			// FIXME: add typing rules
		case NodeType::LET: {
			auto decls_idx = node[0];
			auto exp_idx = node[1];

			const auto& decls = ast.at(decls_idx);

			auto joined = EMPTY_EFFSET;
			for (auto decl_idx : decls) {
				auto z = typecheck(decl_idx);
				joined = union_of(joined, z.second);
			}

			auto x = typecheck(exp_idx);
			ASSOC_TYPE(node_idx, x.first);
			return std::make_pair(x.first, union_of(joined, x.second));
		}

			// |
			// +---------
			// | |- c : Uint<8>

		case NodeType::CHAR: {
			auto t = make_type(VAL_, uint8_typ);
			ASSOC_TYPE(node_idx, t);
			return std::make_pair(t, EMPTY_EFFSET);
		}

		case NodeType::PATH: {
			auto t = typecheck(node[0]);
			ASSOC_TYPE(node_idx, t.first);
			return t;
		}
			// FIXME: add typing rules
		case NodeType::AS: {
			auto exp = typecheck(node[0]);
			auto typ_ = make_type(make_modevar(), eval(node[1]));
			if (cast_to(exp.first, typ_)) {
				ASSOC_TYPE(node_idx, typ_);
				return std::make_pair(typ_, exp.second);
			} else {
				mismatch_error(node.loc, "Can't cast value to type", typ_, exp.first);
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
			const auto name =
				std::string_view(pool.find(symtab.name_of(node.guid).value()));
			logger.log(LogLevel::INFO, "Retrieving type for \"{}\"", name);
			assert(symtab.namespace_of(node.guid) == symtab.value_namespace);
			assert(variables.contains(node.guid));
			auto found_typ = variables.at(node.guid);
			assert(found_typ != nullptr);
			const auto t1 = instantiate(found_typ);
			logger.log(
				LogLevel::INFO, "Type of \"{}\" instantiated to {}", name, *t1
			);
			ASSOC_TYPE(node_idx, t1);
			return std::make_pair(t1, EMPTY_EFFSET);
		}
		case NodeType::MODE: assert(false);
		case NodeType::PARAM: assert(false);
		case NodeType::RECORD_LITERAL: {
			auto joined = EMPTY_EFFSET;
			std::vector<std::pair<std::string, D>> field_tys {};
			for (auto field_idx : node) {
				const auto& field_node = ast.at(field_idx);
				assert(field_node.type == NodeType::FIELD);
				const auto t1 = typecheck(field_node[1]);
				joined = union_of(joined, t1.second);
				if (not submode(t1.first->mode, VAL_))
					throw std::runtime_error("submoding error");
				const auto& name_node = ast.at(field_node[0]);
				assert(name_node.type == NodeType::ID);
				const auto name = std::string(pool.find(name_node.str_id));
				field_tys.push_back(std::make_pair(name, t1.first->datatype));
			}
			std::sort(
				field_tys.begin(), field_tys.end(), [](const auto& x, const auto& y) {
					return x.first < y.first;
				}
			);
			const auto d2 = make_record(field_tys);
			const auto t2 = make_type(VAL_, d2);
			ASSOC_TYPE(node_idx, t2);
			return std::make_pair(t2, joined);
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
			ss1 << *t1.first;
			std::ostringstream ss2 {};
			ss2 << *t2;
			logger.log(
				LogLevel::INFO,
				node.loc,
				"unifying value (t1:{}) with row set (t2:{})",
				ss1.str(),
				ss2.str()
			);
			if (not unify(t1.first, t2))
				logger.err(
					node.loc, "cannot access value without field \"{}\"", field_name
				);
			const auto t4 = make_type(t1.first->mode, d3);
			ASSOC_TYPE(node_idx, t4);
			return std::make_pair(t4, t1.second);
		}
		case NodeType::ARRAY_LITERAL: {
			auto t1 = make_datavar();
			std::vector<D> elt_dtys {t1};
			auto joined = EMPTY_EFFSET;
			for (auto elt_idx : node) {
				const auto elt_ty = typecheck(elt_idx);
				joined = union_of(joined, elt_ty.second);
				elt_dtys.push_back(elt_ty.first->datatype);
			}
			for (auto [x, y] : std::views::adjacent<2>(elt_dtys)) {
				if (not unify(x, y))
					logger.err(
						node.loc,
						"array literal contains element expression of different types"
					);
			}
			auto t2 = make_type(VAL_, make_array(t1));
			ASSOC_TYPE(node_idx, t2);
			return std::make_pair(t2, joined);
		}
		case NodeType::REPEAT: {
			const auto t0 = typecheck(node[0]);
			const auto t1 = typecheck(node[1]);
			const auto t2 =
				make_type(make_modevar(), make_integer(64, Integer::Sign::UNSIGNED));
			if (not cast_to(t1.first, t2)) {
				logger.err(
					node.loc, "array expansion second argument must be an integer"
				);
			}
			const auto t3 = make_type(VAL_, make_array(t0.first->datatype));
			ASSOC_TYPE(node_idx, t3);
			return std::make_pair(t3, union_of(t0.second, t1.second));
		}
		case NodeType::LEN: {
			const auto t0 = typecheck(node[0]);
			const auto d0 = make_datavar();
			const auto t1 = make_type(VAL_, make_array(d0));
			if (not unify(t0.first, t1))
				logger.err(
					node.loc, "operand to array length operator (#) was not an array"
				);
			const auto t2 =
				make_type(VAL_, make_integer(64, Integer::Sign::UNSIGNED));
			ASSOC_TYPE(node_idx, t2);
			return std::make_pair(t2, t0.second);
		}
		case NodeType::MODULE_BODY: {
			const auto& decl_list = ast.at(node[0]);
			assert(decl_list.type == NodeType::METALIST);
			for (auto decl : decl_list) {
				const auto& decl_node = ast.at(decl);
				const auto& name = ast.at(decl_name(decl_node));
				// FIXME: wrong!!!
				if (decl_node.type == NodeType::TYPE_DECL) {
				} else if (decl_node.type == NodeType::EFF_DECL) {
				} else if (decl_node.type == NodeType::EFF_ALIAS) {
				} else {
					const auto p = make_polytype(VAL_, make_scheme({}, make_datavar()));
					variables[name.guid] = p;
				}
			}
			auto joined = EMPTY_EFFSET;
			for (auto decl : decl_list) {
				auto t = typecheck(decl);
				joined = union_of(joined, t.second);
			}
			return std::make_pair(make_type(VAL_, VOID_), joined);
		}
		case NodeType::ARRAY_TYPE_LITERAL: assert(false);
		case NodeType::RECORD_TYPE_LITERAL: assert(false);
		case NodeType::FIELD_TYPE: assert(false);
		case NodeType::TYPE_DECL: {
			const auto& id_node = ast.at(node[0]);
			assert(id_node.type == NodeType::GUID);
			const auto data = eval(node[1]);
			assert(not type_names.contains(id_node.guid));
			type_names[id_node.guid] = data;
			return std::make_pair(make_type(VAL_, VOID_), EMPTY_EFFSET);
		}
		case NodeType::EFF_DECL: {
			const auto id_idx = node[0];
			const auto params_idx = node[1];
			const auto resume_type_idx = node[2];
			const auto& id_node = ast.at(id_idx);
			assert(id_node.type == NodeType::GUID);
			const auto name =
				std::string_view(pool.find(symtab.name_of(id_node.guid).value()));
			const auto& params_node = ast.at(params_idx);
			assert(params_node.type == NodeType::METALIST);
			std::vector<T> parameter_types {};
			for (auto param_idx : params_node) {
				const auto& param_node = ast.at(param_idx);
				const auto& param_mode_node = ast.at(param_node[0]);
				const auto mode = [&]() {
					if (param_mode_node.type == NodeType::EMPTY)
						throw std::runtime_error(
							"effect declaration parameter must have mode specified"
						);
					else if (param_mode_node.mode == ModeLiteral::VAL)
						return VAL_;
					else if (param_mode_node.mode == ModeLiteral::VAR)
						return VAR_;
					else if (param_mode_node.mode == ModeLiteral::OUT)
						return OUT_;
					else
						assert(false);
				}();
				const auto& param_type_node = ast.at(param_node[2]);
				assert(param_type_node.type != NodeType::EMPTY);
				const auto param_datatype = eval(param_node[2]);
				parameter_types.push_back(make_type(mode, param_datatype));
			}
			const auto resume_type = eval(resume_type_idx);
			const auto effect = std::make_shared<Effect>(Effect {
				.name = std::string(name),
				.inputs = parameter_types,
				.output = resume_type,
			});
			std::vector<std::shared_ptr<Effect>> xs {{effect}};
			const auto effect_set = std::make_shared<EffectSet>(xs);
			effset_names[id_node.guid] = effect_set;
			return std::make_pair(make_type(VAL_, VOID_), EMPTY_EFFSET);
		}
		case NodeType::EFF_ALIAS: {
			const auto id_idx = node[0];
			const auto union_idx = node[1];
			const auto& id_node = ast.at(id_idx);
			assert(id_node.type == NodeType::GUID);
			const auto effset = eval_effset(union_idx);
			effset_names[id_node.guid] = effset;
			return std::make_pair(make_type(VAL_, VOID_), EMPTY_EFFSET);
		}
		case NodeType::EFF_UNION: assert(false);
	}
	assert(false && "unreachable");
}

std::shared_ptr<Datatype> Typechecker::eval(NodeIndex node_idx) {
	const auto& node = ast.at(node_idx);
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

			const auto& name_node = ast.at(name_idx);
			assert(name_node.type == NodeType::GUID);
			const auto& args_node = ast.at(args_idx);
			const auto name =
				std::string_view(pool.find(symtab.name_of(name_node.guid).value()));

			if (name == "Array") {
				auto elt_typ = eval(args_node[0]);
				return make_array(elt_typ);
			}

			if (name == "Int") {
				auto bit_count = ast.at(args_node[0]).num;
				return make_integer(bit_count, Integer::Sign::SIGNED);
			}

			if (name == "Uint") {
				auto bit_count = ast.at(args_node[0]).num;
				return make_integer(bit_count, Integer::Sign::UNSIGNED);
			}

			// TODO
			assert(false);

			auto general_typ = eval(name_idx);
			std::vector<D> arguments;
			for (auto idx : ast.at(args_idx)) {
				arguments.push_back(eval(idx));
			}

			assert(false);
		}

		case NodeType::ARRAY_TYPE_LITERAL: {
			return make_array(eval(node[0]));
		}
		case NodeType::RECORD_TYPE_LITERAL: {
			const auto d = std::dynamic_pointer_cast<Record>(make_record({}));
			for (const auto field_dty_idx : node) {
				const auto& field_dty = ast.at(field_dty_idx);
				assert(field_dty.type == NodeType::FIELD_TYPE);
				const auto& field_name_node = ast.at(field_dty[0]);
				assert(field_name_node.type == NodeType::ID);
				const auto field_name = std::string(pool.find(field_name_node.str_id));
				const auto dty = eval(field_dty[1]);
				d->fields.push_back(std::make_pair(field_name, dty));
			}
			return d;
		}
		case NodeType::GUID: {
			return type_names.at(node.guid);
		}
		default: assert(false);
	}
}

D Typechecker::substitute(const std::map<std::size_t, V>& subst, D data) {
	return visit(
		*data,
		overloaded {
			[&](Integer&&) -> D { return data; },
			[&](Nil&&) -> D { return data; },
			[&](Void&&) -> D { return data; },
			[&](Function&& f) -> D {
				std::vector<T> inputs {};
				for (auto& input : f.inputs)
					inputs.push_back(
						make_type(input->mode, substitute(subst, input->datatype))
					);
				auto output = substitute(subst, f.output);
				// FIXME: traverse f.effects!
				return make_function(inputs, output, f.effects);
			},
			[&](Closure&& c) -> D {
				std::vector<T> inputs {};
				for (auto& input : c.inputs)
					inputs.push_back(
						make_type(input->mode, substitute(subst, input->datatype))
					);
				auto output = substitute(subst, c.output);
				std::map<Guid, D> captures {};
				for (auto& [guid, capture] : c.captures)
					captures[guid] = substitute(subst, capture);
				// FIXME: traverse c.effects!
				return make_closure(inputs, output, c.effects, captures);
			},
			[&](Record&& r) -> D {
				decltype(r.fields) fields {};
				for (const auto& [name, data] : r.fields)
					fields.push_back(std::make_pair(name, substitute(subst, data)));
				return make_record(fields);
			},
			[&](RowSet&& rs) -> D {
				decltype(rs.rows) rows {};
				for (const auto& [name, data] : rs.rows)
					rows[name] = substitute(subst, data);
				return make_row_set(rows);
			},
			[&](Array&& a) -> D {
				return make_array(substitute(subst, a.item_type));
			},
			[&](TypeVariable&& a) -> D {
				if (a.is_bound
		        and subst.contains(
							std::dynamic_pointer_cast<TypeVariable>(data)->name
						))
					assert(false);
				if (a.is_bound) return substitute(subst, a.bound_type);
				auto t = std::dynamic_pointer_cast<TypeVariable>(data);
				if (subst.contains(a.name))
					return subst.at(a.name);
				else
					return t;
			},
		}
	);
}

template<class T, class Hash = std::hash<T>, class Eq = std::equal_to<T>>
std::unordered_set<T, Hash, Eq> set_difference(
	const std::unordered_set<T, Hash, Eq>& a,
	const std::unordered_set<T, Hash, Eq>& b
) {
	std::unordered_set<T, Hash, Eq> result;

	for (const auto& x : a)
		if (!b.contains(x)) result.insert(x);

	return result;
}

P Typechecker::generalize(T ty, ScopeIndex scope) {
	const auto type_free = free_type_variables(ty->datatype);
	const auto context_free = symtab.parent_of(scope)
	                            .and_then([&](const auto& x) {
																return std::optional(free_type_variables(x));
															})
	                            .value_or(std::unordered_set<V> {});
	const auto to_bind = set_difference(type_free, context_free);
	return generalize(ty, to_bind);
}

P Typechecker::generalize(T ty, std::unordered_set<V> to_bind) {
	std::map<std::size_t, V> subst {};
	std::vector<V> binders {};
	for (auto x : to_bind) {
		const auto g = std::dynamic_pointer_cast<TypeVariable>(make_genvar());
		subst[x->name] = g;
		binders.push_back(g);
	}

	std::ostringstream ss {};
	ss << '[';
	for (const auto& [i, x] : std::ranges::views::enumerate(to_bind)) {
		ss << *x;
		if (static_cast<std::size_t>(i) != (to_bind.size() - 1)) ss << ';';
	}
	ss << ']';

	logger.log(LogLevel::INFO, "Free {}", ss.str());
	logger.log(LogLevel::INFO, "Before {}", *ty->datatype);
	const auto s = make_scheme(binders, substitute(subst, ty->datatype));
	logger.log(LogLevel::INFO, "After {}", *ty->datatype);
	const auto p = make_polytype(ty->mode, s);

	logger.log(
		LogLevel::INFO,
		"Generalizing monotype ({}) into polytype ({}). free variables {}",
		*ty,
		*p,
		ss.str()
	);
	return p;
}

std::unordered_set<V> Typechecker::free_type_variables(ScopeIndex s) {
	auto symbols = symtab.symbols_reachable_from(s, symtab.value_namespace);
	std::unordered_set<V> free_variables {};
	for (auto symbol : symbols) {
		const auto name =
			std::string_view(pool.find(symtab.name_of(symbol).value()));
		logger.log(LogLevel::INFO, "Extracting free variables of \"{}\"", name);
		auto ty = variables.at(symbol);
		auto free = free_type_variables(ty->scheme->body);
		free_variables.merge(free);
	}
	return free_variables;
}

T Typechecker::instantiate(P pty) {
	std::map<std::size_t, V> xm {};
	for (auto x : pty->scheme->quantified)
		xm[x->name] = std::dynamic_pointer_cast<TypeVariable>(make_datavar());
	auto t = make_type(pty->mode, substitute(xm, pty->scheme->body));
	logger.log(LogLevel::INFO, "Instantiated ({}) into ({})", *pty, *t);
	return t;
}

std::unordered_set<V> Typechecker::free_type_variables(D data) {
	return visit(
		*data,
		overloaded {
			[](Integer&&) -> std::unordered_set<V> { return {}; },
			[](Nil&&) -> std::unordered_set<V> { return {}; },
			[](Void&&) -> std::unordered_set<V> { return {}; },
			[&](Function&& f) -> std::unordered_set<V> {
				std::unordered_set<V> set {};
				for (const auto& input : f.inputs)
					set.merge(free_type_variables(input->datatype));
				set.merge(free_type_variables(f.output));
				return set;
			},
			[&](Closure&& c) -> std::unordered_set<V> {
				std::unordered_set<V> set {};
				for (const auto& input : c.inputs)
					set.merge(free_type_variables(input->datatype));
				set.merge(free_type_variables(c.output));
				for (const auto& [name, data] : c.captures)
					set.merge(free_type_variables(data));
				return set;
			},
			[&](Record&& r) -> std::unordered_set<V> {
				std::unordered_set<V> set {};
				for (const auto& [name, data] : r.fields)
					set.merge(free_type_variables(data));
				return set;
			},
			[&](RowSet&& rs) -> std::unordered_set<V> {
				std::unordered_set<V> set {};
				for (const auto& [name, data] : rs.rows)
					set.merge(free_type_variables(data));
				return set;
			},
			[&](Array&& a) -> std::unordered_set<V> {
				return free_type_variables(a.item_type);
			},
			[&](TypeVariable&& a) -> std::unordered_set<V> {
				if (a.is_generalized) return {};
				if (a.is_bound) return free_type_variables(a.bound_type);
				std::unordered_set<V> set {};
				set.insert(std::dynamic_pointer_cast<TypeVariable>(data));
				return set;
			},
		}
	);
}

P Typechecker::dont_generalize(T ty, ScopeIndex) {
	return make_polytype(ty->mode, make_scheme({}, ty->datatype));
}
