#ifndef DECISION_TREE_HPP
#define DECISION_TREE_HPP

#include <optional>
#include <sstream>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "indexes.hpp"
#include "utils.hpp"

// Nodes are predicates on a value
// Leaf nodes are tautologies that point to a branch AST node
// Nodes may bind any number of values (a register in some IR) when succesfully
// matched. In the ASCII diagrams, failed edges to the left, successful to
// the right. If a match is infalible, it's successor is represented as a
// straight down edge.

// match e1 with                       Always
//   Some(x) then e2  ===>               |
//   None() then e3           ConstructorMatch(Some): v1
//                                   /           \.
//                 ConstructorMatch(None)       UnconditionalBind(x; v1)
//                              |                      |
//                             Leaf(e2)              Leaf(e3)

// match e1 with                     Always
//   3 then e2          ===>           |
//   _ then e3                    Equation("3")
//                                 /         \.
//                             Always        Leaf(e3)
//                               |
//                             Leaf(e2)

namespace decision_tree {

template<typename Register, typename Constructor>
struct Node {
	struct Always {
		Node* then;
	};

	struct ConstructorMatch {
		Register value;
		Constructor constructor;
		std::vector<Register> constructor_arguments;
		Node* yes;
		Node* no;
	};

	struct UnconditionalBind {
		Guid name;
		Register value;
		Node* then;
	};

	struct Equation {
		Register value;
		NodeIndex equal_to;
		Node* yes;
		Node* no;
	};

	struct Leaf {
		NodeIndex then_branch;
	};

	struct Panic {};

	std::variant<
		Always, ConstructorMatch, UnconditionalBind, Equation, Leaf, Panic>
		data;

	std::optional<Register> input_register() {
		return std::visit(
			overloaded {
				[&](const Node::Always&) -> std::optional<Register> {
					return std::nullopt;
				},
				[&](const Node::ConstructorMatch& x) -> std::optional<Register> {
					return x.value;
				},
				[&](const Node::UnconditionalBind&) -> std::optional<Register> {
					return std::nullopt;
				},
				[&](const Node::Equation& x) -> std::optional<Register> {
					return x.value;
				},
				[&](const Node::Leaf&) -> std::optional<Register> {
					return std::nullopt;
				},
				[&](const Node::Panic&) -> std::optional<Register> {
					return std::nullopt;
				},
			},
			data
		);
	}
};

template<typename Register, typename Constructor>
inline std::ostream& operator<<(
	std::ostream& st, const Node<Register, Constructor>& node
) {
	return std::visit(
		overloaded {
			[&](const Node<Register, Constructor>::Always& x) -> std::ostream& {
				return st << '(' << "always" << *x.then << ')';
			},
			[&](const Node<Register, Constructor>::ConstructorMatch& x)
				-> std::ostream& {
				st << '(' << "constructor tag:" << (int)x.constructor << ' ';
				st << "value:" << 'r' << x.value.id << ' ';
				st << "args:" << '(';
				for (auto arg : x.constructor_arguments) st << 'r' << arg.id << ' ';
				st << ')' << ' ';
				st << "yes:" << *x.yes << ' ' << "no:" << *x.no;
				st << ')';
				return st;
			},
			[&](const Node<Register, Constructor>::UnconditionalBind& x)
				-> std::ostream& {
				st << '(';
				st << "bind name:" << x.name.index << " value:" << 'r' << x.value.id;
				st << " " << *x.then;
				st << ')';
				return st;
			},
			[&](const Node<Register, Constructor>::Equation& x) -> std::ostream& {
				st << '(';
				st << "equation #";
				st << x.equal_to.index;
				st << " yes:" << *x.yes;
				st << " no:" << *x.no;
				st << ')';
				return st;
			},
			[&](const Node<Register, Constructor>::Leaf& x) -> std::ostream& {
				st << '(';
				st << "then #" << x.then_branch.index;
				st << ')';
				return st;
			},
			[&](const Node<Register, Constructor>::Panic&) -> std::ostream& {
				return st << "(panic)";
			},
		},
		node.data
	);
}

} // namespace decision_tree

template<typename Register, typename Constructor>
struct std::formatter<decision_tree::Node<Register, Constructor>>
: std::formatter<std::string> {
	auto format(
		const decision_tree::Node<Register, Constructor>& p,
		std::format_context& ctx
	) const {
		std::ostringstream oss {};
		oss << p;
		return std::formatter<std::string>::format(oss.str(), ctx);
	}
};

#endif
