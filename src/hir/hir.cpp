#include "hir.hpp"

#include <cassert>
#include <cstdio>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <variant>

#include "hir/hir_type.hpp"
#include "str_pool.h"
#include "utils.hpp"

namespace hir {

static void print_operand(
	FILE* fd, Operand opnd, const StringPool& pool, int spaces, HTContext& ctx
);
static void print_instruction(
	FILE* fd, const Instruction& inst, const StringPool& pool, int spaces,
	HTContext& ctx
);
static void print_type(
	FILE* fd, const HTPtr& hptr, const StringPool& pool, int spaces,
	HTContext& ctx
);

Code operator+(Code pre, Code post) {
	Code res {};

	for (auto inst : pre.instructions) res.instructions.push_back(inst);
	for (auto inst : post.instructions) res.instructions.push_back(inst);

	return res;
}

void Code::if_(
	hir::Register result, hir::Operand condition, hir::Region then_branch,
	hir::Region else_branch
) {
	assert(not result.is_mutable);
	instructions.push_back(
		Instruction {
			.opcode = Opcode::IF,
			.operands = {result, condition, then_branch, else_branch},
		}
	);
}

void Code::yield(hir::Operand value) {
	instructions.push_back(
		Instruction {
			.opcode = Opcode::YIELD,
			.operands = {value},
		}
	);
}

void Code::call(
	hir::Register result, hir::Operand function,
	std::vector<hir::Operand> arguments
) {
	std::vector<hir::Operand> operands {};
	operands.push_back(result);
	operands.push_back(function);
	for (const auto& arg : arguments) {
		operands.push_back(arg);
	}
	instructions.push_back(Instruction {hir::Opcode::CALL, operands});
}

void Code::resume(
	hir::Register result, hir::Register continuation, hir::Operand value
) {
	instructions.push_back(
		Instruction {hir::Opcode::RESUME, {result, continuation, value}}
	);
}

void Code::perform(
	hir::Register result, hir::Label effect_tag,
	std::vector<hir::Operand> arguments
) {
	std::vector<hir::Operand> operands {};
	operands.push_back(result);
	operands.push_back(effect_tag);
	for (const auto& arg : arguments) {
		operands.push_back(arg);
	}
	instructions.push_back(Instruction {hir::Opcode::PERFORM, operands});
}

void Code::store(hir::Register a, hir::Register b) {
	instructions.push_back(Instruction {hir::Opcode::STORE, {a, b}});
}

void Code::alloc(hir::Register a, hir::Register b) {
	instructions.push_back(Instruction {hir::Opcode::ALLOC, {a, b}});
}

void Code::clone(hir::Register a, hir::Register b) {
	instructions.push_back(Instruction {hir::Opcode::CLONE, {a, b}});
}

void Code::copy(hir::Register destination, hir::Operand source) {
	instructions.push_back(
		Instruction {hir::Opcode::COPY, {destination, source}}
	);
}

void Code::get_global(hir::Register result, hir::Label name) {
	instructions.push_back(Instruction {hir::Opcode::GET_GLOBAL, {result, name}});
}

void Code::if_false(hir::Operand condition, hir::Region block) {
	instructions.push_back(
		Instruction {hir::Opcode::IF_FALSE, {condition, block}}
	);
}

void Code::if_true(hir::Operand condition, hir::Region block) {
	instructions.push_back(
		Instruction {hir::Opcode::IF_TRUE, {condition, block}}
	);
}

void Code::loop(hir::Label next, hir::Label stop, hir::Region block) {
	instructions.push_back(Instruction {hir::Opcode::LOOP, {next, stop, block}});
}

void Code::brake(hir::Label where_to) {
	instructions.push_back(Instruction {hir::Opcode::BREAK, {where_to}});
}

void Code::continue_(hir::Label where_to) {
	instructions.push_back(Instruction {hir::Opcode::CONTINUE, {where_to}});
}

void Code::equals(hir::Register result, hir::Operand a, hir::Operand b) {
	instructions.push_back(Instruction {hir::Opcode::EQ, {result, a, b}});
}

void Code::add(hir::Register a, hir::Operand b, hir::Operand c) {
	instructions.push_back(Instruction {hir::Opcode::ADD, {a, b, c}});
}

void Code::set_element(
	hir::Register aggregate, std::vector<hir::Operand> indexes, hir::Operand value
) {
	std::vector<hir::Operand> operands;
	operands.push_back(aggregate);
	for (const auto& x : indexes) operands.push_back(x);
	operands.push_back(value);
	instructions.push_back(Instruction {hir::Opcode::SET_ELEMENT, operands});
}

void Code::load(hir::Register r, hir::Register v) {
	instructions.push_back(Instruction {hir::Opcode::LOAD, {r, v}});
}

void Code::get_element(hir::Register a, hir::Register b, hir::Operand c) {
	instructions.push_back(Instruction {hir::Opcode::GET_ELEMENT, {a, b, c}});
}

void print_operand(
	FILE* fd, Operand opnd, const StringPool& pool, int spaces, HTContext& ctx
) {
	switch (opnd.kind) {
		case Operand::Kind::INVALID: assert(false);
		case Operand::Kind::REGISTER: {
			if (opnd.registuhr.name.has_value()) {
				if (opnd.registuhr.is_mutable) {
					fprintf(fd, "$%s", opnd.registuhr.name.value().c_str());
				} else {
					fprintf(fd, "%%%s", opnd.registuhr.name.value().c_str());
				}
			} else {
				if (opnd.registuhr.is_mutable) {
					fprintf(fd, "$%zu", opnd.registuhr.id);
				} else {
					fprintf(fd, "%%%zu", opnd.registuhr.id);
				}
			}
			return;
		}
		case Operand::Kind::BLOCK: {
			assert(opnd.block.body_code != nullptr);
			const auto& code = *opnd.block.body_code;
			print_code(fd, code, pool, spaces + 2, ctx);
			return;
		}
		case Operand::Kind::LABEL: {
			fprintf(fd, "@%s", opnd.label.name.c_str());
			return;
		}
		case Operand::Kind::REGION: {
			const auto& r = opnd.region;
			if (r.parameter_registers.size() > 0) {
				fprintf(fd, "(");
				for (const auto& [i, p] :
				     std::ranges::views::enumerate(r.parameter_registers)) {
					print_operand(fd, p, pool, spaces, ctx);
					fprintf(fd, ": ");
					print_type(fd, p.htype, pool, spaces, ctx);
					if (static_cast<std::size_t>(i)
					    != (r.parameter_registers.size() - 1)) {
						fprintf(fd, ", ");
					}
				}
				fprintf(fd, ") ");
			}
			fprintf(fd, "@%s {\n", r.entry_block.c_str());
			auto j = 0ul;
			for (const auto& [n, b] : r.blocks) {
				for (int i = 0; i < spaces + 2; i++) fprintf(fd, " ");
				fprintf(fd, "@%s:\n", n.c_str());
				print_operand(fd, b, pool, spaces, ctx);
				if (j != (r.blocks.size() - 1)) fprintf(fd, "\n");
				j++;
			}
			for (int i = 0; i < spaces; i++) fprintf(fd, " ");
			fprintf(fd, "}");
			return;
		}
		case Operand::Kind::IMMEDIATE: {
			fprintf(fd, "%ld", opnd.immediate.value);
			return;
		}
	}
}

void print_type(
	FILE* fd, const HTPtr& hptr, const StringPool& pool, int spaces,
	HTContext& ctx
) {
	const auto& htype = ctx.at(hptr);
	std::visit(
		overloaded {
			[&](const HInteger& i) {
				if (i.sign == HInteger::Sign::UNSIGNED) {
					fprintf(fd, "u%d", i.bit_count);
				} else {
					fprintf(fd, "s%d", i.bit_count);
				}
			},
			[&](const HNil&) { fprintf(fd, "nil"); },
			[&](const HVoid&) { fprintf(fd, "void"); },
			[&](const HContinuation&) { fprintf(fd, "cont"); },
			[&](const HClosure&) { fprintf(fd, "clos"); },
			[&](const HArray& a) {
				fprintf(fd, "[]");
				print_type(fd, a.element, pool, spaces, ctx);
			},
			[&](const HTuple& t) {
				fprintf(fd, "(");
				for (auto i = 0ul; i < t.elements.size(); i++) {
					print_type(fd, t.elements.at(i), pool, spaces, ctx);
					if (i != (t.elements.size() - 1)) {
						fprintf(fd, "; ");
					}
				}
				fprintf(fd, ")");
			},
			[&](const HUnion&) { fprintf(fd, "union"); },
			[&](const HAny&) { fprintf(fd, "any"); },
			[&](const HGeneric& g) {
				if (g.name <= 26) {
					fprintf(fd, "'%c", 'a' + (char)g.name);
				} else {
					fprintf(fd, "'%zu", g.name);
				}
			}
		},
		htype.data
	);
}

void print_instruction(
	FILE* fd, const Instruction& inst, const StringPool& pool, int spaces,
	HTContext& ctx
) {
#define A(N, C, R, I)                                                        \
	{                                                                          \
		auto c = inst.operands.size();                                           \
		if (C >= 0 and c != C)                                                   \
			throw std::runtime_error(                                              \
				std::format(                                                         \
					"instruction with wrong operand amount. expected {}. got {}", C, c \
				)                                                                    \
			);                                                                     \
		if (I) {                                                                 \
			assert(C == 3);                                                        \
			print_operand(fd, inst.operands[0], pool, spaces, ctx);                \
			fprintf(fd, ": ");                                                     \
			print_type(fd, inst.operands[0].registuhr.htype, pool, spaces, ctx);   \
			fprintf(fd, " = ");                                                    \
			print_operand(fd, inst.operands[1], pool, spaces, ctx);                \
			fprintf(fd, " ");                                                      \
			fprintf(fd, "%s", N);                                                  \
			fprintf(fd, " ");                                                      \
			print_operand(fd, inst.operands[2], pool, spaces, ctx);                \
		} else if (C > 0 or C < 0) {                                             \
			if (R) {                                                               \
				print_operand(fd, inst.operands[0], pool, spaces, ctx);              \
				fprintf(fd, ": ");                                                   \
				print_type(fd, inst.operands[0].registuhr.htype, pool, spaces, ctx); \
				fprintf(fd, " = ");                                                  \
				fprintf(fd, "%s", N);                                                \
				if (c > 1) fprintf(fd, " ");                                         \
			} else {                                                               \
				fprintf(fd, "%s", N);                                                \
				fprintf(fd, " ");                                                    \
				print_operand(fd, inst.operands[0], pool, spaces, ctx);              \
				if (C > 1) fprintf(fd, " ");                                         \
			}                                                                      \
			if (C > 0) {                                                           \
				for (auto i = 1; i < C; i++) {                                       \
					print_operand(fd, inst.operands[i], pool, spaces, ctx);            \
					if (i != (C - 1)) fprintf(fd, " ");                                \
				}                                                                    \
			} else {                                                               \
				for (auto i = 1ul; i < inst.operands.size(); i++) {                  \
					print_operand(fd, inst.operands[i], pool, spaces, ctx);            \
					if (i != (inst.operands.size() - 1)) fprintf(fd, " ");             \
				}                                                                    \
			}                                                                      \
		}                                                                        \
		fprintf(fd, "\n");                                                       \
		return;                                                                  \
	}                                                                          \
	(void)0

#define BINARY_INST(PRETTY) A(PRETTY, 3, true, true)
#define S(N, C) A(N, C, false, false)
#define E(N, C) A(N, C, true, false)

	for (int i = 0; i < spaces; i++) fprintf(fd, " ");
	switch (inst.opcode) {
		case Opcode::PRINTF: S("printf", 1);
		case Opcode::PRINTV: S("printvf", 1);
		case Opcode::PRINTC: S("printvc", 1);
		case Opcode::COPY: E("", 2);
		case Opcode::ADD: BINARY_INST("+");
		case Opcode::SUB: BINARY_INST("-");
		case Opcode::MUL: BINARY_INST("*");
		case Opcode::DIV: BINARY_INST("/");
		case Opcode::MOD: BINARY_INST("%%");
		case Opcode::OR: BINARY_INST("or");
		case Opcode::AND: BINARY_INST("and");
		case Opcode::EQ: BINARY_INST("==");
		case Opcode::NEQ: BINARY_INST("!=");
		case Opcode::LESS: BINARY_INST("<");
		case Opcode::LESS_EQ: BINARY_INST("<=");
		case Opcode::GREATER: BINARY_INST(">=");
		case Opcode::GREATER_EQ: BINARY_INST(">=");
		case Opcode::NOT: E("not", 2);
		case Opcode::GET_ELEMENT: E("get_element", 3);
		case Opcode::SET_ELEMENT: S("set_element", -1);
		case Opcode::CALL: E("call", -1);
		case Opcode::RET: S("ret", -1);
		case Opcode::LOOP: S("loop", 3);
		case Opcode::IF_TRUE: S("if_true", 2);
		case Opcode::IF_FALSE: S("if_false", 2);
		case Opcode::BREAK: S("break", 1);
		case Opcode::CONTINUE: S("continue", 1);
		case Opcode::LOAD: E("load", 2);
		case Opcode::STORE: S("store", 2);
		case Opcode::ALLOC: E("alloc", 2);
		case Opcode::CLONE: E("clone", 2);
		case Opcode::ALIAS: E("alias", 2);
		case Opcode::GET_GLOBAL: E("get_global", 2);
		case Opcode::CATCH: E("catch", 4);
		case Opcode::PERFORM: E("perform", -1);
		case Opcode::RESUME: E("resume", 3);
		case Opcode::REPEAT: E("repeat", 3);
		case Opcode::MAKE_TUPLE: E("make_tuple", -1);
		case Opcode::ENCLOSE: E("enclose", 3);
		case Opcode::MAKE_ARRAY: E("make_array", -1);
		case Opcode::IF: E("if", 4);
		case Opcode::YIELD: S("yield", 1);
		case Opcode::GET_CONSTRUCTOR: E("get_constructor", 2);
		case Opcode::GET_VARIANT: E("get_variant", 3);
		case Opcode::PANIC: S("panic", 0);
		case Opcode::MAKE_VARIANT: E("make_variant", 3);
		case Opcode::LENGTH: E("length", 2);
	}

#undef BINARY_INST
#undef S
#undef E
#undef A
}

void print_code(
	FILE* fd, const Code& code, const StringPool& pool, int spaces, HTContext& ctx
) {
	size_t i = 0;
	for (i = 0; i < code.size(); i++) {
		print_instruction(fd, code.at(i), pool, spaces, ctx);
	}
}

void Module::load_builtin(hir::Label name) {
	if (not static_symbols.contains(name.name)) {
		static_symbols[name.name] =
			std::make_pair(Visibility::HIDDEN, BuiltinLoad {name.name});
	}
}

hir::Label Module::register_constant(
	hir::Label name, Visibility visibility, const Constant& constant
) {
	for (const auto& [symbol, v] : static_symbols) {
		const auto& init = std::get<1>(v);
		if (std::holds_alternative<Constant>(init)) {
			const auto& c = std::get<Constant>(init);
			const auto are_equal = std::visit(
				overloaded {
					[](const hir::Integer& a, const hir::Integer& b) {
						return a.integer == b.integer;
					},
					[](const hir::String& a, const hir::String& b) {
						return a.str_id == b.str_id;
					},
					[](const hir::Character& a, const hir::Character& b) {
						return a.character == b.character;
					},
					[](const hir::Nil&, const hir::Nil&) { return true; },
					[](const hir::Boolean& a, const hir::Boolean& b) {
						return a.boolean == b.boolean;
					},
					[](const hir::Function&, const hir::Function&) {
						// FIXME: how to compare functions?
						return false;
					},
					[](const auto&, const auto&) { return false; }
				},
				c,
				constant
			);
			if (are_equal) return hir::Label {symbol};
		} else {
			if (symbol == name.name) {
				throw std::runtime_error(
					std::format(
						"static symbol {} already declared as another kind", symbol
					)
				);
			}
		}
	}
	static_symbols[name.name] = std::make_pair(visibility, constant);
	return name;
}

static void myprint(
	FILE* fd, const hir::Integer& i, const StringPool&, int, HTContext&
) {
	fprintf(fd, "%ld", i.integer);
}

static void myprint(
	FILE* fd, const hir::Character& c, const StringPool&, int, HTContext&
) {
	fprintf(fd, "'%c'", c.character);
}

static void myprint(
	FILE* fd, const hir::Boolean& b, const StringPool&, int, HTContext&
) {
	fprintf(fd, "%s", b.boolean ? "true" : "false");
}

static void myprint(
	FILE* fd, const hir::String& s, const StringPool& pool, int, HTContext&
) {
	auto str = std::string_view(pool.find(s.str_id));
	fprintf(fd, "\"");
	for (auto c : str) {
		if (c == '\n')
			fprintf(fd, "\\n");
		else if (c == '\t')
			fprintf(fd, "\\t");
		else if (c == '\r')
			fprintf(fd, "\\r");
		else
			fprintf(fd, "%c", c);
	}
	fprintf(fd, "\"");
}

static void myprint(
	FILE* fd, const hir::Function& fn, const StringPool& pool, int spaces,
	HTContext& ctx
) {
	fprintf(fd, "function ");
	print_operand(fd, fn.region, pool, spaces, ctx);
}

static void myprint(
	FILE* fd, const hir::Nil&, const StringPool&, int, HTContext&
) {
	fprintf(fd, "nil");
}

static void myprint(
	FILE* fd, const hir::Constant& c, const StringPool& pool, int spaces,
	HTContext& ctx
) {
	std::visit([&](const auto& x) { myprint(fd, x, pool, spaces, ctx); }, c);
}

static void myprint(
	FILE* fd, const hir::StaticAllocation& c, const StringPool&, int, HTContext&
) {
	fprintf(fd, "static_alloc %zu", c.size);
}

static void myprint(
	FILE* fd, const hir::BuiltinLoad& c, const StringPool&, int, HTContext&
) {
	fprintf(fd, "builtin_load %s", c.name.c_str());
}

static void myprint(
	FILE* fd, const hir::ModuleImport& c, const StringPool&, int, HTContext&
) {
	fprintf(fd, "module_import \"%s\"", c.name.c_str());
}

static void myprint(
	FILE* fd, const hir::MemberImport& c, const StringPool&, int, HTContext&
) {
	fprintf(fd, "member_import @%s \"%s\"", c.label.name.c_str(), c.name.c_str());
}

void print_module(
	FILE* fd, const Module& mod, const StringPool& pool, int spaces,
	HTContext& ctx
) {
	fprintf(fd, "module {\n");
	spaces += 2;
	for (const auto& [symbol, v] : mod.static_symbols) {
		for (auto i = 0; i < spaces; i++) fprintf(fd, " ");
		const auto& vis = std::get<0>(v);
		if (vis == Visibility::EXPORTED) {
			fprintf(fd, "export ");
		}
		fprintf(fd, "@%s: ", symbol.c_str());
		const auto& init = std::get<1>(v);
		std::visit([&](const auto& x) { myprint(fd, x, pool, spaces, ctx); }, init);
		fprintf(fd, "\n");
	}
	fprintf(fd, "}\n");
}

hir::Label Module::register_static_initialization(
	hir::Label name, Visibility visibility,
	const StaticInitialization& static_init
) {
	// FIXME
	static_symbols[name.name] = std::make_pair(visibility, static_init);
	return name;
}

void Module::register_member_variable(hir::Operand) {
	// TODO
}

void Code::make_tuple(
	hir::Register result, std::vector<hir::Operand> elements
) {
	std::vector<hir::Operand> arguments {result};
	for (auto c : elements) {
		arguments.push_back(c);
	}
	instructions.push_back(
		Instruction {
			.opcode = Opcode::MAKE_TUPLE,
			.operands = arguments,
		}
	);
}
void Code::make_array(
	hir::Register result, std::vector<hir::Operand> elements
) {
	std::vector<hir::Operand> arguments {result};
	for (auto c : elements) {
		arguments.push_back(c);
	}
	instructions.push_back(
		Instruction {
			.opcode = Opcode::MAKE_ARRAY,
			.operands = arguments,
		}
	);
}

void Code::enclose(
	hir::Register result, hir::Label label, hir::Register closure
) {
	instructions.push_back(
		Instruction {
			.opcode = Opcode::ENCLOSE,
			.operands = {result, label, closure},
		}
	);
}

void Code::printf(hir::Operand opnd) {
	instructions.push_back(
		Instruction {
			.opcode = Opcode::PRINTF,
			.operands = {opnd},
		}
	);
}

void Code::printv(hir::Operand opnd) {
	instructions.push_back(
		Instruction {
			.opcode = Opcode::PRINTV,
			.operands = {opnd},
		}
	);
}

void Code::printc(hir::Operand opnd) {
	instructions.push_back(
		Instruction {
			.opcode = Opcode::PRINTC,
			.operands = {opnd},
		}
	);
}

void Code::catch_(
	hir::Register result, hir::Label tag, hir::Region handler,
	hir::Region expression
) {
	instructions.push_back(
		Instruction {
			.opcode = Opcode::CATCH,
			.operands = {result, tag, handler, expression},
		}
	);
}

void Code::alias(hir::Register result, hir::Register value) {
	instructions.push_back(
		Instruction {.opcode = Opcode::ALIAS, .operands = {result, value}}
	);
}

void Code::get_constructor(hir::Register result, hir::Operand value) {
	instructions.push_back(
		Instruction {.opcode = Opcode::GET_CONSTRUCTOR, .operands = {result, value}}
	);
}

void Code::get_variant(
	hir::Register result, hir::Operand value, hir::Operand tag
) {
	instructions.push_back(
		Instruction {
			.opcode = Opcode::GET_VARIANT, .operands = {result, value, tag}
		}
	);
}

void Code::panic() {
	instructions.push_back(Instruction {.opcode = Opcode::PANIC, .operands = {}});
}

void Code::ret(hir::Operand value) {
	instructions.push_back(
		Instruction {.opcode = Opcode::RET, .operands = {value}}
	);
}

void Code::binop(
	hir::Opcode operation, hir::Register result, hir::Operand left,
	hir::Operand right
) {
	// FIXME: check if operation is a binary operator
	instructions.push_back(
		Instruction {.opcode = operation, .operands = {result, left, right}}
	);
}

void Code::unop(
	hir::Opcode operation, hir::Register result, hir::Operand right
) {
	// FIXME: check if operation is a unary operator
	instructions.push_back(
		Instruction {.opcode = operation, .operands = {result, right}}
	);
}

void Code::repeat(
	hir::Register result, hir::Operand element, hir::Operand amount
) {
	instructions.push_back(
		Instruction {
			.opcode = Opcode::REPEAT, .operands = {result, element, amount}
		}
	);
}

void Code::make_variant(
	hir::Register result, hir::Operand tag, hir::Register tuple
) {
	instructions.push_back(
		Instruction {
			.opcode = Opcode::MAKE_VARIANT, .operands = {result, tag, tuple}
		}
	);
}

void Code::length(hir::Register result, hir::Operand array) {
	instructions.push_back(
		Instruction {
			.opcode = Opcode::LENGTH,
			.operands = {result, array},
		}
	);
}

} // namespace hir
