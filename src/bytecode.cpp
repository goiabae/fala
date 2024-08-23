#include "bytecode.hpp"

#include <cassert>

namespace bytecode {

Chunk& Chunk::emit(Opcode opcode, Operand fst, Operand snd, Operand trd) {
	m_vec.push_back(Instruction {opcode, {fst, snd, trd}, ""});
	return *this;
}

Chunk& Chunk::with_comment(std::string comment) {
	m_vec.back().comment = comment;
	return *this;
}

size_t opcode_opnd_count(Opcode op) {
	switch (op) {
		case Opcode::PRINTF: return 1;
		case Opcode::PRINTV: return 1;
		case Opcode::READ: return 1;
		case Opcode::MOV: return 2;
		case Opcode::ADD: return 3;
		case Opcode::SUB: return 3;
		case Opcode::MUL: return 3;
		case Opcode::DIV: return 3;
		case Opcode::MOD: return 3;
		case Opcode::NOT: return 2;
		case Opcode::OR: return 3;
		case Opcode::AND: return 3;
		case Opcode::EQ: return 3;
		case Opcode::DIFF: return 3;
		case Opcode::LESS: return 3;
		case Opcode::LESS_EQ: return 3;
		case Opcode::GREATER: return 3;
		case Opcode::GREATER_EQ: return 3;
		case Opcode::LOAD: return 3;
		case Opcode::STORE: return 3;
		case Opcode::LABEL: return 1;
		case Opcode::JMP: return 1;
		case Opcode::JMP_FALSE: return 2;
		case Opcode::JMP_TRUE: return 2;
	}
	assert(false);
};

void print_chunk(FILE* fd, const Chunk& chunk) {
	int max = 0;
	int printed = 0;
	for (const auto& inst : chunk.m_vec) {
		print_inst(fd, inst);
		if (printed > max) max = printed;
		if (inst.comment != "") {
			for (int i = 0; i < (max - printed); i++) fputc(' ', fd);
			fprintf(fd, "  ; ");
			fprintf(fd, "%s", inst.comment.c_str());
		}
		fprintf(fd, "\n");
	}
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

int print_operand(FILE* fd, Operand opnd) {
	switch (opnd.type) {
		case Operand::Type::NIL: return fprintf(fd, "0");
		case Operand::Type::TMP: return fprintf(fd, "%%t%zu", opnd.reg.index);
		case Operand::Type::REG: return fprintf(fd, "%%r%zu", opnd.reg.index);
		case Operand::Type::LAB: return fprintf(fd, "L%03zu", opnd.lab.id);
		case Operand::Type::STR: return print_str(fd, opnd.str);
		case Operand::Type::NUM: return fprintf(fd, "%d", opnd.num);
		case Operand::Type::FUN: assert(false && "unreachable");
	}
	return 0;
}

// return textual representation of opcode
const char* opcode_repr(Opcode op) {
	switch (op) {
		case Opcode::PRINTF: return "printf";
		case Opcode::PRINTV: return "printv";
		case Opcode::READ: return "read";
		case Opcode::MOV: return "mov";
		case Opcode::ADD: return "add";
		case Opcode::SUB: return "sub";
		case Opcode::MUL: return "mult";
		case Opcode::DIV: return "div";
		case Opcode::MOD: return "mod";
		case Opcode::NOT: return "not";
		case Opcode::OR: return "or";
		case Opcode::AND: return "and";
		case Opcode::EQ: return "equal";
		case Opcode::DIFF: return "diff";
		case Opcode::LESS: return "less";
		case Opcode::LESS_EQ: return "lesseq";
		case Opcode::GREATER: return "greater";
		case Opcode::GREATER_EQ: return "greatereq";
		case Opcode::LOAD: return "load";
		case Opcode::STORE: return "store";
		case Opcode::LABEL: return "label";
		case Opcode::JMP: return "jump";
		case Opcode::JMP_FALSE: return "jf";
		case Opcode::JMP_TRUE: return "jt";
	}
	assert(false);
}

int print_operand_indirect(FILE* fd, Operand opnd) {
	if (opnd.type == Operand::Type::NUM) {
		return fprintf(fd, "%d", opnd.num);
	} else if (opnd.type == Operand::Type::REG) {
		if (opnd.reg.has_num())
			return fprintf(fd, "%zu", opnd.reg.index);
		else if (opnd.reg.has_addr())
			return fprintf(fd, "%%r%zu", opnd.reg.index);
	} else if (opnd.type == Operand::Type::TMP) {
		if (opnd.reg.has_num())
			return fprintf(fd, "%zu", opnd.reg.index);
		else if (opnd.reg.has_addr())
			return fprintf(fd, "%%t%zu", opnd.reg.index);
	} else
		assert(false && "unreachable");
	return 0;
}

// print an indirect memory access instruction (OP_STORE or OP_LOAD), where
// the base and offset operands are printed differently depending on their
// contents.
int print_inst_indirect(FILE* fd, const Instruction& inst) {
	int printed = 0;
	printed += fprintf(fd, "    ");
	printed += fprintf(fd, "%s", opcode_repr(inst.opcode));
	printed += fprintf(fd, " ");
	printed += print_operand(fd, inst.operands[0]);
	printed += fprintf(fd, ", ");
	printed += print_operand_indirect(fd, inst.operands[1]);
	printed += fprintf(fd, "(");
	printed += print_operand_indirect(fd, inst.operands[2]);
	printed += fprintf(fd, ")");
	return printed;
}

int print_inst(FILE* fd, const Instruction& inst) {
	if (inst.opcode == Opcode::LOAD || inst.opcode == Opcode::STORE) {
		return print_inst_indirect(fd, inst);
	} else {
		constexpr const char* separators[3] = {" ", ", ", ", "};
		int printed = 0;
		if (inst.opcode != Opcode::LABEL) printed += fprintf(fd, "    ");
		printed += fprintf(fd, "%s", opcode_repr(inst.opcode));
		for (size_t i = 0; i < bytecode::opcode_opnd_count(inst.opcode); i++) {
			printed += fprintf(fd, "%s", separators[i]);
			printed += print_operand(fd, inst.operands[i]);
		}
		return printed;
	}
}

} // namespace bytecode
