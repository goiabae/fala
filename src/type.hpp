#ifndef TYPE_HPP
#define TYPE_HPP

#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "variable.hpp"

struct Type;
struct Datatype;
struct Mode;

struct Datatype {
	virtual ~Datatype() = default;
	virtual size_t size_of() = 0;
	virtual std::ostream& print(std::ostream&) const = 0;
};

struct Integer;
struct Nil;
struct Void;
struct Function;
struct TypeVariable;
struct Array;
struct Toat;
struct General;
struct Record;
struct RowSet;

#define DERIVE(...)                                        \
	Datatype {                                               \
		size_t size_of() override;                             \
		std::ostream& print(std::ostream & st) const override; \
		__VA_ARGS__                                            \
	}

// clang-format off

struct Integer final : DERIVE (
	enum class Sign { SIGNED, UNSIGNED };
	int bit_count;
	Sign sign;
);

struct Nil final : DERIVE();
struct Void final : DERIVE();
struct Toat final : DERIVE(); // top type

struct Function final : DERIVE (
	std::vector<std::shared_ptr<Type>> inputs;
	std::shared_ptr<Datatype> output;
);

struct TypeVariable final : DERIVE (
	TypeVariable(std::size_t name) : unbound_name(name) {}
	std::size_t unbound_name {};
	std::shared_ptr<Datatype> bound_type {nullptr};
	bool is_bound {false};
	void bind_to(std::shared_ptr<Datatype> t);
);

struct Array final : DERIVE (
	std::shared_ptr<Datatype> item_type;
);

struct General final : DERIVE (
	std::vector<std::shared_ptr<Datatype>> vars;
	std::shared_ptr<Datatype> body;
);

struct Record final : DERIVE (
	std::vector<std::pair<std::string, std::shared_ptr<Datatype>>> fields;
);

struct RowSet final : DERIVE (
	std::map<std::string, std::shared_ptr<Datatype>> rows;
);

// clang-format on

struct Mode {
	enum class Concrete { VAL, VAR, OUT };
	std::variant<Concrete, Variable<Mode>> data;
	std::optional<Concrete> to_concrete();
};

struct Type {
	std::shared_ptr<Mode> mode;
	std::shared_ptr<Datatype> datatype;
};

std::shared_ptr<Datatype> get_datatype(std::shared_ptr<Type>);

inline bool mode_is_assignable(Mode::Concrete m) {
	return m == Mode::Concrete::VAL or m == Mode::Concrete::OUT;
}

std::ostream& operator<<(std::ostream&, const Type&);
std::ostream& operator<<(std::ostream&, const Datatype&);
std::ostream& operator<<(std::ostream&, const Mode&);
std::ostream& operator<<(std::ostream&, Mode::Concrete);

#undef DERIVE

#endif
