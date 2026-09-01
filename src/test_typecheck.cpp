#include <gtest/gtest.h>

#include <memory>

#include "ast.hpp"
#include "parser.hpp"
#include "scope_analyser.hpp"
#include "string_reader.hpp"
#include "symbol_table.hpp"
#include "type.hpp"
#include "typecheck.hpp"

TEST(TypecheckTest, function_application) {
	std::string source {"let fun is_inst op = op >= 0 and op <= 7 in is_inst(6)"};
	Reader* reader = new StringReader(source);
	StringPool pool {};
	auto ast = parse(reader, pool, true);
	ScopeAnalyser scop {ast, pool};
	const auto symtab = scop.analyse();
	Typechecker checker {ast, pool, symtab};
	checker.typecheck();
	const auto syms =
		symtab.symbols_with_name(pool.intern("is_inst"), symtab.value_namespace);
	EXPECT_TRUE(syms.size() >= 1);
	const auto sym = syms.at(0);
	const auto ty = checker.variables.at(sym);
	EXPECT_TRUE(ty != nullptr);
	const auto dty = get_datatype(make_type(ty->mode, ty->scheme->body));
	EXPECT_TRUE(dty != nullptr);
	const auto fty = std::dynamic_pointer_cast<Closure>(dty);
	EXPECT_TRUE(fty != nullptr);
	for (const auto& pty : fty->inputs) {
		EXPECT_TRUE(pty != nullptr);
		const auto pmode = pty->mode;
		EXPECT_TRUE(pmode != nullptr);
		const auto pcmode = pmode->to_concrete();
		EXPECT_TRUE(pcmode.has_value());
	};
}

TEST(TypecheckerTest, unifying_quantified_variable_with_inference) {
	StringPool pool {};
	AST ast {};
	SymbolTable symtab {};
	Typechecker checker {ast, pool, symtab};
	auto gen = std::dynamic_pointer_cast<TypeVariable>(checker.make_genvar());
	auto inf = std::dynamic_pointer_cast<TypeVariable>(checker.make_datavar());
	EXPECT_TRUE(not inf->is_bound);
	EXPECT_TRUE(checker.unify(gen, inf));
	EXPECT_TRUE(inf->is_bound);
	EXPECT_EQ(std::dynamic_pointer_cast<TypeVariable>(inf->bound_type), gen);
}

TEST(TypecheckerTest, unifying_inference_variable_with_quantified) {
	StringPool pool {};
	AST ast {};
	SymbolTable symtab {};
	Typechecker checker {ast, pool, symtab};
	auto gen = std::dynamic_pointer_cast<TypeVariable>(checker.make_genvar());
	auto inf = std::dynamic_pointer_cast<TypeVariable>(checker.make_datavar());
	EXPECT_TRUE(not inf->is_bound);
	EXPECT_TRUE(checker.unify(inf, gen));
	EXPECT_TRUE(inf->is_bound);
	EXPECT_EQ(std::dynamic_pointer_cast<TypeVariable>(inf->bound_type), gen);
}

TEST(TypecheckerTest, instantiate_polymorphic_function) {
	StringPool pool {};
	AST ast {};
	SymbolTable symtab {};
	Typechecker checker {ast, pool, symtab};
	auto g19 = checker.make_genvar();
	auto g20 = checker.make_genvar();
	auto VAL_ = std::make_shared<Mode>(Mode::Concrete::VAL);
	const auto EMPTY_EFFSET = []() {
		std::vector<std::shared_ptr<Effect>> effs {};
		return std::make_shared<EffectSet>(effs);
	}();
	auto a = make_function({make_type(VAL_, g19)}, g20, EMPTY_EFFSET);
	auto b = make_function(
		{make_type(VAL_, make_array(g19)),
	   make_type(checker.make_modevar(), g20),
	   make_type(checker.make_modevar(), a)},
		make_array(g20),
		EMPTY_EFFSET
	);
	auto c = make_polytype(VAL_, make_scheme({g19, g20}, b));
	EXPECT_EQ(
		std::format("{}", *c),
		"val ∀'g0 'g1 . (val Array<'g0>, m0 'g1, m1 (val 'g0) -> 'g1) -> Array<'g1>"
	);
	auto d = checker.instantiate(c);
	auto expected =
		"val (val Array<'t2>, m0 't3, m1 (val 't2) -> 't3) -> Array<'t3>";
	auto actual = std::format("{}", *d);
	EXPECT_EQ(actual, expected);
}

TEST(TypecheckerTest, unifying_row_set_with_concrete_record) {
	StringPool pool {};
	AST ast {};
	SymbolTable symtab {};
	Typechecker checker {ast, pool, symtab};
	auto a = make_row_set(
		{{"x", make_array({make_integer(8, Integer::Sign::UNSIGNED)})}}
	);
	auto b = make_record(
		{{"x", make_array({make_integer(8, Integer::Sign::UNSIGNED)})}}
	);
	checker.unify(a, b);
	EXPECT_EQ(std::format("{}", *a), "{x:Array<UInt<8>>}");
	EXPECT_EQ(std::format("{}", *b), "{x:Array<UInt<8>>}");
}
