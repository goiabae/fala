#include "lir.hpp"

#include <cassert>

namespace lir {

Chunk& Chunk::emit(Opcode opcode, Operand fst, Operand snd, Operand trd) {
	m_vec.push_back(Instruction {opcode, {fst, snd, trd}, ""});
	return *this;
}

Chunk& Chunk::with_comment(std::string comment) {
	m_vec.back().comment = comment;
	return *this;
}

void Chunk::add_label(Operand label) {
	label_indexes[label.as_label().id] = m_vec.size();
}

size_t opcode_opnd_count(Opcode op) {
	switch (op) {
		case Opcode::PRINTF: return 1;
		case Opcode::PRINTV: return 1;
		case Opcode::PRINTC: return 1;
		case Opcode::READV: return 1;
		case Opcode::READC: return 1;
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
		case Opcode::JMP: return 1;
		case Opcode::JMP_FALSE: return 2;
		case Opcode::JMP_TRUE: return 2;
		case Opcode::PUSH: return 1;
		case Opcode::POP: return 1;
		case Opcode::CALL: return 1;
		case Opcode::RET: return 0;
		case Opcode::FUNC: return 0;
		case Opcode::ALLOCA: return 2;
		case Opcode::LOADA: return 3;
		case Opcode::STOREA: return 3;
		case Opcode::SHIFTA: return 3;
	}
	assert(false);
};

void print_chunk(FILE* fd, const Chunk& chunk) {
	int max = 0;
	int printed = 0;
	size_t i = 0;
	for (const auto& inst : chunk.m_vec) {
		for (auto l : chunk.label_indexes)
			if (l.second == i) fprintf(fd, "L%03zu:\n", l.first);
		print_inst(fd, inst);
		if (printed > max) max = printed;
		if (inst.comment != "") {
			for (int i = 0; i < (max - printed); i++) fputc(' ', fd);
			fprintf(fd, "  ; ");
			fprintf(fd, "%s", inst.comment.c_str());
		}
		fprintf(fd, "\n");
		i++;
	}
	for (auto l : chunk.label_indexes)
		if (l.second == i) fprintf(fd, "L%03zu:\n", l.first);
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

auto Operand::make_immediate_integer(int integer) -> Operand {
	Operand opnd;
	opnd.type = Operand::Type::IMMEDIATE;
	opnd.data.emplace<Immediate>(integer);
	return opnd;
}

int print_operand(FILE* fd, Operand opnd) {
	switch (opnd.type) {
		case Operand::Type::NOTHING: return fprintf(fd, "0");
		case Operand::Type::REGISTER:
			return fprintf(fd, "%%%zu", opnd.as_register().index);
		case Operand::Type::LABEL: return fprintf(fd, "L%03zu", opnd.as_label().id);
		case Operand::Type::IMMEDIATE:
			return fprintf(fd, "%d", opnd.as_immediate().number);
		case Operand::Type::FUN: assert(false && "unreachable");
	}
	return 0;
}

// return textual representation of opcode
const char* opcode_repr(Opcode op) {
	switch (op) {
		case Opcode::PRINTF: return "printf";
		case Opcode::PRINTV: return "printv";
		case Opcode::PRINTC: return "printc";
		case Opcode::READV: return "readv";
		case Opcode::READC: return "readc";
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
		case Opcode::JMP: return "jump";
		case Opcode::JMP_FALSE: return "jf";
		case Opcode::JMP_TRUE: return "jt";
		case Opcode::PUSH: return "push";
		case Opcode::POP: return "pop";
		case Opcode::CALL: return "call";
		case Opcode::RET: return "ret";
		case Opcode::FUNC: return "func";
		case Opcode::ALLOCA: return "alloca";
		case Opcode::LOADA: return "loada";
		case Opcode::STOREA: return "storea";
		case Opcode::SHIFTA: return "shifta";
	}
	assert(false);
}

Type Type::make_integer() {
	lir::Type t {lir::Integer {}};
	return t;
}

Type Type::make_integer_array() {
	lir::Type t {lir::Pointer {std::make_shared<lir::Type>(lir::Integer {}), true}
	};
	return t;
}

Type Type::make_integer_pointer() {
	return lir::Type {
		lir::Pointer {std::make_shared<lir::Type>(lir::Integer {}), false}
	};
}

int print_operand_indirect(FILE* fd, Operand opnd) {
	if (opnd.type == Operand::Type::IMMEDIATE) {
		return fprintf(fd, "%d", opnd.as_immediate().number);
	} else if (opnd.type == Operand::Type::REGISTER) {
		return fprintf(fd, "%%r%zu", opnd.as_register().index);
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
		fprintf(fd, "    ");
		printed += fprintf(fd, "%s", opcode_repr(inst.opcode));
		for (size_t i = 0; i < lir::opcode_opnd_count(inst.opcode); i++) {
			printed += fprintf(fd, "%s", separators[i]);
			printed += print_operand(fd, inst.operands[i]);
		}
		return printed;
	}
}

Chunk operator+(Chunk x, Chunk y) {
	Chunk res {};

	for (auto inst : x.m_vec) res.m_vec.push_back(inst);
	for (auto inst : y.m_vec) res.m_vec.push_back(inst);

	for (auto p : x.label_indexes) res.label_indexes[p.first] = p.second;

	for (auto p : y.label_indexes)
		res.label_indexes[p.first] = p.second + x.m_vec.size();

	res.result_opnd = y.result_opnd;

	return res;
}

const char* operand_type_repr(Operand::Type type) {
	switch (type) {
		case Operand::Type::NOTHING: return "Operand::Type::NIL";
		case Operand::Type::REGISTER: return "Operand::Type::REG";
		case Operand::Type::LABEL: return "Operand::Type::LAB";
		case Operand::Type::IMMEDIATE: return "Operand::Type::NUM";
		case Operand::Type::FUN: return "Operand::Type::FUN";
	}
	assert(false);
}

void Chunk::emit_store(Operand value, Operand offset, Operand base) {
	emit(Opcode::STORE, value, offset, base);
}

} // namespace lir
