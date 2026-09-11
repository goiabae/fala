#include "lir.hpp"

#include <algorithm>
#include <cassert>
#include <optional>
#include <ostream>
#include <stdexcept>

#include "utils.hpp"

namespace lir {

static std::ostream& operator<<(std::ostream&, const ByteString&);
static std::ostream& print_operand_indirect(std::ostream& st, Operand opnd);
static int print_operand(FILE* fd, Operand opnd);
static int print_operand_indirect(FILE* fd, Operand opnd);
static int print_inst_indirect(FILE* fd, const Instruction& inst);
static std::ostream& print_inst_indirect(
	std::ostream& st, const Instruction& inst
);
static int print_type(FILE* fd, Type typ);

static int print_type(FILE* fd, Type typ) {
	switch (typ) {
		case Type::POINTER: return fprintf(fd, "ptr");
		case Type::INTEGER: return fprintf(fd, "int");
		case Type::CONTINUATION: return fprintf(fd, "cnt");
		case Type::CLOSURE: return fprintf(fd, "fun");
		case Type::UNKNOWN: return fprintf(fd, "???");
	}
	assert(false);
}

std::ostream& operator<<(std::ostream& st, const ByteString& bs) {
	st << '\"';
	for (auto c : bs.bytes) {
		if (c == '\n')
			st << "\\n";
		else if (c == '\t')
			st << "\\t";
		else if (c == '\r')
			st << "\\r";
		else
			st << c;
	}
	st << '\"';
	return st;
}

std::ostream& operator<<(std::ostream& st, const Operand& opnd) {
	switch (opnd.kind) {
		case Operand::Kind::NOTHING: return st << '0';
		case Operand::Kind::REGISTER:
			return st << std::format(
							 "%{}:{}", opnd.as_register().index, opnd.as_register().type
						 );
		case Operand::Kind::LABEL:
			return st << std::format("L{:03}", opnd.as_label().id);
		case Operand::Kind::IMMEDIATE:
			return st << std::format("{}", opnd.as_immediate().number);
		case Operand::Kind::BYTE_STRING: return st << opnd.as_byte_string();
	}
	return st;
}

Chunk& Chunk::emit(Opcode opcode, Operand fst, Operand snd, Operand trd) {
	m_vec.push_back(Instruction {opcode, {fst, snd, trd}, ""});
	return *this;
}

Chunk& Chunk::add_label(Operand label) {
	label_indexes[label.as_label().id] = m_vec.size();
	return *this;
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
		case Opcode::CLONEA: return 2;
		case Opcode::NOP: return 0;
		case Opcode::REPEAT: return 3;
		case Opcode::INITA: return 2;
		case Opcode::ENCLOSE: return 3;
		case Opcode::CATCH: return 3;
		case Opcode::RESUME: return 1;
		case Opcode::THROW: return 2;
		case Opcode::BUMP: return 1;
		case Opcode::DROP: return 1;
		case Opcode::PICK: return 2;
		case Opcode::HALT: return 0;
		case Opcode::DBGSTK: return 1;
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

std::ostream& operator<<(std::ostream& st, const Chunk& chunk) {
	int max = 0;
	int printed = 0;
	size_t i = 0;
	for (const auto& inst : chunk.m_vec) {
		for (auto l : chunk.label_indexes)
			if (l.second == i) st << std::format("L{:03}:\n", l.first);
		st << inst;
		if (printed > max) max = printed;
		if (inst.comment != "") {
			for (int i = 0; i < (max - printed); i++) st << ' ';
			st << "  ; ";
			st << inst.comment;
		}
		st << '\n';
		i++;
	}
	for (auto l : chunk.label_indexes)
		if (l.second == i) st << std::format("L{:03}:\n", l.first);
	return st;
}

int print_operand(FILE* fd, Operand opnd) {
	switch (opnd.kind) {
		case Operand::Kind::NOTHING: return fprintf(fd, "0");
		case Operand::Kind::REGISTER:
			return fprintf(fd, "%%%zu", opnd.as_register().index) + fprintf(fd, ":")
			     + print_type(fd, opnd.as_register().type);
		case Operand::Kind::LABEL: return fprintf(fd, "L%03zu", opnd.as_label().id);
		case Operand::Kind::IMMEDIATE:
			return fprintf(fd, "%d", opnd.as_immediate().number);
		case Operand::Kind::BYTE_STRING: {
			int printed = 0;
			printed += fprintf(fd, "\"");
			for (auto c : opnd.as_byte_string().bytes) {
				if (c == '\n')
					printed += fprintf(fd, "\\n");
				else if (c == '\t')
					printed += fprintf(fd, "\\t");
				else if (c == '\r')
					printed += fprintf(fd, "\\r");
				else
					printed += fprintf(fd, "%c", c);
			}
			printed += fprintf(fd, "\"");
			return printed;
		}
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
		case Opcode::CLONEA: return "clonea";
		case Opcode::NOP: return "nop";
		case Opcode::REPEAT: return "repeat";
		case Opcode::INITA: return "inita";
		case Opcode::ENCLOSE: return "enclose";
		case Opcode::CATCH: return "catch";
		case Opcode::RESUME: return "resume";
		case Opcode::THROW: return "throw";
		case Opcode::BUMP: return "bump";
		case Opcode::DROP: return "drop";
		case Opcode::PICK: return "pick";
		case Opcode::HALT: return "halt";
		case Opcode::DBGSTK: return "dbgstk";
	}
	assert(false);
}

int print_operand_indirect(FILE* fd, Operand opnd) {
	if (opnd.kind == Operand::Kind::IMMEDIATE) {
		return fprintf(fd, "%d", opnd.as_immediate().number);
	} else if (opnd.kind == Operand::Kind::REGISTER) {
		return fprintf(fd, "%%%zu", opnd.as_register().index) + fprintf(fd, ":")
		     + print_type(fd, opnd.as_register().type);
	} else
		assert(false && "unreachable");
	return 0;
}

static std::ostream& print_operand_indirect(std::ostream& st, Operand opnd) {
	if (opnd.kind == Operand::Kind::IMMEDIATE) {
		st << opnd.as_immediate().number;
	} else if (opnd.kind == Operand::Kind::REGISTER) {
		st << std::format("%{}", opnd.as_register().index);
	} else
		assert(false && "unreachable");
	return st;
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

std::ostream& print_inst_indirect(std::ostream& st, const Instruction& inst) {
	st << "    ";
	st << opcode_repr(inst.opcode);
	st << " ";
	st << inst.operands[0];
	st << ", ";
	print_operand_indirect(st, inst.operands[1]);
	st << "(";
	print_operand_indirect(st, inst.operands[2]);
	st << ")";
	return st;
}

int print_inst(FILE* fd, const Instruction& inst) {
	if (inst.opcode == Opcode::LOADA || inst.opcode == Opcode::STOREA
	    || inst.opcode == Opcode::SHIFTA) {
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

std::ostream& operator<<(std::ostream& st, const Instruction& inst) {
	if (inst.opcode == Opcode::LOADA || inst.opcode == Opcode::STOREA
	    || inst.opcode == Opcode::SHIFTA) {
		print_inst_indirect(st, inst);
	} else {
		constexpr const char* separators[3] = {" ", ", ", ", "};
		st << "    ";
		st << opcode_repr(inst.opcode);
		for (size_t i = 0; i < lir::opcode_opnd_count(inst.opcode); i++) {
			st << separators[i];
			st << inst.operands[i];
		}
	}
	return st;
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

Chunk& operator+=(Chunk& x, Chunk y) {
	x = x + y;
	return x;
}

const char* operand_type_repr(Operand::Kind type) {
	switch (type) {
		case Operand::Kind::NOTHING: return "Operand::Type::NIL";
		case Operand::Kind::REGISTER: return "Operand::Type::REG";
		case Operand::Kind::LABEL: return "Operand::Type::LAB";
		case Operand::Kind::IMMEDIATE: return "Operand::Type::NUM";
		case Operand::Kind::BYTE_STRING: return "Operand::Type::BYTE_STRING";
	}
	assert(false);
}

static bool is_value_operand(const Operand& opnd) {
	return opnd.kind == Operand::Kind::REGISTER
	    or opnd.kind == Operand::Kind::IMMEDIATE;
}

Chunk& Chunk::emit_binop(
	BinaryOperator binop, Operand result, Operand left, Operand right
) {
	// FIXME: add the rest of the operators
	switch (binop) {
		case BinaryOperator::PLUS: return emit(Opcode::ADD, result, left, right);
		case BinaryOperator::MINUS:
		case BinaryOperator::MULTIPLY:
		case BinaryOperator::DIVIDE:
		case BinaryOperator::MODULO: break;
	}
	throw std::runtime_error("unhandled binary operator");
}

Chunk& Chunk::emit_mov(Operand result, Operand value) {
	return emit(Opcode::MOV, result, value);
}

Chunk& Chunk::emit_jmp(Operand destination) {
	return emit(Opcode::JMP, destination);
}

Chunk& Chunk::emit_alloca(Operand result, Operand size) {
	if (not is_value_operand(size))
		throw std::runtime_error("size must be a value operand");
	return emit(Opcode::ALLOCA, result, size);
}

Chunk& Chunk::emit_storea(Operand value, Operand offset, Operand base) {
	if (not is_value_operand(offset))
		throw std::runtime_error("offset must be a value operand");
	return emit(Opcode::STOREA, value, offset, base);
}

Chunk& Chunk::emit_loada(Operand result, Operand offset, Operand base) {
	if (not is_value_operand(offset))
		throw std::runtime_error("offset must be a value operand");
	return emit(Opcode::LOADA, result, offset, base);
}

Chunk& Chunk::emit_shifta(Operand result, Operand offset, Operand base) {
	if (not is_value_operand(offset))
		throw std::runtime_error("offset must be a value operand");
	return emit(Opcode::SHIFTA, result, offset, base);
}

Chunk& Chunk::emit_clonea(Operand destination, Operand source) {
	return emit(Opcode::CLONEA, destination, source);
}

Chunk& Chunk::emit_nop() { return emit(Opcode::NOP); }

Chunk& Chunk::emit_enclose(Operand result, Operand label, Operand closure) {
	return emit(Opcode::ENCLOSE, result, label, closure);
}

static void apply_patch(
	const ChunkPatch& patch, std::vector<lir::Chunk>& chunks, lir::Operand opnd
) {
	std::visit(
		overloaded {[&](const AppendFinalJump& patch) {
			chunks.at(patch.block_id).emit_jmp(opnd);
		}},
		patch
	);
}

void Block::and_then(std::vector<lir::Chunk>& chunks, lir::Operand opnd) {
	for (const auto& patch : patches) {
		apply_patch(patch, chunks, opnd);
	}
}

lir::Block Block::defer_last_jump(lir::Label entry, std::size_t to_patch) {
	Block blk {
		.entry = entry, .patches = {AppendFinalJump {.block_id = to_patch}}
	};
	return blk;
}

ChunkRef ChunkRef::emit(
	lir::Opcode opcode, lir::Operand fst, lir::Operand snd, lir::Operand trd
) {
	chunks.at(index).emit(opcode, fst, snd, trd);
	return *this;
}

void ChunkRef::finalize() {
	auto& chunk = chunks.at(index);
	assert(chunk.m_vec.size() >= 1);
	auto opcode = chunk.m_vec.back().opcode;
	assert(
		opcode == Opcode::JMP or opcode == Opcode::JMP_TRUE
		or opcode == Opcode::JMP_FALSE or opcode == Opcode::RET
		or opcode == Opcode::HALT
	);
}

std::optional<lir::Type> Operand::type() const {
	if (kind == Kind::IMMEDIATE) {
		return Type::INTEGER;
	} else if (kind == Kind::REGISTER) {
		return as_register().type;
	} else {
		return std::nullopt;
	}
}

std::ostream& operator<<(std::ostream& st, const Type& typ) {
	switch (typ) {
		case Type::POINTER: return st << "ptr";
		case Type::INTEGER: return st << "int";
		case Type::CONTINUATION: return st << "cnt";
		case Type::CLOSURE: return st << "fun";
		case Type::UNKNOWN: return st << "???";
	}
	assert(false);
}

} // namespace lir
