#ifndef HIR_TO_LIR_HPP
#define HIR_TO_LIR_HPP

#include <optional>
#include <string>

#include "counter.hpp"
#include "dst.hpp"
#include "hir/hir.hpp"
#include "hir/hir_type.hpp"
#include "lir/lir.hpp"
#include "str_pool.h"

namespace lower::hir_to_lir {

class Compiler {
 public:
	Compiler(const StringPool& pool, const hir::HTContext& hctx)
	: pool(pool), hctx(hctx) {}

 public:
	lir::Chunk compile(const hir::Module&);

 private:
	void compile_function(lir::Chunk&, lir::Label, const hir::Function&);
	void compile_instruction(
		lir::Chunk& chunk, const hir::Instruction& instruction,
		std::optional<lir::Register> yielding_to, const hir::Block&, std::size_t
	);
	void compile_region(
		lir::Chunk&, const hir::Region&, std::optional<lir::Register> yielding_to,
		lir::Label jump_to, lir::Label
	);
	void compile_operand();

	lir::Operand translate_operand(hir::Operand);
	lir::Register ensure_mutable(lir::Chunk&, hir::Operand);
	lir::Operand ensure_immutable(lir::Chunk&, hir::Operand);
	void append_init(lir::Chunk& chunk, lir::Operand main_closure);
	void reserve_registers(const hir::Function&);
	void reserve_registers(const hir::Region&);
	void reserve_registers(const hir::Instruction&);
	void reserve_register(const hir::Register&);

 private:
	const StringPool& pool;
	const hir::HTContext& hctx;

 private:
	Counter<lir::Register> m_registers {};
	Counter<lir::Label> m_labels {};
	dst::map<std::string, lir::Register> m_global_to_register {};
	dst::map<std::string, lir::Label> m_global_function_to_label {};
	dst::map<lir::Label, lir::Chunk> m_compile_functions {};
	dst::map<std::string, lir::Opcode> m_global_to_builtin {};
	dst::map<hir::Register, lir::Opcode> m_register_to_builtin {};
	dst::map<hir::Register, lir::Operand> m_value_to_value {};
	dst::map<hir::Label, lir::Label> m_block_mapping {};
	dst::map<hir::Register, lir::Register> m_register_mapping {};
	std::optional<lir::Label> m_innermost_break_point {};
	std::optional<lir::Label> m_innermost_continue_point {};
};

} // namespace lower::hir_to_lir

#endif
