#include "hir_type.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

#include "type.hpp"
#include "utils.hpp"

namespace hir {

static auto translate_recursive(
	const std::shared_ptr<Datatype>& styp, HTContext& hctx,
	const std::map<Guid, std::shared_ptr<Datatype>>& sctx,
	std::optional<Guid> fixpoint
) -> HTPtr;

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
	} else if (auto x = dynamic_cast<Union*>(&obj)) {
		return f(std::forward<Union>(*x));
	} else if (auto x = dynamic_cast<Named*>(&obj)) {
		return f(std::forward<Named>(*x));
	} else {
		assert(false && "no more datatype subclasses!!!");
	}
}

static auto translate(const Integer::Sign& sign) -> HInteger::Sign {
	switch (sign) {
		case Integer::Sign::SIGNED: return HInteger::Sign::SIGNED;
		case Integer::Sign::UNSIGNED: return HInteger::Sign::UNSIGNED;
	}
	std::unreachable();
}

static auto concrete_or_val(const std::shared_ptr<Mode>& m) -> Mode::Concrete {
	if (auto c = std::get_if<Mode::Concrete>(&m->data)) {
		return *c;
	} else if (auto v = std::get_if<Variable<Mode>>(&m->data)) {
		if (v->is_bound()) {
			return concrete_or_val(v->get_bound());
		} else {
			return Mode::Concrete::VAL;
		}
	} else {
		std::unreachable();
	}
}

auto translate(
	const std::shared_ptr<Datatype>& styp, HTContext& hctx,
	const std::map<Guid, std::shared_ptr<Datatype>>& sctx
) -> HTPtr {
	return translate_recursive(styp, hctx, sctx, std::nullopt);
}

auto translate_recursive(
	const std::shared_ptr<Datatype>& styp, HTContext& hctx,
	const std::map<Guid, std::shared_ptr<Datatype>>& sctx,
	std::optional<Guid> fixpoint
) -> HTPtr {
	return visit(
		*styp,
		overloaded {
			[&](Integer&& i) {
				HInteger hi {
					.bit_count = i.bit_count,
					.sign = translate(i.sign),
				};
				return hctx.alloc(HType {hi});
			},
			[&](Nil&&) { return hctx.alloc(HType {HNil {}}); },
			[&](Void&&) { return hctx.alloc(HType {HVoid {}}); },
			[&](Named&& n) {
				// FIXME: the current might be referenced from inside this new, named
		    // type
		    // FIXME: give this a proper type, rather than any
				if (n.name == fixpoint)
					return hctx.alloc(HType {HAny {}});
				else
					return translate_recursive(sctx.at(n.name), hctx, sctx, n.name);
			},
			[&](Function&& f) {
				std::vector<std::pair<HClosure::Mode, HTPtr>> inputs;
				HTPtr output;
				for (const auto& input : f.inputs) {
					auto data =
						translate_recursive(input->datatype, hctx, sctx, fixpoint);
					auto mode = concrete_or_val(input->mode);
					if (mode == Mode::Concrete::VAL) {
						inputs.push_back(std::make_pair(HClosure::Mode::IMMUTABLE, data));
					} else if (mode == Mode::Concrete::VAR) {
						inputs.push_back(std::make_pair(HClosure::Mode::NEW_MUTABLE, data));
					} else if (mode == Mode::Concrete::OUT) {
						inputs.push_back(
							std::make_pair(HClosure::Mode::SAME_MUTABLE, data)
						);
					}
				}
				output = translate_recursive(f.output, hctx, sctx, fixpoint);
				HClosure c {
					.inputs = inputs,
					.output = output,
					.captures = {},
				};
				return hctx.alloc(HType {c});
			},
			[&](Closure&& f) {
				std::vector<std::pair<HClosure::Mode, HTPtr>> inputs;
				HTPtr output;
				std::vector<HTPtr> captures;
				for (const auto& input : f.inputs) {
					auto data =
						translate_recursive(input->datatype, hctx, sctx, fixpoint);
					auto mode = concrete_or_val(input->mode);
					if (mode == Mode::Concrete::VAL) {
						inputs.push_back(std::make_pair(HClosure::Mode::IMMUTABLE, data));
					} else if (mode == Mode::Concrete::VAR) {
						inputs.push_back(std::make_pair(HClosure::Mode::NEW_MUTABLE, data));
					} else if (mode == Mode::Concrete::OUT) {
						inputs.push_back(
							std::make_pair(HClosure::Mode::SAME_MUTABLE, data)
						);
					}
				}
				output = translate_recursive(f.output, hctx, sctx, fixpoint);
				for (const auto& [name, capture] : f.captures) {
					captures.push_back(translate_recursive(capture, hctx, sctx, name));
				}
				HClosure c {
					.inputs = inputs,
					.output = output,
					.captures = captures,
				};
				return hctx.alloc(HType {c});
			},
			[&](Record&& r) {
				std::vector<HTPtr> elements;
				for (const auto& field : r.fields) {
					elements.push_back(
						translate_recursive(field.second, hctx, sctx, fixpoint)
					);
				}
				HTuple t {
					.elements = elements,
				};
				return hctx.alloc(HType {t});
			},
			[&](RowSet&& rs) {
				assert(rs.bound_record != nullptr);
				if (not rs.bound_record->has_value()) {
					throw std::runtime_error("");
				}
				return translate_recursive(
					rs.bound_record->value(), hctx, sctx, fixpoint
				);
			},
			[&](Array&& a) {
				auto element = translate_recursive(a.item_type, hctx, sctx, fixpoint);
				HArray ha {
					.element = element,
				};
				return hctx.alloc(HType {ha});
			},
			[&](Union&& u) {
				std::vector<HUnion::Constructor> constructors;
				for (const auto& ctor : u.constructors) {
					std::vector<HTPtr> arguments;
					for (const auto& argument : ctor.arguments) {
						arguments.push_back(
							translate_recursive(argument, hctx, sctx, fixpoint)
						);
					}
					HUnion::Constructor hctor {.name = ctor.name, .arguments = arguments};
					constructors.push_back(hctor);
				}
				HUnion hu {.constructors = constructors};
				return hctx.alloc(HType {hu});
			},
			[&](TypeVariable&& tv) {
				if (tv.is_generalized) {
					return hctx.alloc(HType {HGeneric {.name = tv.name}});
				}
				if (not tv.is_bound)
					throw std::runtime_error("inference variable not bound");
				return translate_recursive(tv.bound_type, hctx, sctx, fixpoint);
			},
		}
	);
}

HType& HTContext::at(const HTPtr& ptr) { return *ptr.key; }

HType const& HTContext::at(const HTPtr& ptr) const {
	return const_cast<HTContext*>(this)->at(ptr);
}

HTPtr HTContext::alloc(const HType& htyp) {
	auto x = std::make_shared<HType>(htyp);
	HTPtr p;
	p.key = x;
	return p;
}

} // namespace hir
