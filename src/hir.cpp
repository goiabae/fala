#include "hir.hpp"

#include <cassert>

namespace hir {

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

void Code::copy(hir::Register destination, hir::Operand source) {
	instructions.push_back(Instruction {hir::Opcode::COPY, {destination, source}}
	);
}

void Code::builtin(hir::Register result, hir::String function_name) {
	// name may be a literal string or a register containing a string
	instructions.push_back(
		Instruction {hir::Opcode::BUILTIN, {result, function_name}}
	);
}

void Code::if_false(hir::Register condition, hir::Block block) {
	instructions.push_back(Instruction {hir::Opcode::IF_FALSE, {condition, block}}
	);
}

void Code::if_true(hir::Register condition, hir::Block block) {
	instructions.push_back(Instruction {hir::Opcode::IF_TRUE, {condition, block}}
	);
}

void Code::loop(hir::Block block) {
	instructions.push_back(Instruction {hir::Opcode::LOOP, {block}});
}

void Code::brake() {
	instructions.push_back(Instruction {hir::Opcode::BREAK, {}});
}

void Code::equals(hir::Register result, hir::Operand a, hir::Operand b) {
	instructions.push_back(Instruction {hir::Opcode::EQ, {result, a, b}});
}

// pseudo-instruction
void Code::inc(hir::Register registuhr) {
	instructions.push_back(
		Instruction {hir::Opcode::ADD, {registuhr, registuhr, hir::Integer(1)}}
	);
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

bool Register::contains_value() { return m_contains_value; }
bool Register::contains_pointer() { return not m_contains_value; }

void print_char(FILE* fd, char c) {
	if (c == '\n')
		fprintf(fd, "\\n");
	else
		fprintf(fd, "%c", c);
}

int print_str(FILE* fd, const char* str) {
	int printed = 2; // because open and closing double quotes
	fputc('"', fd);
	char c;
	while ((c = *(str++))) {
		if (c == '\n')
			printed += fprintf(fd, "\\n");
		else if (c == '\t')
			printed += fprintf(fd, "\\t");
		else {
			fputc(c, fd);
			printed++;
		}
	}
	fputc('"', fd);
	return printed;
}

void print_operand(FILE* fd, Operand opnd, const StringPool& pool, int spaces) {
	switch (opnd.kind) {
		case Operand::Kind::INVALID: assert(false);
		case Operand::Kind::INTEGER: {
			fprintf(fd, "%d", opnd.integer.integer);
			return;
		}
		case Operand::Kind::STRING: {
			print_str(fd, pool.find(opnd.string.str_id));
			return;
		}
		case Operand::Kind::REGISTER: {
			fprintf(fd, "%%%zu", opnd.registuhr.id);
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
		case Operand::Kind::BOOLEAN: {
			fprintf(fd, "%s", opnd.boolean.boolean ? "true" : "false");
			return;
		}
		case Operand::Kind::CHARACTER: {
			print_char(fd, opnd.character.character);
			return;
		}
		case Operand::Kind::NIL: {
			fprintf(fd, "nil");
			return;
		}
		case Operand::Kind::FUNCTION: {
			fprintf(fd, "function ");
			if (opnd.function.is_builtin) {
				fprintf(fd, "\"%s\"", pool.find(opnd.function.builtin_name));
			} else {
				fprintf(fd, "(");
				size_t parameter_count = opnd.function.parameter_registers.size();
				for (size_t i = 0; i < (parameter_count - 1); i++) {
					print_operand(fd, opnd.function.parameter_registers[i], pool, spaces);
					fprintf(fd, ", ");
				}
				print_operand(
					fd,
					opnd.function.parameter_registers[parameter_count - 1],
					pool,
					spaces
				);
				fprintf(fd, ") ");
				print_operand(fd, opnd.function.body_block, pool, spaces);
			}
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
		case Opcode::BUILTIN: {
			assert(inst.operands.size() == 2);
			print_operand(fd, inst.operands[0], pool, spaces);
			fprintf(fd, " = ");
			fprintf(fd, "builtin");
			fprintf(fd, " ");
			print_operand(fd, inst.operands[1], pool, spaces);
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
			assert(inst.operands.size() == 1);
			fprintf(fd, "loop ");
			print_operand(fd, inst.operands[0], pool, spaces);
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
			fprintf(fd, "break\n");
			return;
		}
		case Opcode::CONTINUE: {
			fprintf(fd, "continue\n");
			return;
		}
		case Opcode::REFTO: assert(false);
		case Opcode::GET_ELEMENT_PTR: assert(false);
		case Opcode::LOAD: assert(false);
		case Opcode::STORE: assert(false);
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

} // namespace hir
