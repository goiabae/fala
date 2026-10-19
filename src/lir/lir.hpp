#ifndef LIR_HPP
#define LIR_HPP

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "ast.hpp"

namespace lir {

enum class Opcode {
	PRINTF,
	PRINTV,
	PRINTC,
	READV,
	READC,
	MOV,
	ADD,
	SUB,
	MUL,
	DIV,
	MOD,
	NOT,
	OR,
	AND,
	EQ,
	DIFF,
	LESS,
	LESS_EQ,
	GREATER,
	GREATER_EQ,
	JMP,
	JMP_FALSE,
	JMP_TRUE,
	CALL,
	RET,
	FUNC,
	NOP,
	ENCLOSE,
	CATCH,
	RESUME,
	THROW,
	HALT,

	// stack operations
	BUMP,
	DROP,
	PUSH,
	POP,
	PICK,

	// pointer operations
	ALLOCA,
	LOADA,
	STOREA,
	SHIFTA,
	CLONEA,
	REPEAT,
	INITA,

	DBGSTK,
};

enum class Type {
	POINTER,
	INTEGER,
	CONTINUATION,
	CLOSURE,
	UNKNOWN,
};

struct Register {
	size_t index;
	Type type;
};

struct Label {
	size_t id;
};

struct Immediate {
	Immediate(int64_t number) : number {number} {}
	Immediate() = default;
	int64_t number;
};

struct ByteString {
	std::string bytes;
};

struct Operand {
	enum class Kind {
		NOTHING,   // no operand
		REGISTER,  // variables register
		LABEL,     // label
		IMMEDIATE, // immediate number
		BYTE_STRING,
	};

	Kind kind;
	std::variant<Register, Label, Immediate, ByteString> data;

	Operand() : kind {Kind::NOTHING}, data {Immediate {0}} {}
	Operand(Register reg) : kind(Kind::REGISTER), data {reg} {}
	Operand(Label lab) : kind(Kind::LABEL), data {lab} {}
	Operand(Immediate imm) : kind(Kind::IMMEDIATE), data(imm) {}
	Operand(ByteString bs) : kind(Kind::BYTE_STRING), data(bs) {}

	Label& as_label() { return std::get<Label>(data); }
	Register& as_register() { return std::get<Register>(data); }
	Immediate& as_immediate() { return std::get<Immediate>(data); }
	ByteString& as_byte_string() { return std::get<ByteString>(data); }

	const Label& as_label() const { return std::get<Label>(data); }
	const Register& as_register() const { return std::get<Register>(data); }
	const Immediate& as_immediate() const { return std::get<Immediate>(data); }
	const ByteString& as_byte_string() const {
		return std::get<ByteString>(data);
	}

	std::optional<lir::Type> type() const;
};

struct Instruction {
	static constexpr auto max_operands = 3;
	Opcode opcode;
	Operand operands[max_operands];
	std::string comment;
};

struct Chunk {
	Chunk& emit(Opcode, Operand fst = {}, Operand snd = {}, Operand trd = {});
	Chunk& add_label(Operand label);

	std::vector<Instruction> m_vec;
	std::map<size_t, size_t> label_indexes;

	std::optional<Operand> result_opnd;

	Chunk& emit_binop(
		BinaryOperator binop, Operand result, Operand left, Operand right
	);
	Chunk& emit_mov(Operand result, Operand value);
	Chunk& emit_jmp(Operand destination);
	Chunk& emit_alloca(Operand result, Operand size);
	Chunk& emit_storea(Operand value, Operand offset, Operand base);
	Chunk& emit_loada(Operand result, Operand offset, Operand base);
	Chunk& emit_shifta(Operand result, Operand offset, Operand base);
	Chunk& emit_clonea(Operand destination, Operand source);
	Chunk& emit_nop();
	Chunk& emit_enclose(Operand result, Operand label, Operand closure);
	Chunk& emit_catch(Operand tag, Operand outside_of);
	Chunk& emit_resume(Operand continuation);
};

struct ChunkRef {
	std::vector<lir::Chunk>& chunks;
	std::size_t index;
	lir::Label entry;

 public:
	ChunkRef emit(
		lir::Opcode opcode, lir::Operand fst = {}, lir::Operand snd = {},
		lir::Operand trd = {}
	);
	void finalize();
};

struct AppendFinalJump {
	std::size_t block_id;
};

using ChunkPatch = std::variant<AppendFinalJump>;

struct Block {
 public:
	lir::Label entry;
	std::vector<ChunkPatch> patches;

 public:
	void and_then(std::vector<lir::Chunk>&, lir::Operand);

 public:
	static lir::Block defer_last_jump(lir::Label entry, std::size_t to_patch);
};

// return amount of operands of each opcode
size_t opcode_opnd_count(Opcode op);
const char* opcode_repr(Opcode op);

void print_chunk(FILE*, const Chunk&);
int print_inst(FILE*, const Instruction& inst);

std::ostream& operator<<(std::ostream&, const Chunk&);
std::ostream& operator<<(std::ostream& st, const Instruction& inst);
std::ostream& operator<<(std::ostream& st, const Operand& opnd);
std::ostream& operator<<(std::ostream& st, const Type& typ);

Chunk operator+(Chunk x, Chunk y);
Chunk& operator+=(Chunk& x, Chunk y);

const char* operand_type_repr(Operand::Kind type);

} // namespace lir

#define DEFFMT(T)                                                 \
	template<>                                                      \
	struct std::formatter<T> : std::formatter<std::string> {        \
		auto format(const T& p, std::format_context& ctx) const {     \
			std::ostringstream oss {};                                  \
			oss << p;                                                   \
			return std::formatter<std::string>::format(oss.str(), ctx); \
		}                                                             \
	};

DEFFMT(lir::Operand);
DEFFMT(lir::Label);
DEFFMT(lir::Type);

#undef DEFFMT

#endif
