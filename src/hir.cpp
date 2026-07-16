#include "hir.hpp"

#include <cassert>
#include <ranges>
#include <stdexcept>
#include <variant>

#include "str_pool.h"
#include "utils.hpp"

namespace hir {

static void print_operand(
	FILE* fd, Operand opnd, const StringPool& pool, int spaces
);
static void print_instruction(
	FILE* fd, const Instruction& inst, const StringPool& pool, int spaces
);

Code operator+(Code pre, Code post) {
	Code res {};

	for (auto inst : pre.instructions) res.instructions.push_back(inst);
	for (auto inst : post.instructions) res.instructions.push_back(inst);

	return res;
}

void Code::call(
	hir::Register result, hir::Register function,
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

void Code::if_false(hir::Register condition, hir::Block block) {
	instructions.push_back(
		Instruction {hir::Opcode::IF_FALSE, {condition, block}}
	);
}

void Code::if_true(hir::Register condition, hir::Block block) {
	instructions.push_back(
		Instruction {hir::Opcode::IF_TRUE, {condition, block}}
	);
}

void Code::loop(hir::Label next, hir::Label stop, hir::Block block) {
	instructions.push_back(Instruction {hir::Opcode::LOOP, {next, stop, block}});
}

void Code::brake(hir::Label where_to) {
	instructions.push_back(Instruction {hir::Opcode::BREAK, {where_to}});
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
void Code::get_element(
	hir::Register result, hir::Register aggregate,
	std::vector<hir::Operand> indexes
) {
	std::vector<hir::Operand> operands;
	operands.push_back(result);
	operands.push_back(aggregate);
	for (const auto& x : indexes) operands.push_back(x);
	instructions.push_back(Instruction {hir::Opcode::GET_ELEMENT, operands});
}

void Code::load(hir::Register r, hir::Register v) {
	instructions.push_back(Instruction {hir::Opcode::LOAD, {r, v}});
}

void Code::get_element(hir::Register a, hir::Register b, hir::Operand c) {
	instructions.push_back(Instruction {hir::Opcode::GET_ELEMENT, {a, b, c}});
}

void print_operand(FILE* fd, Operand opnd, const StringPool& pool, int spaces) {
	switch (opnd.kind) {
		case Operand::Kind::INVALID: assert(false);
		case Operand::Kind::REGISTER: {
			if (opnd.registuhr.is_mutable) {
				fprintf(fd, "$%zu", opnd.registuhr.id);
			} else {
				fprintf(fd, "%%%zu", opnd.registuhr.id);
			}
			return;
		}
		case Operand::Kind::BLOCK: {
			fprintf(fd, "{\n");
			const auto& code = *opnd.block.body_code;
			print_code(fd, code, pool, spaces + 2);
			for (int i = 0; i < spaces; i++) fprintf(fd, " ");
			fprintf(fd, "}");
			return;
		}
		case Operand::Kind::LABEL: {
			fprintf(fd, "@%s", opnd.label.name.c_str());
			return;
		}
	}
}

void print_instruction(
	FILE* fd, const Instruction& inst, const StringPool& pool, int spaces
) {
	for (int i = 0; i < spaces; i++) fprintf(fd, " ");
	switch (inst.opcode) {
		case Opcode::PRINT: assert(false);
		case Opcode::READ: assert(false);
		case Opcode::COPY: {
			assert(inst.operands.size() == 2);
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " = ");
			print_operand(fd, inst.operands[1], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
#define BINARY_INST(PRETTY)                            \
	{                                                    \
		assert(inst.operands.size() == 3);                 \
		print_operand(fd, inst.operands[0], pool, spaces); \
		fprintf(fd, " = ");                                \
		print_operand(fd, inst.operands[1], pool, spaces); \
		fprintf(fd, " " PRETTY " ");                       \
		print_operand(fd, inst.operands[2], pool, spaces); \
		fprintf(fd, "\n");                                 \
		return;                                            \
	}

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
		case Opcode::NOT: {
			assert(inst.operands.size() == 2);
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " = ");
			fprintf(fd, "not");
			for (size_t i = 1; i < inst.operands.size(); i++) {
				fprintf(fd, " ");
				print_operand(fd, inst.operands[i], pool, spaces);
			}
			fprintf(fd, "\n");
			return;
		}
		case Opcode::GET_ELEMENT: {
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " = ");
			fprintf(fd, "get_element");
			for (size_t i = 1; i < inst.operands.size(); i++) {
				fprintf(fd, " ");
				print_operand(fd, inst.operands[i], pool, spaces);
			}
			fprintf(fd, "\n");
			return;
		}
		case Opcode::SET_ELEMENT: {
			fprintf(fd, "set_element");
			for (size_t i = 0; i < inst.operands.size(); i++) {
				fprintf(fd, " ");
				print_operand(fd, inst.operands[i], pool, spaces);
			}
			fprintf(fd, "\n");
			return;
		}
		case Opcode::CALL: {
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " = ");
			fprintf(fd, "call");
			for (size_t i = 1; i < inst.operands.size(); i++) {
				fprintf(fd, " ");
				print_operand(fd, inst.operands[i], pool, spaces);
			}
			fprintf(fd, "\n");
			return;
		}
		case Opcode::RET: {
			fprintf(fd, "ret");
			for (size_t i = 0; i < inst.operands.size(); i++) {
				fprintf(fd, " ");
				print_operand(fd, inst.operands[i], pool, spaces);
			}
			fprintf(fd, "\n");
			return;
		}
		case Opcode::LOOP: {
			assert(inst.operands.size() == 3);
			fprintf(fd, "loop ");
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " ");
			print_operand(fd, inst.operands[1], pool, spaces);
			fprintf(fd, " ");
			print_operand(fd, inst.operands[2], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
		case Opcode::IF_TRUE: {
			assert(inst.operands.size() == 2);
			fprintf(fd, "if_true ");
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " ");
			print_operand(fd, inst.operands[1], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
		case Opcode::IF_FALSE: {
			assert(inst.operands.size() == 2);
			fprintf(fd, "if_false ");
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " ");
			print_operand(fd, inst.operands[1], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
		case Opcode::BREAK: {
			fprintf(fd, "break ");
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
		case Opcode::CONTINUE: {
			fprintf(fd, "continue\n");
			return;
		}
		case Opcode::LOAD: {
			assert(inst.operands.size() == 2);
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " = ");
			fprintf(fd, "load ");
			print_operand(fd, inst.operands[1], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
		case Opcode::STORE: {
			assert(inst.operands.size() == 2);
			fprintf(fd, "store ");
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " ");
			print_operand(fd, inst.operands[1], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
		case Opcode::ALLOC: {
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " = ");
			fprintf(fd, "alloc ");
			print_operand(fd, inst.operands[1], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
		case Opcode::CLONE: {
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " = ");
			fprintf(fd, "clone ");
			print_operand(fd, inst.operands[1], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
		case Opcode::ALIAS: {
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " = ");
			fprintf(fd, "alias ");
			print_operand(fd, inst.operands[1], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
		case Opcode::GET_GLOBAL: {
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " = ");
			fprintf(fd, "get_global ");
			print_operand(fd, inst.operands[1], pool, spaces);
			fprintf(fd, "\n");
			return;
		}
	}
}

void print_code(
	FILE* fd, const Code& code, const StringPool& pool, int spaces
) {
	size_t i = 0;
	for (i = 0; i < code.instructions.size(); i++) {
		print_instruction(fd, code.instructions[i], pool, spaces);
	}
}

void Module::load_builtin(hir::Label name) {
	if (not static_symbols.contains(name.name)) {
		static_symbols[name.name] = BuiltinLoad {name.name};
	}
}

hir::Label Module::register_constant(
	hir::Label name, const Constant& constant
) {
	for (const auto& [symbol, init] : static_symbols) {
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
	static_symbols[name.name] = constant;
	return name;
}

static void myprint(FILE* fd, const hir::Integer& i, const StringPool&, int) {
	fprintf(fd, "%d", i.integer);
}

static void myprint(FILE* fd, const hir::Character& c, const StringPool&, int) {
	fprintf(fd, "'%c'", c.character);
}

static void myprint(FILE* fd, const hir::Boolean& b, const StringPool&, int) {
	fprintf(fd, "%s", b.boolean ? "true" : "false");
}

static void myprint(
	FILE* fd, const hir::String& s, const StringPool& pool, int
) {
	fprintf(fd, "\"%s\"", pool.find(s.str_id));
}

static void myprint(
	FILE* fd, const hir::Function& fn, const StringPool& pool, int spaces
) {
	fprintf(fd, "function ");
	if (fn.is_builtin) {
		fprintf(fd, "\"%s\"", pool.find(fn.builtin_name));
	} else {
		fprintf(fd, "(");
		for (const auto& [i, reg] :
		     std::ranges::views::enumerate(fn.parameter_registers)) {
			print_operand(fd, reg, pool, spaces);
			if (i != (long)(fn.parameter_registers.size() - 1)) {
				fprintf(fd, ", ");
			}
		}
		fprintf(fd, ") ");
		fprintf(fd, "@%s ", fn.entry_block.c_str());
		for (const auto& [name, block] : fn.blocks) {
			fprintf(fd, "@%s ", name.c_str());
			print_operand(fd, block, pool, spaces);
		}
	}
}

static void myprint(FILE* fd, const hir::Nil&, const StringPool&, int) {
	fprintf(fd, "nil");
}

static void myprint(
	FILE* fd, const hir::Constant& c, const StringPool& pool, int spaces
) {
	std::visit([&](const auto& x) { myprint(fd, x, pool, spaces); }, c);
}

static void myprint(
	FILE* fd, const hir::StaticAllocation& c, const StringPool&, int
) {
	fprintf(fd, "static_alloc %zu", c.size);
}

static void myprint(
	FILE* fd, const hir::BuiltinLoad& c, const StringPool&, int
) {
	fprintf(fd, "builtin_load %s", c.name.c_str());
}

void print_module(
	FILE* fd, const Module& mod, const StringPool& pool, int spaces
) {
	for (const auto& [symbol, init] : mod.static_symbols) {
		fprintf(fd, "@%s: ", symbol.c_str());
		std::visit([&](const auto& x) { myprint(fd, x, pool, spaces); }, init);
		fprintf(fd, "\n");
	}
}

} // namespace hir
