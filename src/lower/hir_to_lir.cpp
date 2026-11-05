#include "hir_to_lir.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <variant>

#include "counter.hpp"
#include "hir/hir.hpp"
#include "hir/hir_type.hpp"
#include "lir/lir.hpp"
#include "utils.hpp"

namespace lower::hir_to_lir {

using Set = std::unordered_set<hir::Register>;

static Set live_in(const hir::Block&, std::size_t);
static Set live_out(const hir::Block&, std::size_t);
static Set kill(const hir::Block&, std::size_t);
static Set gen(const hir::Block&, std::size_t);
static Set live_across(const hir::Block&, std::size_t);

Set live_across(const hir::Block& blk, std::size_t index) {
	auto li = live_in(blk, index);
	auto lo = live_out(blk, index);
	auto inter = Set {};
	for (const auto& x : li) {
		if (lo.contains(x)) inter.insert(x);
	}
	return inter;
}

std::unordered_set<hir::Register> live_in(
	const hir::Block& blk, std::size_t index
) {
	auto lo = live_out(blk, index);
	auto ki = kill(blk, index);
	for (const auto& killed : ki) lo.erase(killed);
	auto gn = gen(blk, index);
	gn.merge(lo);
	return gn;
}

Set live_out(const hir::Block& blk, std::size_t index) {
	if (index == (blk.body_code->size() - 1)) return {};
	// FIXME: assumes instructions only have one successor. might not be the case,
	// for example with "if", that has successors in true and false branches.
	return live_in(blk, index + 1);
}

Set kill(const hir::Block& blk, std::size_t index) {
	const auto& instruction = blk.body_code->at(index);
	switch (instruction.opcode) {
#define A                                         \
	{                                               \
		const auto& x = instruction.operands.at(0);   \
		if (x.kind == hir::Operand::Kind::REGISTER) { \
			return Set {x.registuhr};                   \
		} else {                                      \
			return Set {};                              \
		}                                             \
	}
#define E \
	return Set {}
		case hir::Opcode::COPY: assert(false);
		case hir::Opcode::PRINTV: E;
		case hir::Opcode::PRINTC: E;
		case hir::Opcode::PRINTF: E;
		case hir::Opcode::ADD: A;
		case hir::Opcode::SUB: A;
		case hir::Opcode::MUL: A;
		case hir::Opcode::DIV: A;
		case hir::Opcode::MOD: A;
		case hir::Opcode::NOT: A;
		case hir::Opcode::OR: A;
		case hir::Opcode::AND: A;
		case hir::Opcode::EQ: A;
		case hir::Opcode::NEQ: A;
		case hir::Opcode::LESS: A;
		case hir::Opcode::LESS_EQ: A;
		case hir::Opcode::GREATER: A;
		case hir::Opcode::GREATER_EQ: A;
		case hir::Opcode::GET_GLOBAL: A;
		case hir::Opcode::MAKE_TUPLE: A;
		case hir::Opcode::MAKE_ARRAY: A;
		case hir::Opcode::ENCLOSE: A;
		case hir::Opcode::PANIC: E;
		case hir::Opcode::LOOP: E;
		case hir::Opcode::IF: A;
		case hir::Opcode::IF_TRUE: E;
		case hir::Opcode::IF_FALSE: E;
		case hir::Opcode::BREAK: E;
		case hir::Opcode::CONTINUE: E;
		case hir::Opcode::YIELD: E;
		case hir::Opcode::CALL: A;
		case hir::Opcode::RET: E;
		case hir::Opcode::CATCH: A;
		case hir::Opcode::PERFORM: A;
		case hir::Opcode::RESUME: A;
		case hir::Opcode::SET_ELEMENT: E;
		case hir::Opcode::GET_ELEMENT: A;
		case hir::Opcode::GET_CONSTRUCTOR: A;
		case hir::Opcode::GET_VARIANT: A;
		case hir::Opcode::MAKE_VARIANT: A;
		case hir::Opcode::LOAD: A;
		case hir::Opcode::STORE: A;
		case hir::Opcode::ALLOC: A;
		case hir::Opcode::CLONE: A;
		case hir::Opcode::ALIAS: A;
		case hir::Opcode::REPEAT: A;
		case hir::Opcode::LENGTH: A;
#undef A
#undef E
	}
	assert(false);
}

Set gen(const hir::Block& blk, std::size_t index) {
	const auto& instruction = blk.body_code->at(index);
	switch (instruction.opcode) {
#define E \
	return {}
#define S(N)                                      \
	{                                               \
		const auto& x = instruction.operands.at(N);   \
		if (x.kind == hir::Operand::Kind::REGISTER) { \
			return Set {x.registuhr};                   \
		} else {                                      \
			return Set {};                              \
		}                                             \
	}
#define O S(0)
#define O1 S(1)
#define A(N)                                                            \
	{                                                                     \
		Set xs {};                                                          \
		for (const auto& opnd : std::span(instruction.operands).subspan(N)) \
			if (opnd.kind == hir::Operand::Kind::REGISTER)                    \
				xs.insert(opnd.registuhr);                                      \
		return xs;                                                          \
	}
#define T A(1)
		case hir::Opcode::COPY: assert(false);
		case hir::Opcode::PRINTV: O;
		case hir::Opcode::PRINTC: O;
		case hir::Opcode::PRINTF: O;
		case hir::Opcode::ADD: T;
		case hir::Opcode::SUB: T;
		case hir::Opcode::MUL: T;
		case hir::Opcode::DIV: T;
		case hir::Opcode::MOD: T;
		case hir::Opcode::NOT: O1;
		case hir::Opcode::OR: T;
		case hir::Opcode::AND: T;
		case hir::Opcode::EQ: T;
		case hir::Opcode::NEQ: T;
		case hir::Opcode::LESS: T;
		case hir::Opcode::LESS_EQ: T;
		case hir::Opcode::GREATER: T;
		case hir::Opcode::GREATER_EQ: T;
		case hir::Opcode::GET_GLOBAL: E;
		case hir::Opcode::MAKE_TUPLE: A(1);
		case hir::Opcode::MAKE_ARRAY: A(1);
		case hir::Opcode::ENCLOSE:
			return Set {instruction.operands.at(2).registuhr};
		case hir::Opcode::PANIC: E;
		case hir::Opcode::LOOP: E;
		case hir::Opcode::IF: S(1);
		case hir::Opcode::IF_TRUE: S(0);
		case hir::Opcode::IF_FALSE: S(0);
		case hir::Opcode::BREAK: E;
		case hir::Opcode::CONTINUE: E;
		case hir::Opcode::YIELD: S(0);
		case hir::Opcode::CALL: A(1);
		case hir::Opcode::RET: S(0);
		case hir::Opcode::CATCH: E;
		case hir::Opcode::PERFORM: A(2);
		case hir::Opcode::RESUME: A(1);
		case hir::Opcode::SET_ELEMENT: assert(false);
		case hir::Opcode::GET_ELEMENT: A(1);
		case hir::Opcode::GET_CONSTRUCTOR: S(1);
		case hir::Opcode::GET_VARIANT: A(1);
		case hir::Opcode::MAKE_VARIANT: A(1);
		case hir::Opcode::LOAD: S(1);
		case hir::Opcode::STORE: S(1);
		case hir::Opcode::ALLOC: S(1);
		case hir::Opcode::CLONE: S(1);
		case hir::Opcode::ALIAS: S(1);
		case hir::Opcode::REPEAT: A(1);
		case hir::Opcode::LENGTH: S(1);
#undef O
#undef A
	}
	assert(false);
}

lir::Chunk Compiler::compile(const hir::Module& mod) {
	lir::Chunk chunk {};
	std::vector<std::pair<hir::Function const*, std::string>>
		defered_functions {};
	lir::Operand main_closure {};
	for (const auto& symbol : mod.static_symbols) {
		const auto& [name, value] = symbol;
		const auto& [visibility, initialization] = value;
		std::visit(
			overloaded {
				[&](const hir::Constant& c) {
					std::visit(
						overloaded {
							[&](const hir::Integer& i) {
								auto v = m_registers.next(lir::Type::UNKNOWN);
								chunk.emit_mov(v, lir::Immediate(i.integer));
								m_global_to_register[name] = v;
							},
							[&](const hir::String& s) {
								lir::ByteString bs {.bytes = pool.find(s.str_id)};
								auto length = (int64_t)bs.bytes.size();
								auto v = m_registers.next(lir::Type::UNKNOWN);
								auto r1 = m_registers.next(lir::Type::UNKNOWN);
								chunk.emit_alloca(v, lir::Immediate(2));
								chunk.emit_alloca(r1, lir::Immediate(length));
								chunk.emit(lir::Opcode::INITA, r1, bs);
								chunk.emit_storea(r1, lir::Immediate(0), v);
								chunk.emit_storea(lir::Immediate(length), lir::Immediate(1), v);
								m_global_to_register[name] = v;
							},
							[&](const hir::Character& c) {
								auto v = m_registers.next(lir::Type::UNKNOWN);
								chunk.emit_mov(v, lir::Immediate(c.character));
								m_global_to_register[name] = v;
							},
							[&](const hir::Nil&) {
								auto v = m_registers.next(lir::Type::UNKNOWN);
								chunk.emit_mov(v, lir::Immediate(0));
								m_global_to_register[name] = v;
							},
							[&](const hir::Boolean& b) {
								auto v = m_registers.next(lir::Type::UNKNOWN);
								chunk.emit_mov(v, lir::Immediate(b.boolean ? 1 : 0));
								m_global_to_register[name] = v;
							},
							[&](const hir::Function& f) {
								defered_functions.push_back(std::make_pair(&f, name));
								auto l = m_labels.next();
								m_global_function_to_label[name] = l;
								reserve_registers(f);
								if (name == "main") {
									main_closure = m_registers.next(lir::Type::UNKNOWN);
									auto r0 = m_registers.next(lir::Type::UNKNOWN);
									auto r1 = m_registers.next(lir::Type::UNKNOWN);
									chunk.emit_alloca(r0, lir::Immediate(0));
									chunk.emit_enclose(r1, l, r0);
									chunk.emit_alloca(main_closure, lir::Immediate(1));
									chunk.emit_storea(r1, lir::Immediate(0), main_closure);
								}
							}
						},
						c
					);
				},
				[&](const hir::StaticAllocation& sa) {
					auto v = m_registers.next(lir::Type::UNKNOWN);
					chunk.emit_alloca(v, lir::Immediate((std::int64_t)sa.size));
					m_global_to_register[name] = v;
				},
				[&](const hir::BuiltinLoad& bl) {
					if (bl.name == "read_int") {
						m_global_to_builtin[name] = lir::Opcode::READV;
					} else if (bl.name == "read_char") {
						m_global_to_builtin[name] = lir::Opcode::READC;
					} else {
						throw std::runtime_error(
							std::format("unknown builtin name: {}", bl.name)
						);
					}
				},
				[&](const hir::ModuleImport&) {
					assert(false && "not yet implemented");
				},
				[&](const hir::MemberImport&) {
					assert(false && "not yet implemented");
				},
			},
			initialization
		);
	}
	append_init(chunk, main_closure);
	for (const auto& [func, name] : defered_functions) {
		compile_function(chunk, m_global_function_to_label.at(name), *func);
	}
	return chunk;
}

void Compiler::append_init(lir::Chunk& chunk, lir::Operand main_closure) {
	const auto r1 = m_registers.next(lir::Type::INTEGER);
	const auto r2 = m_registers.next(lir::Type::CLOSURE);
	const auto r3 = m_registers.next(lir::Type::INTEGER);
	const auto k0 = m_registers.next(lir::Type::CONTINUATION);
	const auto r8 = m_registers.next(lir::Type::INTEGER);
	const auto r9 = m_registers.next(lir::Type::INTEGER);
	const auto r10 = m_registers.next(lir::Type::UNKNOWN);
	const auto r12 = m_registers.next(lir::Type::UNKNOWN);
	const auto r13 = m_registers.next(lir::Type::UNKNOWN);
	const auto k1 = m_registers.next(lir::Type::CONTINUATION);
	const auto k2 = m_registers.next(lir::Type::CONTINUATION);

	const auto write_char_catch_entry = m_labels.next();
	const auto write_char_handler_entry = m_labels.next();
	const auto write_str_catch_entry = m_labels.next();
	const auto write_str_handler_entry = m_labels.next();
	const auto write_int_catch_entry = m_labels.next();
	const auto write_int_handler_entry = m_labels.next();
	const auto write_int_body_entry = m_labels.next();

	lir::Immediate write_str_tag(0);
	lir::Immediate write_char_tag(2);
	lir::Immediate write_int_tag(1);

	chunk.add_label(write_char_catch_entry);
	chunk.emit(
		lir::Opcode::CATCH,
		write_char_tag,
		write_char_handler_entry,
		write_str_catch_entry
	);
	chunk.emit(lir::Opcode::POP, r10);
	chunk.emit(lir::Opcode::HALT);

	chunk.add_label(write_char_handler_entry);
	chunk.emit(lir::Opcode::POP, k0);
	chunk.emit(lir::Opcode::POP, r8);
	chunk.emit(lir::Opcode::LOADA, r9, lir::Immediate(0), r8);
	chunk.emit(lir::Opcode::PRINTC, r9);
	chunk.emit(lir::Opcode::PUSH, lir::Immediate(0));
	chunk.emit(lir::Opcode::RESUME, k0);
	chunk.emit(lir::Opcode::RET);

	chunk.add_label(write_str_catch_entry);
	chunk.emit(
		lir::Opcode::CATCH,
		write_str_tag,
		write_str_handler_entry,
		write_int_catch_entry
	);
	chunk.emit(lir::Opcode::POP, r12);
	chunk.emit(lir::Opcode::PUSH, r12);
	chunk.emit(lir::Opcode::RET);

	chunk.add_label(write_str_handler_entry);
	chunk.emit(lir::Opcode::POP, k1);
	chunk.emit(lir::Opcode::POP, r8);
	chunk.emit(lir::Opcode::LOADA, r9, lir::Immediate(0), r8);
	chunk.emit(lir::Opcode::PRINTF, r8);
	chunk.emit(lir::Opcode::PUSH, lir::Immediate(0));
	chunk.emit(lir::Opcode::RESUME, k1);
	chunk.emit(lir::Opcode::RET);

	chunk.add_label(write_int_catch_entry);
	chunk.emit(
		lir::Opcode::CATCH,
		write_int_tag,
		write_int_handler_entry,
		write_int_body_entry
	);
	chunk.emit(lir::Opcode::POP, r13);
	chunk.emit(lir::Opcode::PUSH, r13);
	chunk.emit(lir::Opcode::RET);

	chunk.add_label(write_int_handler_entry);
	chunk.emit(lir::Opcode::POP, k2);
	chunk.emit(lir::Opcode::POP, r8);
	chunk.emit(lir::Opcode::LOADA, r9, lir::Immediate(0), r8);
	chunk.emit(lir::Opcode::PRINTV, r9);
	chunk.emit(lir::Opcode::PUSH, lir::Immediate(0));
	chunk.emit(lir::Opcode::RESUME, k2);
	chunk.emit(lir::Opcode::RET);

	chunk.add_label(write_int_body_entry);
	chunk.emit(lir::Opcode::LOADA, r2, lir::Immediate(0), main_closure);
	chunk.emit(lir::Opcode::ALLOCA, r3, lir::Immediate(1));
	chunk.emit(lir::Opcode::STOREA, lir::Immediate(0), lir::Immediate(0), r3);
	chunk.emit(lir::Opcode::PUSH, r3);
	chunk.emit(lir::Opcode::CALL, r2);
	chunk.emit(lir::Opcode::POP, r1);
	chunk.emit(lir::Opcode::PUSH, r1);
	chunk.emit(lir::Opcode::RET);
}

void Compiler::compile_function(
	lir::Chunk& chunk, lir::Label l, const hir::Function& f
) {
	chunk.add_label(l);
	for (const auto& param : f.region.parameter_registers) {
		auto r = m_registers.next(lir::Type::UNKNOWN);
		m_register_mapping[param] = r;
		chunk.emit(lir::Opcode::POP, r);
	}
	std::vector<std::pair<std::string, hir::Block>> blocks {};
	for (const auto& [name, blk] : f.region.blocks) {
		blocks.push_back(std::make_pair(name, blk));
	}
	auto entry = f.region.entry_block;
	auto found =
		std::ranges::find(blocks, entry, [](const auto& x) { return x.first; });
	if (found == blocks.end()) throw std::runtime_error("entry block not found");
	if (found != blocks.begin()) {
		std::iter_swap(found, blocks.begin());
	}
	for (const auto& [name, blk] : blocks) {
		auto l1 = m_labels.next();
		m_block_mapping[hir::Label {name}] = l1;
		chunk.add_label(l1);
		for (const auto& [index, inst] :
		     *blk.body_code | std::ranges::views::enumerate) {
			compile_instruction(chunk, inst, std::nullopt, blk, (std::size_t)index);
		}
	}
}

void Compiler::compile_instruction(
	lir::Chunk& chunk, const hir::Instruction& instruction,
	std::optional<lir::Register> yielding_to, const hir::Block& parent_block,
	std::size_t index
) {
	switch (instruction.opcode) {
		case hir::Opcode::COPY:
		case hir::Opcode::PRINTV:
		case hir::Opcode::PRINTC:
		case hir::Opcode::PRINTF:
		case hir::Opcode::SET_ELEMENT: {
			throw std::runtime_error(
				std::format(
					"unhandled opcode {}", hir::opcode_to_string(instruction.opcode)
				)
			);
		}
		case hir::Opcode::GET_VARIANT: {
			auto result = translate_operand(instruction.operands.at(0));
			auto union_ = translate_operand(instruction.operands.at(2));
			chunk.emit_loada(result, lir::Immediate(1), union_);
			return;
		}
		case hir::Opcode::GET_CONSTRUCTOR: {
			auto result = translate_operand(instruction.operands.at(0));
			auto union_ = translate_operand(instruction.operands.at(1));
			chunk.emit_loada(result, lir::Immediate(0), union_);
			return;
		}
		case hir::Opcode::MAKE_ARRAY: {
			auto result = translate_operand(instruction.operands.at(0));
			auto size = (int64_t)instruction.operands.size() - 1;
			auto r0 = m_registers.next(lir::Type::UNKNOWN);
			chunk.emit_alloca(result, lir::Immediate(2));
			chunk.emit_alloca(r0, lir::Immediate(size));
			for (auto i = 0l; i < size; i++) {
				auto arg =
					translate_operand(instruction.operands.at((std::size_t)i + 1));
				chunk.emit_storea(arg, lir::Immediate(i), r0);
			}
			chunk.emit_storea(r0, lir::Immediate(0), result);
			chunk.emit_storea(lir::Immediate(size), lir::Immediate(1), result);
			return;
		}
		case hir::Opcode::MAKE_VARIANT: {
			auto result = translate_operand(instruction.operands.at(0));
			auto tag = translate_operand(instruction.operands.at(1));
			auto args = translate_operand(instruction.operands.at(2));
			chunk.emit_alloca(result, lir::Immediate(2));
			chunk.emit_storea(tag, lir::Immediate(0), result);
			chunk.emit_storea(args, lir::Immediate(1), result);
			return;
		}
		case hir::Opcode::PANIC: {
			chunk.emit(lir::Opcode::HALT);
			return;
		}
		case hir::Opcode::BREAK: {
			auto label = m_block_mapping.at(instruction.operands.at(0).label);
			chunk.emit_jmp(label);
			return;
		}
		case hir::Opcode::CONTINUE: {
			auto label = m_block_mapping.at(instruction.operands.at(0).label);
			chunk.emit_jmp(label);
			return;
		}
		case hir::Opcode::IF_TRUE: {
			auto condition = translate_operand(instruction.operands.at(0));
			auto branch = instruction.operands.at(1).region;
			auto l0 = m_labels.next();
			auto l1 = m_labels.next();
			chunk.emit(lir::Opcode::JMP_FALSE, condition, l0);
			compile_region(chunk, branch, std::nullopt, l0, l1);
			chunk.add_label(l0);
			return;
		}
		case hir::Opcode::IF_FALSE: {
			auto condition = translate_operand(instruction.operands.at(0));
			auto branch = instruction.operands.at(1).region;
			auto l0 = m_labels.next();
			auto l1 = m_labels.next();
			chunk.emit(lir::Opcode::JMP_TRUE, condition, l0);
			compile_region(chunk, branch, std::nullopt, l0, l1);
			chunk.add_label(l0);
			return;
		}
		case hir::Opcode::GET_ELEMENT: {
			auto hresult = instruction.operands.at(0);
			auto hbase = instruction.operands.at(1);
			auto result = translate_operand(instruction.operands.at(0));
			auto base = translate_operand(hbase);
			auto index = translate_operand(instruction.operands.at(2));
			assert(hresult.kind == hir::Operand::Kind::REGISTER);
			if (hbase.kind == hir::Operand::Kind::REGISTER) {
				if (hbase.registuhr.is_mutable) {
					assert(hresult.registuhr.is_mutable);
					auto t = hctx.at(hbase.registuhr.htype);
					std::visit(
						overloaded {
							[&](const hir::HArray&) {
								auto r1 = m_registers.next(lir::Type::UNKNOWN);
								auto r2 = m_registers.next(lir::Type::UNKNOWN);
								chunk.emit_loada(r1, lir::Immediate(0), base);
								chunk.emit_loada(r2, lir::Immediate(0), r1);
								chunk.emit(lir::Opcode::SHIFTA, result, index, r2);
								return;
								// auto r1 = m_registers.next(lir::Type::UNKNOWN);
						    // chunk.emit_loada(r1, lir::Immediate(0), base);
						    // chunk.emit(lir::Opcode::SHIFTA, result, index, r1);
						    // return;
							},
							[&](const hir::HTuple&) {
								auto r1 = m_registers.next(lir::Type::UNKNOWN);
								chunk.emit_loada(r1, lir::Immediate(0), base);
								chunk.emit(lir::Opcode::SHIFTA, result, index, r1);
								return;
							},
							[&](const auto&) {
								throw std::runtime_error(
									"can't index argument of non-tuple/array type"
								);
							}
						},
						t.data
					);
					return;
				} else {
					assert(not hresult.registuhr.is_mutable);
					auto t = hctx.at(hbase.registuhr.htype);
					std::visit(
						overloaded {
							[&](const hir::HArray&) {
								auto r1 = m_registers.next(lir::Type::UNKNOWN);
								chunk.emit_loada(r1, lir::Immediate(0), base);
								chunk.emit(lir::Opcode::SHIFTA, result, index, r1);
								return;
								// chunk.emit_loada(result, index, base);
						    // return;
							},
							[&](const hir::HTuple&) {
								chunk.emit_loada(result, index, base);
								return;
							},
							[&](const hir::HAny&) {
								// FIXME: assuming to be tuple type
						    // This is case when for the return value of get_variant, which
						    // can't be statically known
								chunk.emit(lir::Opcode::SHIFTA, result, index, base);
								return;
							},
							[&](const auto&) {
								throw std::runtime_error(
									"can't index argument of non-tuple/array type"
								);
							}
						},
						t.data
					);
					return;
				}
			} else {
				throw std::runtime_error("get_element only works with registers");
			}
			return;
		}
		case hir::Opcode::LOOP: {
			auto l0 = m_labels.next();
			auto l1 = m_labels.next();
			auto continue_ = instruction.operands.at(0);
			auto break_ = instruction.operands.at(1);
			auto body = instruction.operands.at(2).region;
			m_block_mapping[continue_.label] = l0;
			m_block_mapping[break_.label] = l1;
			compile_region(chunk, body, std::nullopt, l0, l0);
			chunk.add_label(l1);
			return;
		}
		case hir::Opcode::RESUME: {
			auto result = translate_operand(instruction.operands.at(0));
			auto continuation = translate_operand(instruction.operands.at(1));
			auto value = translate_operand(instruction.operands.at(2));
			chunk.emit(lir::Opcode::PUSH, value);
			chunk.emit_resume(continuation);
			chunk.emit(lir::Opcode::POP, result);
			return;
		}
		case hir::Opcode::CATCH: {
			auto result = translate_operand(instruction.operands.at(0));
			auto tag = std::stoi(instruction.operands.at(1).label.name);
			auto handler = instruction.operands.at(2).region;
			auto body = instruction.operands.at(3).region;
			auto l0 = m_labels.next();
			auto l1 = m_labels.next();
			auto l2 = m_labels.next();
			auto l3 = m_labels.next();

			chunk.emit(lir::Opcode::CATCH, lir::Immediate(tag), l0, l1);
			chunk.emit(lir::Opcode::POP, result);
			chunk.emit_jmp(l3);

			compile_region(chunk, handler, result.as_register(), l2, l0);
			compile_region(chunk, body, result.as_register(), l2, l1);

			chunk.add_label(l2);
			chunk.emit(lir::Opcode::PUSH, result);
			chunk.emit(lir::Opcode::RET);

			chunk.add_label(l3);
			return;
		}
		case hir::Opcode::CALL: {
			auto hfunction = instruction.operands.at(1);
			auto result = translate_operand(instruction.operands.at(0));
			auto function = translate_operand(hfunction);
			assert(hfunction.kind == hir::Operand::Kind::REGISTER);
			if (m_register_to_builtin.contains(hfunction.registuhr)) {
				auto opcode = m_register_to_builtin.at(hfunction.registuhr);
				if (opcode == lir::Opcode::READC) {
					chunk.emit(lir::Opcode::READC, result);
				} else if (opcode == lir::Opcode::READV) {
					chunk.emit(lir::Opcode::READV, result);
				} else {
					throw std::runtime_error("unknown builtin function");
				}
				return;
			}
			auto alive = live_across(parent_block, index);
			std::cerr << "CALL ALIVE ACCROSS\n";
			std::vector<hir::Register> xs {};
			for (const auto& reg : alive) {
				xs.push_back(reg);
				std::cerr << std::format("ALIVE: %{}", reg.id) << '\n';
			}
			for (const auto& reg : xs) {
				chunk.emit(lir::Opcode::PUSH, translate_operand(reg));
			}
			for (const auto& arg : std::span(instruction.operands).subspan(2)
			                         | std::ranges::views::reverse) {
				auto x = ensure_mutable(chunk, arg);
				chunk.emit(lir::Opcode::PUSH, x);
			}
			chunk.emit(lir::Opcode::CALL, function);
			chunk.emit(lir::Opcode::POP, result);
			for (const auto& reg : xs | std::ranges::views::reverse) {
				chunk.emit(lir::Opcode::POP, translate_operand(reg));
			}
			return;
		}
		case hir::Opcode::ALLOC: {
			auto result = translate_operand(instruction.operands.at(0));
			auto value = translate_operand(instruction.operands.at(1));
			chunk.emit_alloca(result, lir::Immediate(1));
			chunk.emit_storea(value, lir::Immediate(0), result);
			return;
		}
		case hir::Opcode::ENCLOSE: {
			auto result = translate_operand(instruction.operands.at(0));
			auto label =
				m_global_function_to_label.at(instruction.operands.at(1).label.name);
			auto closure = translate_operand(instruction.operands.at(2));
			chunk.emit_enclose(result, label, closure);
			return;
		}
#define A(X)                                                          \
	{                                                                   \
		auto result = translate_operand(instruction.operands.at(0));      \
		auto left = ensure_immutable(chunk, instruction.operands.at(1));  \
		auto right = ensure_immutable(chunk, instruction.operands.at(2)); \
		chunk.emit(X, result, left, right);                               \
		return;                                                           \
	}
		case hir::Opcode::EQ: A(lir::Opcode::EQ);
		case hir::Opcode::ADD: A(lir::Opcode::ADD);
		case hir::Opcode::LESS: A(lir::Opcode::LESS);
		case hir::Opcode::LESS_EQ: A(lir::Opcode::LESS_EQ);
		case hir::Opcode::GREATER_EQ: A(lir::Opcode::GREATER_EQ);
		case hir::Opcode::GREATER: A(lir::Opcode::GREATER);
		case hir::Opcode::SUB: A(lir::Opcode::SUB);
		case hir::Opcode::MUL: A(lir::Opcode::MUL);
		case hir::Opcode::DIV: A(lir::Opcode::DIV);
		case hir::Opcode::MOD:
			A(lir::Opcode::MOD);
			// FIXME: short-circuiting
		case hir::Opcode::OR: A(lir::Opcode::OR);
		case hir::Opcode::AND: A(lir::Opcode::AND);
#undef A
		case hir::Opcode::NOT: {
			auto result = translate_operand(instruction.operands.at(0));
			auto right = translate_operand(instruction.operands.at(1));
			chunk.emit(lir::Opcode::NOT, result, right);
			return;
		}
		case hir::Opcode::NEQ: {
			auto left = translate_operand(instruction.operands.at(1));
			auto right = translate_operand(instruction.operands.at(2));
			auto result = translate_operand(instruction.operands.at(0));
			auto r0 = m_registers.next(lir::Type::UNKNOWN);
			chunk.emit(lir::Opcode::EQ, r0, left, right);
			chunk.emit(lir::Opcode::NOT, result, r0);
			return;
		}
		case hir::Opcode::MAKE_TUPLE: {
			auto result = translate_operand(instruction.operands.at(0));
			chunk.emit_alloca(
				result, lir::Immediate((int64_t)instruction.operands.size() - 1)
			);
			for (auto i = 1ul; i < instruction.operands.size(); i++) {
				auto arg = translate_operand(instruction.operands.at(i));
				chunk.emit_storea(arg, lir::Immediate((int64_t)i - 1), result);
			}
			return;
		}
		case hir::Opcode::YIELD: {
			auto value = translate_operand(instruction.operands.at(0));
			if (auto x = yielding_to) {
				chunk.emit_mov(*x, value);
			} else {
				throw std::runtime_error("yield failed");
			}
			return;
		}
		case hir::Opcode::IF: {
			assert(instruction.operands.size() == 4);
			auto result = translate_operand(instruction.operands.at(0)).as_register();
			auto condition = translate_operand(instruction.operands.at(1));
			auto true_branch = instruction.operands.at(2).region;
			auto false_branch = instruction.operands.at(3).region;
			auto l = m_labels.next();
			auto l1 = m_labels.next();
			auto l2 = m_labels.next();
			chunk.emit(lir::Opcode::JMP_FALSE, condition, l2);
			compile_region(chunk, true_branch, result, l, l1);
			chunk.add_label(l2);
			compile_region(chunk, false_branch, result, l, l2);
			chunk.add_label(l);
			return;
		}
		case hir::Opcode::RET: {
			auto value = ensure_immutable(chunk, instruction.operands.at(0));
			chunk.emit(lir::Opcode::PUSH, value);
			chunk.emit(lir::Opcode::RET);
			return;
		}
		case hir::Opcode::PERFORM: {
			auto result = translate_operand(instruction.operands.at(0));
			auto tag = std::stoi(instruction.operands.at(1).label.name);
			// FIXME: might be in the wrong order
			for (auto i = 2ul; i < instruction.operands.size(); i++) {
				auto amount = ensure_mutable(chunk, instruction.operands.at(i));
				chunk.emit(lir::Opcode::PUSH, amount);
			}
			auto argument_count =
				(int64_t)(instruction.operands.size()
			            - (instruction.operands.size() > 2 ? 2 : 0));
			chunk.emit(
				lir::Opcode::THROW, lir::Immediate(tag), lir::Immediate(argument_count)
			);
			chunk.emit(lir::Opcode::POP, result);
			return;
		}
		case hir::Opcode::GET_GLOBAL: {
			auto name = instruction.operands.at(1).label.name;
			if (m_global_to_builtin.contains(name)) {
				m_register_to_builtin[instruction.operands.at(0).registuhr] =
					m_global_to_builtin.at(name);
				return;
			}
			if (not m_global_to_register.contains(name)) return;
			auto value = m_global_to_register.at(name);
			auto result = translate_operand(instruction.operands.at(0));
			chunk.emit_mov(result, value);
			return;
		}
		case hir::Opcode::LOAD: {
			auto variable = translate_operand(instruction.operands.at(1));
			auto result = translate_operand(instruction.operands.at(0));
			chunk.emit_loada(result, lir::Immediate(0), variable);
			return;
		}
		case hir::Opcode::STORE: {
			auto value = translate_operand(instruction.operands.at(1));
			auto variable = translate_operand(instruction.operands.at(0));
			chunk.emit_storea(value, lir::Immediate(0), variable);
			return;
		}
		case hir::Opcode::CLONE: {
			auto result = translate_operand(instruction.operands.at(0));
			auto value = translate_operand(instruction.operands.at(1));
			chunk.emit(lir::Opcode::CLONEA, result, value);
			return;
		}
		case hir::Opcode::ALIAS: {
			auto result = translate_operand(instruction.operands.at(0));
			auto value = translate_operand(instruction.operands.at(1));
			chunk.emit_mov(result, value);
			return;
		}
		case hir::Opcode::REPEAT: {
			auto element = translate_operand(instruction.operands.at(1));
			auto amount = translate_operand(instruction.operands.at(2));
			auto result = translate_operand(instruction.operands.at(0));
			auto t1 = m_registers.next(lir::Type::UNKNOWN);
			chunk.emit(lir::Opcode::REPEAT, t1, element, amount);
			chunk.emit(lir::Opcode::ALLOCA, result, lir::Immediate(2));
			chunk.emit(lir::Opcode::STOREA, t1, lir::Immediate(0), result);
			chunk.emit(lir::Opcode::STOREA, amount, lir::Immediate(1), result);
			return;
		}
		case hir::Opcode::LENGTH: {
			auto result = translate_operand(instruction.operands.at(0));
			auto value = ensure_immutable(chunk, instruction.operands.at(1));
			chunk.emit_loada(result, lir::Immediate(1), value);
			return;
		}
	} // namespace lower::hir_to_lir
}

lir::Operand Compiler::translate_operand(hir::Operand opnd) {
	if (opnd.kind == hir::Operand::Kind::REGISTER) {
		return m_register_mapping.at(opnd.registuhr);
	} else if (opnd.kind == hir::Operand::Kind::IMMEDIATE) {
		return lir::Immediate(opnd.immediate.value);
	} else {
		throw std::runtime_error("not implemented yet");
	}
}

lir::Register Compiler::ensure_mutable(lir::Chunk& chunk, hir::Operand opnd) {
	if (opnd.kind == hir::Operand::Kind::REGISTER) {
		const auto& reg = opnd.registuhr;
		if (reg.is_mutable) {
			return m_register_mapping.at(reg);
		} else {
			auto r0 = m_registers.next(lir::Type::UNKNOWN);
			auto r1 = m_register_mapping.at(reg);
			chunk.emit_alloca(r0, lir::Immediate(1));
			chunk.emit_storea(r1, lir::Immediate(0), r0);
			return r0;
		}
	} else {
		throw std::runtime_error("not implemented yet");
	}
}

lir::Operand Compiler::ensure_immutable(lir::Chunk& chunk, hir::Operand opnd) {
	if (opnd.kind == hir::Operand::Kind::REGISTER) {
		const auto& reg = opnd.registuhr;
		if (reg.is_mutable) {
			auto r0 = m_registers.next(lir::Type::UNKNOWN);
			auto r1 = m_register_mapping.at(reg);
			chunk.emit_loada(r0, lir::Immediate(0), r1);
			return r0;
		} else {
			return m_register_mapping.at(reg);
		}
	} else if (opnd.kind == hir::Operand::Kind::IMMEDIATE) {
		return lir::Immediate(opnd.immediate.value);
	} else {
		throw std::runtime_error("not implemented yet");
	}
}

void Compiler::compile_region(
	lir::Chunk& chunk, const hir::Region& region,
	std::optional<lir::Register> yielding_to, lir::Label jump_to, lir::Label l
) {
	chunk.add_label(l);
	for (const auto& param : region.parameter_registers) {
		auto r = m_registers.next(lir::Type::UNKNOWN);
		m_register_mapping[param] = r;
		chunk.emit(lir::Opcode::POP, r);
	}
	std::vector<std::pair<std::string, hir::Block>> blocks {};
	for (const auto& [name, blk] : region.blocks) {
		blocks.push_back(std::make_pair(name, blk));
	}
	auto entry = region.entry_block;
	auto found =
		std::ranges::find(blocks, entry, [](const auto& x) { return x.first; });
	if (found == blocks.end()) throw std::runtime_error("entry block not found");
	if (found != blocks.begin()) {
		std::iter_swap(found, blocks.begin());
	}
	for (const auto& [name, blk] : blocks) {
		auto l1 = m_labels.next();
		m_block_mapping[hir::Label {name}] = l1;
		chunk.add_label(l1);
		for (const auto& [index, inst] :
		     *blk.body_code | std::ranges::views::enumerate) {
			compile_instruction(chunk, inst, yielding_to, blk, (std::size_t)index);
		}
	}
	chunk.emit_jmp(jump_to);
}

void Compiler::reserve_register(const hir::Register& r) {
	auto result = m_registers.next(lir::Type::UNKNOWN);
	m_register_mapping[r] = result;
}

void Compiler::reserve_registers(const hir::Function& f) {
	reserve_registers(f.region);
}

void Compiler::reserve_registers(const hir::Region& region) {
	for (const auto& reg : region.parameter_registers) {
		reserve_register(reg);
	}
	std::vector<std::pair<std::string, hir::Block>> blocks {};
	for (const auto& [name, blk] : region.blocks) {
		blocks.push_back(std::make_pair(name, blk));
	}
	auto entry = region.entry_block;
	auto found =
		std::ranges::find(blocks, entry, [](const auto& x) { return x.first; });
	if (found == blocks.end()) throw std::runtime_error("entry block not found");
	if (found != blocks.begin()) {
		std::iter_swap(found, blocks.begin());
	}
	for (const auto& [name, blk] : blocks) {
		for (const auto& inst : *blk.body_code) {
			reserve_registers(inst);
		}
	}
}
void Compiler::reserve_registers(const hir::Instruction& instruction) {
	switch (instruction.opcode) {
		case hir::Opcode::COPY:
		case hir::Opcode::PRINTV:
		case hir::Opcode::PRINTC:
		case hir::Opcode::PRINTF:
		case hir::Opcode::SET_ELEMENT: {
			throw std::runtime_error(
				std::format(
					"unhandled opcode {}", hir::opcode_to_string(instruction.opcode)
				)
			);
		}
		case hir::Opcode::PANIC: return;
#define A                                                   \
	{                                                         \
		reserve_register(instruction.operands.at(0).registuhr); \
		return;                                                 \
	};                                                        \
	(void)0
		case hir::Opcode::GET_VARIANT: A;
		case hir::Opcode::GET_CONSTRUCTOR: A;
		case hir::Opcode::MAKE_ARRAY: A;
		case hir::Opcode::MAKE_VARIANT: A;
		case hir::Opcode::BREAK: return;
		case hir::Opcode::CONTINUE: return;
		case hir::Opcode::IF_TRUE: {
			reserve_registers(instruction.operands.at(1).region);
			return;
		}
		case hir::Opcode::IF_FALSE: {
			reserve_registers(instruction.operands.at(1).region);
			return;
		}
		case hir::Opcode::GET_ELEMENT: A;
		case hir::Opcode::LOOP: {
			reserve_registers(instruction.operands.at(2).region);
			return;
		}
		case hir::Opcode::RESUME: A;
		case hir::Opcode::CATCH: {
			reserve_register(instruction.operands.at(0).registuhr);
			reserve_registers(instruction.operands.at(2).region);
			reserve_registers(instruction.operands.at(3).region);
			return;
		}
		case hir::Opcode::CALL: A;
		case hir::Opcode::ALLOC: A;
		case hir::Opcode::ENCLOSE: A;
		case hir::Opcode::EQ: A;
		case hir::Opcode::ADD: A;
		case hir::Opcode::LESS: A;
		case hir::Opcode::LESS_EQ: A;
		case hir::Opcode::GREATER_EQ: A;
		case hir::Opcode::GREATER: A;
		case hir::Opcode::SUB: A;
		case hir::Opcode::MUL: A;
		case hir::Opcode::DIV: A;
		case hir::Opcode::MOD: A;
		case hir::Opcode::OR: A;
		case hir::Opcode::AND: A;
		case hir::Opcode::NOT: A;
		case hir::Opcode::NEQ: A;
		case hir::Opcode::MAKE_TUPLE: A;
		case hir::Opcode::YIELD: return;
		case hir::Opcode::IF: {
			reserve_register(instruction.operands.at(0).registuhr);
			reserve_registers(instruction.operands.at(2).region);
			reserve_registers(instruction.operands.at(3).region);
			return;
		}
		case hir::Opcode::RET: return;
		case hir::Opcode::PERFORM: A;
		case hir::Opcode::GET_GLOBAL: A;
		case hir::Opcode::LOAD: A;
		case hir::Opcode::STORE: return;
		case hir::Opcode::CLONE: A;
		case hir::Opcode::ALIAS: A;
		case hir::Opcode::REPEAT: A;
		case hir::Opcode::LENGTH: A;
	}
}

} // namespace lower::hir_to_lir
