#include "vm.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>

#include "lir/lir.hpp"

using lir::Operand;

template<typename... Args>
std::string join(Args&&... args) {
	std::string result;
	((result += std::format("{} ", std::forward<Args>(args))), ...);
	return result;
}

template<typename T, typename... Args>
void assert_oneof(T v, Args&&... args) {
	if (not((v == args) || ...)) {
		throw std::domain_error(
			std::format("value {} is not one of {}", v, join(args...))
		);
	}
}

template<>
struct std::formatter<Operand::Type> {
	constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
	auto format(const Operand::Type& opnd_ty, std::format_context& ctx) const {
		switch (opnd_ty) {
			case lir::Operand::Type::NOTHING:
				return std::format_to(ctx.out(), "NOTHING");
			case lir::Operand::Type::REGISTER:
				return std::format_to(ctx.out(), "REGISTER");
			case lir::Operand::Type::LABEL: return std::format_to(ctx.out(), "LABEL");
			case lir::Operand::Type::IMMEDIATE:
				return std::format_to(ctx.out(), "IMMEDIATE");
			case lir::Operand::Type::BYTE_STRING:
				return std::format_to(ctx.out(), "BYTE_STRING");
		}
		assert(false);
	}
};

namespace lir {
static Value clone(Value);
static void err(const char* msg);

void err(const char* msg) {
	fprintf(stderr, "VM ERROR: %s\n", msg);
	exit(1);
}

std::size_t VM::address_of_label(const Chunk& code, lir::Label label) {
	return code.label_indexes.at(label.id);
}

auto VM::deref(Operand opnd) -> Value& {
	if (opnd.type == Operand::Type::REGISTER) {
		return cells[opnd.as_register().index];
	} else {
		fprintf(stderr, "%s\n", operand_type_repr(opnd.type));
		exit(1);
	}
};

auto VM::fetch(Operand opnd) -> Value {
	if (opnd.type == Operand::Type::REGISTER) {
		return cells[opnd.as_register().index];
	} else if (opnd.type == Operand::Type::IMMEDIATE) {
		return opnd.as_immediate().number;
	} else if (opnd.type == Operand::Type::NOTHING) {
		return 0;
	} else {
		fprintf(stderr, "%s\n", operand_type_repr(opnd.type));
		exit(1);
	}
};

void VM::run(const lir::Chunk& code) {
	while (program_counter < code.m_vec.size()) {
		const auto& inst = code.m_vec.at(program_counter);
		switch (inst.opcode) {
			case Opcode::PRINTF: {
				auto t1 = inst.operands[0];
				auto t4 = fetch(t1).as_pointer();
				auto t3 = (*(&t4[0])).as_pointer();
				auto t2 = *(&t3[0]);
				if (not t2.is_pointer())
					throw std::runtime_error("printf operand was not a pointer");
				auto base = t2.as_pointer();
				auto size = base.size();
				for (size_t i = 0; i < size; i++) {
					auto c = (*base[i]).as_integer();
					if (c == 0) break;
					output << (char)c;
				}
				break;
			}
			case Opcode::PRINTV: {
				auto t1 = fetch(inst.operands[0]);
				assert(t1.is_integer());
				output << t1.as_integer();
				break;
			}
			case Opcode::PRINTC: {
				auto t1 = fetch(inst.operands[0]);
				assert(t1.is_integer());
				output << (char)t1.as_integer();
				break;
			}
			case Opcode::READV: {
				std::string line {};
				if (not std::getline(input, line)) err("Couldn't read input");
				int num = std::stoi(line);
				if (inst.operands[0].type != Operand::Type::REGISTER)
					err("First argument must be a register");
				deref(inst.operands[0]) = Value::Integer(num);
				break;
			}
			case Opcode::READC: {
				char c;
				input >> c;
				deref(inst.operands[0]) = (c == EOF) ? -1 : c;
				break;
			}
			case Opcode::MOV: {
				const auto& dest = inst.operands[0];
				const auto& src = inst.operands[1];
				auto& cell = cells[dest.as_register().index];
				cell = fetch(src);
				break;
			}
#define BIN_ARITH_OP(OP)                          \
	{                                               \
		auto t1 = fetch(inst.operands[1]);            \
		assert(t1.is_integer());                      \
		auto t2 = fetch(inst.operands[2]);            \
		assert(t2.is_integer());                      \
		cells[inst.operands[0].as_register().index] = \
			t1.as_integer() OP t2.as_integer();         \
	}
			case Opcode::ADD: BIN_ARITH_OP(+); break;
			case Opcode::SUB: BIN_ARITH_OP(-); break;
			case Opcode::MUL: BIN_ARITH_OP(*); break;
			case Opcode::DIV: BIN_ARITH_OP(/); break;
			case Opcode::MOD: BIN_ARITH_OP(%); break;
			case Opcode::OR: BIN_ARITH_OP(||); break;
			case Opcode::AND: BIN_ARITH_OP(&&); break;
			case Opcode::EQ: BIN_ARITH_OP(==); break;
			case Opcode::DIFF: BIN_ARITH_OP(!=); break;
			case Opcode::LESS: BIN_ARITH_OP(<); break;
			case Opcode::LESS_EQ: BIN_ARITH_OP(<=); break;
			case Opcode::GREATER: BIN_ARITH_OP(>); break;
			case Opcode::GREATER_EQ: BIN_ARITH_OP(>=); break;
			case Opcode::NOT:
				deref(inst.operands[0]) = !fetch(inst.operands[1]).as_integer();
				break;
			case Opcode::JMP: {
				program_counter = code.label_indexes.at(inst.operands[0].as_label().id);
				goto dont_inc;
			}
			case Opcode::JMP_FALSE:
				if (!fetch(inst.operands[0]).as_integer())
					program_counter = address_of_label(code, inst.operands[1].as_label());
				else
					program_counter++;
				goto dont_inc;
			case Opcode::JMP_TRUE:
				if (fetch(inst.operands[0]).as_integer())
					program_counter = address_of_label(code, inst.operands[1].as_label());
				else
					program_counter++;
				goto dont_inc;
			case Opcode::PUSH: {
				if (inst.operands[0].type == Operand::Type::IMMEDIATE) {
					stack.push(Value(inst.operands[0].as_immediate().number));
				} else if (inst.operands[0].type == Operand::Type::REGISTER) {
					stack.push(cells[inst.operands[0].as_register().index]);
				} else if (inst.operands[0].type == Operand::Type::NOTHING) {
					stack.push(Value(0));
				} else {
					throw std::runtime_error("can't push value");
				}
				break;
			}
			case Opcode::POP: {
				assert(inst.operands[0].type == Operand::Type::REGISTER);
				auto& cell = cells[inst.operands[0].as_register().index];
				cell = stack.top();
				stack.pop();
				break;
			}
			case Opcode::CALL: {
				//
				// r0 : *---> [cls *, fn $]
				//                 |
				//                 +---> ...
				auto closure_pointer = fetch(inst.operands[0]).as_pointer();
				auto env_pointer = *(&closure_pointer[0]);
				stack.push(env_pointer);
				stack.push(Value((int64_t)program_counter));
				auto function_address =
					static_cast<std::size_t>((*(&closure_pointer[1])).as_integer());
				program_counter = function_address;
				goto dont_inc;
			}
			case Opcode::RET:
				program_counter = return_address;
				return_address = callstack.top();
				callstack.pop();
				break;
			case Opcode::FUNC:
				callstack.push(return_address);
				return_address = (size_t)stack.top().as_integer();
				stack.pop();
				break;
			case Opcode::ALLOCA: {
				auto result_register = inst.operands[0];
				auto size_register = inst.operands[1];
				assert(result_register.type == Operand::Type::REGISTER);
				assert(
					size_register.type == Operand::Type::REGISTER
					or size_register.type == Operand::Type::IMMEDIATE
				);
				auto a = fetch(size_register);
				if (not a.is_integer())
					throw std::runtime_error("alloca size was not an integer");
				auto size_integer = fetch(size_register).as_integer();
				assert(size_integer >= 0);
				auto register_index = result_register.as_register().index;
				auto& cell = cells[register_index];
				if (size_integer == 0) {
					cell = Value(nullptr, 0);
				} else {
					cell = Value(new Value[(size_t)size_integer], (size_t)size_integer);
				}
				if (not cell.is_pointer())
					throw std::runtime_error("allocated value was not a pointer");
				break;
			}
			case Opcode::STOREA: {
				auto value_register = inst.operands[0];
				auto offset_register = inst.operands[1];
				auto base_register = inst.operands[2].as_register();
				assert_oneof(
					value_register.type, Operand::Type::REGISTER, Operand::Type::IMMEDIATE
				);
				assert(
					value_register.type == Operand::Type::REGISTER
					or value_register.type == Operand::Type::IMMEDIATE
				);
				assert(
					offset_register.type == Operand::Type::REGISTER
					or offset_register.type == Operand::Type::IMMEDIATE
				);
				auto value = fetch(value_register);
				auto a = fetch(offset_register);
				if (not a.is_integer())
					throw std::runtime_error("storea offset operand was not an integer");
				auto offset = a.as_integer();
				if (not cells[base_register.index].is_pointer())
					throw std::runtime_error("storea base operand was not a pointer");
				auto t1 = cells[base_register.index];
				auto pointer = t1.as_pointer();
				auto cell = pointer[(size_t)offset];
				*cell = Value(value);
				break;
			}
			case Opcode::LOADA: {
				auto result_register = inst.operands[0];
				auto offset_register = inst.operands[1];
				auto pointer_register = inst.operands[2].as_register();
				assert(result_register.type == Operand::Type::REGISTER);
				auto a = fetch(offset_register);
				if (not a.is_integer())
					throw std::runtime_error("loada offset operand was not an integer");
				auto offset = a.as_integer();
				auto t1 = cells[pointer_register.index];
				if (not t1.is_pointer())
					throw std::runtime_error("loada base operand was not a pointer");
				auto pointer = t1.as_pointer();
				auto value = pointer[(size_t)offset];
				auto& cell = cells[result_register.as_register().index];
				cell = *value;
				break;
			}
			case Opcode::SHIFTA: {
				auto result_register = inst.operands[0];
				auto offset_register = inst.operands[1];
				auto pointer_register = inst.operands[2];
				assert(result_register.type == Operand::Type::REGISTER);
				assert(
					offset_register.type == Operand::Type::REGISTER
					|| offset_register.type == Operand::Type::IMMEDIATE
				);
				assert(pointer_register.type == Operand::Type::REGISTER);
				auto t1 = fetch(offset_register);
				if (not t1.is_integer())
					throw std::runtime_error("shifta offset operand was not an integer");
				auto offset = t1.as_integer();
				auto t2 = cells[pointer_register.as_register().index];
				if (not t2.is_pointer())
					throw std::runtime_error("shifta base operand was not a pointer");
				auto pointer = t2.as_pointer();
				auto x = &pointer[(size_t)offset];
				cells[result_register.as_register().index] = x;
				break;
			}
			case Opcode::CLONEA: {
				auto result_register = inst.operands[0];
				auto source_register = inst.operands[1];
				assert(result_register.type == Operand::Type::REGISTER);
				assert(source_register.type == Operand::Type::REGISTER);
				auto source = cells[source_register.as_register().index];
				auto& destination = cells[result_register.as_register().index];
				destination = clone(source);
				break;
			}
			case Opcode::NOP: {
				break;
			}
			case Opcode::REPEAT: {
				auto result_register = inst.operands[0];
				auto element_register = inst.operands[1];
				auto amount_register = inst.operands[2];
				assert(result_register.type == Operand::Type::REGISTER);
				assert(
					element_register.type == Operand::Type::REGISTER
					|| element_register.type == Operand::Type::IMMEDIATE
				);
				assert(
					amount_register.type == Operand::Type::REGISTER
					|| amount_register.type == Operand::Type::IMMEDIATE
				);
				auto t1 = fetch(element_register);
				auto amount = fetch(amount_register).as_integer();
				auto ptr = Value(
					new Value[static_cast<std::size_t>(amount)],
					static_cast<std::size_t>(amount)
				);
				for (auto i = 0ul; i < static_cast<std::size_t>(amount); i++) {
					auto iptr = &(ptr.as_pointer())[i];
					*iptr = clone(t1);
				}
				cells[result_register.as_register().index] = ptr;
				break;
			}
			case Opcode::INITA: {
				auto result_register = inst.operands[0];
				auto element_register = inst.operands[1];
				assert(result_register.type == Operand::Type::REGISTER);
				assert(element_register.type == Operand::Type::BYTE_STRING);
				auto t1 = fetch(result_register).as_pointer();
				auto i = 0ul;
				for (auto byte : element_register.as_byte_string().bytes) {
					*(&t1[i]) = byte;
					i++;
				}
				break;
			}
			case Opcode::ENCLOSE: {
				auto& result = deref(inst.operands[0]);
				auto label = Value(
					static_cast<int64_t>(
						address_of_label(code, inst.operands[1].as_label())
					)
				);
				auto closure = fetch(inst.operands[2]);
				auto object = Value(new Value[2] {closure, label}, 2);
				result = object;
				break;
			}
			case Opcode::CATCH: {
				const auto tag =
					static_cast<std::size_t>(fetch(inst.operands[0]).as_integer());
				const auto npc = address_of_label(code, inst.operands[1].as_label());
				const auto k = save_continuation();
				prompt_tags.push_back(
					std::make_pair(tag, std::make_shared<Continuation>(k))
				);
				program_counter = npc;
				goto dont_inc;
			}
			case Opcode::RESUME: {
				const auto k = fetch(inst.operands[0]).as_continuation();
				restore_continuation(*k);
				break;
			}
			case Opcode::THROW: {
				const auto tag =
					static_cast<std::size_t>(fetch(inst.operands[0]).as_integer());
				const auto arg_count =
					static_cast<std::size_t>(fetch(inst.operands[1]).as_integer());
				const auto maybe_handler = find_handler(tag);
				if (not maybe_handler.has_value()) {
					throw std::runtime_error("Thrown value without an enclosing handler");
				} else {
					const auto current_cont = save_continuation();
					const auto handler = maybe_handler.value();
					std::vector<Value> args {};
					for (auto i = 0ul; i < arg_count; i++) {
						args.push_back(stack.top());
						stack.pop();
					}
					restore_continuation(handler);
					for (const auto& a : std::ranges::views::reverse(args)) {
						stack.push(a);
					}
					stack.push(Value(std::make_shared<Continuation>(current_cont)));
				}
				break;
			}
		}
		program_counter++;
	dont_inc:
		(void)0;
	}

	if (should_print_result and code.result_opnd.has_value()) {
		auto result = code.result_opnd.value();
		if (result.type == Operand::Type::IMMEDIATE) {
			auto t1 = fetch(result);
			assert(t1.is_integer());
			std::cout << "==> " << t1.as_integer() << '\n';
		} else if (result.type == Operand::Type::REGISTER) {
			std::cout << '%' << result.as_register().index << ' ';
			auto reg = cells[result.as_register().index];
			if (reg.is_integer()) {
				std::cout << "==> " << reg.as_integer() << '\n';
			} else if (reg.is_pointer()) {
				std::cout << "==> 0d" << reg.as_pointer().address() << '\n';
			} else if (reg.is_undefined()) {
				throw std::runtime_error("undefined result");
			}
		} else {
			std::cout << "VM ERROR: Couldn't print value" << '\n';
		}
	}
}

static Value clone(Value val) {
	if (val.is_undefined()) {
		return Value();
	} else if (val.is_integer()) {
		return Value::Integer(val.as_integer());
	} else if (val.is_pointer()) {
		return clone_pointer(val.as_pointer());
	} else {
		assert(false);
	}
}

Value::Pointer clone_pointer(Value::Pointer ptr) {
	auto a = new Value[ptr.m_size];
	for (auto i = 0ul; i < ptr.m_size; i++) a[i] = clone(ptr.m_pointer[i]);
	Value::Pointer ptr2(a, ptr.m_size);
	return ptr2;
}

Continuation VM::save_continuation() {
	return Continuation {
		.cells = cells,
		.stack = stack,
		.callstack = callstack,
		.prompt_tags = prompt_tags,
		.program_counter = program_counter,
		.return_address = return_address,
	};
}

void VM::restore_continuation(const Continuation& k) {
	cells = k.cells;
	stack = k.stack;
	callstack = k.callstack;
	prompt_tags = k.prompt_tags;
	program_counter = k.program_counter;
	return_address = k.return_address;
}

std::optional<Continuation> VM::find_handler(std::size_t tag) {
	for (const auto& [tag2, handler] : std::ranges::views::reverse(prompt_tags))
		if (tag2 == tag) return *handler;
	return std::nullopt;
}

} // namespace lir
