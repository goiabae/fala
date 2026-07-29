#include <gtest/gtest.h>

#include <memory>

#include "parser.hpp"
#include "scope_analyser.hpp"
#include "string_reader.hpp"
#include "type.hpp"
#include "typecheck.hpp"

TEST(TypecheckTest, function_application) {
	std::string source {"let fun is_inst op = op >= 0 and op <= 7 in is_inst(6)"};
	Reader* reader = new StringReader(source);
	StringPool pool {};
	auto ast = parse(reader, pool);
	ScopeAnalyser scop {ast, pool};
	const auto symtab = scop.analyse();
	Typechecker checker {ast, pool, symtab};
	checker.typecheck();
	const auto syms = symtab.symbols_with_name(pool.intern("is_inst"));
	EXPECT_TRUE(syms.size() >= 1);
	const auto sym = syms.at(0);
	const auto ty = checker.variables.at(sym);
	EXPECT_TRUE(ty != nullptr);
	const auto dty = get_datatype(ty);
	EXPECT_TRUE(dty != nullptr);
	const auto fty = std::dynamic_pointer_cast<Function>(dty);
	EXPECT_TRUE(fty != nullptr);
	for (const auto& pty : fty->inputs) {
		EXPECT_TRUE(pty != nullptr);
		const auto pmode = pty->mode;
		EXPECT_TRUE(pmode != nullptr);
		const auto pcmode = pmode->to_concrete();
		EXPECT_TRUE(pcmode.has_value());
	};
}
