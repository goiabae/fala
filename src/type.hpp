#ifndef TYPE_HPP
#define TYPE_HPP

#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

#include "ast.hpp"
#include "variable.hpp"

#define M std::shared_ptr<Mode>
#define D std::shared_ptr<Datatype>
#define T std::shared_ptr<Type>
#define E std::shared_ptr<EffectSet>

#define DERIVE(...)                                        \
	Datatype {                                               \
		size_t size_of() override;                             \
		std::ostream& print(std::ostream & st) const override; \
		__VA_ARGS__                                            \
	}

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
struct Closure;
struct TypeVariable;
struct Array;
struct Record;
struct RowSet;

struct EffectSet;

// clang-format off

struct Integer final : DERIVE (
	enum class Sign { SIGNED, UNSIGNED };
	int bit_count;
	Sign sign;
);

struct Nil final : DERIVE();
struct Void final : DERIVE();

struct Function final : DERIVE (
	std::vector<std::shared_ptr<Type>> inputs;
	std::shared_ptr<Datatype> output;
	std::shared_ptr<EffectSet> effects;
);

struct Closure final : DERIVE (
	std::vector<std::shared_ptr<Type>> inputs;
	std::shared_ptr<Datatype> output;
	std::shared_ptr<EffectSet> effects;
	std::map<Guid, D> captures;
);

struct TypeVariable final : DERIVE (
	TypeVariable(std::size_t name, bool is_generalized)
	: name(name), is_generalized(is_generalized) {}

	std::size_t name;
	bool is_generalized;
	std::shared_ptr<Datatype> bound_type {nullptr};
	bool is_bound {false};
	void bind_to(std::shared_ptr<Datatype> t);
);

struct Array final : DERIVE (
	std::shared_ptr<Datatype> item_type;
);

struct Record final : DERIVE (
	std::vector<std::pair<std::string, std::shared_ptr<Datatype>>> fields;
);

struct RowSet final : DERIVE (
	std::map<std::string, std::shared_ptr<Datatype>> rows;
);

// clang-format on

struct DataScheme {
	DataScheme(
		std::vector<std::shared_ptr<TypeVariable>> quantified,
		std::shared_ptr<Datatype> body
	)
	: quantified(quantified), body(body) {}

	std::vector<std::shared_ptr<TypeVariable>> quantified;
	std::shared_ptr<Datatype> body;
};

struct Mode {
	enum class Concrete { VAL, VAR, OUT };
	std::variant<Concrete, Variable<Mode>> data;
	std::optional<Concrete> to_concrete();
};

struct Type {
	std::shared_ptr<Mode> mode;
	std::shared_ptr<Datatype> datatype;
};

struct PolyType {
	std::shared_ptr<Mode> mode;
	std::shared_ptr<DataScheme> scheme;
};

struct Effect {
	std::string name;
	std::vector<std::shared_ptr<Type>> inputs;
	std::shared_ptr<Datatype> output;
};

struct EffectSet {
	EffectSet(std::size_t index)
	: concrete {}, variable(std::make_shared<Variable<EffectSet>>(index)) {}
	EffectSet(std::vector<std::shared_ptr<Effect>> concrete)
	: concrete(concrete), variable(nullptr) {}
	EffectSet(
		std::vector<std::shared_ptr<Effect>> concrete,
		std::shared_ptr<Variable<EffectSet>> variable
	)
	: concrete(concrete), variable(variable) {}
	std::vector<std::shared_ptr<Effect>> concrete;
	std::shared_ptr<Variable<EffectSet>> variable;
};

std::shared_ptr<Datatype> get_datatype(std::shared_ptr<Type>);

inline bool mode_is_assignable(Mode::Concrete m) {
	return m == Mode::Concrete::VAL or m == Mode::Concrete::OUT;
}

std::ostream& operator<<(std::ostream&, const Type&);
std::ostream& operator<<(std::ostream&, const Datatype&);
std::ostream& operator<<(std::ostream&, const Mode&);
std::ostream& operator<<(std::ostream&, Mode::Concrete);
std::ostream& operator<<(std::ostream&, const PolyType&);
std::ostream& operator<<(std::ostream&, const DataScheme&);
std::ostream& operator<<(std::ostream& st, const EffectSet& set);
std::ostream& operator<<(std::ostream& st, const Effect& eff);

M make_modevar(std::size_t index);
D make_datavar(std::size_t index);
D make_integer(int bit_count, Integer::Sign sign);
D make_array(D item);
D make_function(std::vector<T> inputs, D output, E effects);
D make_closure(
	std::vector<T> inputs, D output, E effects, std::map<Guid, D> captures
);
D make_bool();
D make_record(std::vector<std::pair<std::string, D>> field_tys);
D make_row_set(std::map<std::string, std::shared_ptr<Datatype>>);
T make_type(M mode, D datatype);
std::shared_ptr<PolyType> make_polytype(M mode, std::shared_ptr<DataScheme>);
std::shared_ptr<DataScheme> make_scheme(
	std::vector<std::shared_ptr<TypeVariable>>, D
);
E make_effsetvar(std::size_t index);

std::shared_ptr<Function> erase_function_captures(D);

#define DEFFMT(T)                                                 \
	template<>                                                      \
	struct std::formatter<T> : std::formatter<std::string> {        \
		auto format(const T& p, std::format_context& ctx) const {     \
			std::ostringstream oss {};                                  \
			oss << p;                                                   \
			return std::formatter<std::string>::format(oss.str(), ctx); \
		}                                                             \
	};

DEFFMT(Type);
DEFFMT(Datatype);
DEFFMT(Mode);
DEFFMT(Mode::Concrete);
DEFFMT(PolyType);
DEFFMT(DataScheme);
DEFFMT(EffectSet);
DEFFMT(Effect);

#undef M
#undef D
#undef T
#undef E
#undef DEFFMT
#undef DERIVE

#endif
