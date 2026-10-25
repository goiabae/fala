#ifndef HIR_TYPE_HPP
#define HIR_TYPE_HPP

#include <memory>

#include "type.hpp"

namespace hir {

struct HType;

struct HTPtr {
	friend struct HTContext;

 private:
	std::shared_ptr<HType> key;
};

struct HInteger {
	enum class Sign { SIGNED, UNSIGNED };
	int bit_count;
	Sign sign;
};

struct HNil {};
struct HVoid {};

// FIXME: add arguments/result information
struct HContinuation {};

struct HClosure {
	enum class Mode {
		SAME_MUTABLE,
		NEW_MUTABLE,
		IMMUTABLE,
	};
	std::vector<std::pair<Mode, HTPtr>> inputs;
	HTPtr output;
	std::vector<HTPtr> captures;
};

struct HArray {
	HTPtr element;
};

struct HTuple {
	std::vector<HTPtr> elements;
};

struct HUnion {
	struct Constructor {
		std::vector<HTPtr> arguments;
	};
	// position of the constructor in the vector represents the tag used to match
	// it
	std::vector<Constructor> constructors;
};

struct HAny {};

struct HGeneric {
	std::size_t name;
};

struct HType {
	std::variant<
		HInteger, HNil, HVoid, HContinuation, HClosure, HArray, HTuple, HUnion,
		HAny, HGeneric>
		data;
};

struct HTContext {
	HType& at(const HTPtr&);
	HTPtr alloc(const HType&);
};

auto translate(
	const std::shared_ptr<Datatype>&, HTContext&,
	const std::map<Guid, std::shared_ptr<Datatype>>&
) -> HTPtr;

} // namespace hir

#endif
