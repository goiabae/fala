%require "3.0.5"
%language "c++"
%skeleton "lalr1.cc"

%define parse.trace
%define parse.error verbose

%locations

%define api.location.type {Location}

/* type of symbols ($N and $$) in grammar actions */
%define api.value.type {union TokenValue}

%lex-param {Lexer* lexer}
%parse-param {Lexer* lexer}{AST* ast}{StringPool& pool}

/* necessary for node functions */
%code requires {
#include "str_pool.h"
#include "ast.hpp"
#include "lexer.hpp"
#include "token_value.hpp"

#define yylex lexer_lex
}

%code provides {
AST parse(Reader* reader, StringPool& pool, bool is_repl);
}

%{
#include <stdio.h>

#include "parser.hpp"

#define NODE(TYPE, ...) new_node(ast, TYPE, {__VA_ARGS__})
%}

%token YYEOF 0 /* old versions of bison don't predefine this */

/* values */
%token <num> NUMBER
%token <str> ID
%token <str> STRING
%token <character> CHAR

/* keywords and constants */
%token DO END IF THEN ELSE WHEN FOR FROM TO STEP WHILE BREAK CONTINUE VAR VAL OUT LET IN FUN BOOL INT UINT AS
%token NIL TRUE FALSE TYPE

/* pontuation */
%token PAREN_OPEN "(" PAREN_CLOSE ")"
%token BRACKET_OPEN "[" BRACKET_CLOSE "]"
%token CURLY_OPEN "{" CURLY_CLOSE "}"
%token SEMICOL ";" COLON ":"
%token COMMA ","
%token DOT "."
%token NEWLINE "\n"

/* binary operators */
%token EQ "="
%token OR AND
%token GREATER_EQ ">=" LESSER_EQ "<=" EQ_EQ "==" GREATER ">" LESSER "<"
%token PLUS "+" MINUS "-"
%token ASTER "*" SLASH "/" PERCT "%"
%token DOTDOT ".."
%token POUND "#"
%token NOT

%token START_REPL START_MODULE

/* non-terminals */
%type <node> exp

%type <node> do block stmts
%type <node> cond
%type <node> loop step
%type <node> jump
%type <node> let decls
%type <node> decl params opt_type
%type <node> app func args arg param mode
%type <node> ass path at dot
%type <node> op op0 opa op1 op2 op3 op4 op5 op6 op7 op9 op10 op11 op12 op13 op14 op15 op16

%type <node> term id int
%type <node> type_expression type_argument_list type_argument type_instantiation
%type <node> record_literal field_list field
%type <node> record_type_literal field_type_list field_type
%type <node> array_type_literal
%type <node> array_literal element_list
%type <node> module_body declaration_list
%type <node> program
%type <node> type_declaration

%%

%start start ;

start
  : START_REPL program
  | START_MODULE module_body {
    ast_set_root(ast, $module_body);
  }
  ;

module_body
  : nls declaration_list nls {
    $$ = NODE(NodeType::MODULE_BODY, $2);
  }
  ;

declaration_list
  : decl {
    $$ = new_list_node(ast, NodeType::METALIST);
    $$ = list_append_node(ast, $$, $1);
  }
  | declaration_list[prefix] nls decl[postfix] {
    $$ = list_append_node(ast, $prefix, $postfix);
  }
  ;

program : %empty { if (is_interactive(lexer)) YYACCEPT; }
        | nls exp nls  { ast_set_root(ast, $2); if (is_interactive(lexer)) YYACCEPT; }
        ;

exp : do
    | cond
    | loop
    | jump
    | let
    | app
    | ass
    | op
    ;

nls : %empty | nls NEWLINE ;

/* Expression sequences */
do : DO nls block END { $$ = $block; } ;

block : stmts exp opt_seps { $$ = list_append_node(ast, $1, $2); } ;

opt_seps : %empty | stmt_seps ;

stmts : %empty { $$ = new_list_node(ast, NodeType::BLK); }
      | stmts exp stmt_seps { $$ = list_append_node(ast, $$, $2); }
      | stmts decl stmt_seps { $$ = list_append_node(ast, $$, $2); }
      ;

stmt_seps : stmt_sep | stmt_seps stmt_sep ;

stmt_sep : NEWLINE | ";" ;

/* Conditionals */
cond : IF exp nls THEN nls exp[then] nls ELSE nls exp[else] { $$ = NODE(NodeType::IF, $2, $then, $else); }
     | WHEN exp  THEN nls exp[then]        { $$ = NODE(NodeType::WHEN, $2, $then); }
     ;

/* Loops */
loop : WHILE exp THEN nls exp[then]            { $$ = NODE(NodeType::WHILE, $2, $then); }
     | FOR decl TO exp step THEN nls exp[then] { $$ = NODE(NodeType::FOR, $2, $4, $5, $then); }
     ;

step : %empty   { $$ = new_empty_node(ast); }
     | STEP exp { $$ = $2; }
     ;

/* Jumps */
jump : BREAK exp    { $$ = NODE(NodeType::BREAK, $2); }
     | CONTINUE exp { $$ = NODE(NodeType::CONTINUE, $2); }
     ;

/* Let bindings */
let : LET nls decls nls IN nls exp { $$ = NODE(NodeType::LET, $decls, $exp); }
    | LET nls decls nls "," nls IN nls exp { $$ = NODE(NodeType::LET, $decls, $exp); }
    ;

decls : decl           { $$ = new_list_node(ast, NodeType::METALIST); $$ = list_append_node(ast, $$, $1); }
      | decls nls "," nls decl { $$ = list_append_node(ast, $$, $decl); }
      ;

/* Declarations */
decl
	: mode id opt_type "=" nls exp       { $$ = NODE(NodeType::VAR_DECL, $1, $2, $3, $exp); }
	| FUN id params opt_type "=" nls exp { $$ = NODE(NodeType::FUN_DECL, $2, $3, $4, $exp); }
	| type_declaration
	;

opt_type : %empty { $$ = new_empty_node(ast); }
         | ":" type_expression { $$ = $2; }
         ;

params : %empty    { $$ = new_list_node(ast, NodeType::METALIST); }
       | params param { $$ = list_append_node(ast, $$, $2);}
       ;

param : id { $$ = NODE(NodeType::PARAM, new_empty_node(ast), $1, new_empty_node(ast)); }
      | "(" mode id opt_type ")" { $$ = NODE(NodeType::PARAM, $2, $3, $4); }
      ;

mode : VAR { $$ = new_mode_node(ast, @$, ModeLiteral::VAR); }
     | VAL { $$ = new_mode_node(ast, @$, ModeLiteral::VAL); }
     | OUT { $$ = new_mode_node(ast, @$, ModeLiteral::OUT); }
		 ;

/* Function application */
app : func "(" args ")" { $$ = NODE(NodeType::APP, $1, $3); };

/* TODO: allow application of function expressions to arguments */
func : id ;

args
  : arg {
    $$ = new_list_node(ast, NodeType::METALIST);
    $$ = list_append_node(ast, $$, $1);
  }
  | args "," arg { $$ = list_append_node(ast, $$, $3); }
  ;

arg : term ;

/* Assignment */
ass : path "=" nls exp { $$ = NODE(NodeType::ASS, $1, $exp); } ;

at: path "[" exp "]" { $$ = NODE(NodeType::AT, $1, $3); } ;

dot : path "." id { $$ = NODE(NodeType::FIELD_ACCESS, $1, $3); } ;

path
  : id { $$ = NODE(NodeType::PATH, $1); }
  | at { $$ = NODE(NodeType::PATH, $1); }
  | dot { $$ = NODE(NodeType::PATH, $1); }
  ;

/* Operators. Infix and prefix */
op : op0;

op0  : opa  | opa[lft] AS type_expression[rgt] { $$ = NODE(NodeType::AS, $lft, $rgt); } ;
opa  : op1  | opa[lft] ".." nls op1[rgt]  { $$ = NODE(NodeType::REPEAT, $lft, $rgt); } ;
op1  : op2  | op1[lft] OR   nls op2[rgt]  { $$ = NODE(NodeType::OR,  $lft, $rgt); } ;
op2  : op3  | op2[lft] AND  nls op3[rgt]  { $$ = NODE(NodeType::AND, $lft, $rgt); } ;
op3  : op4  | op3[lft] ">"  nls op4[rgt]  { $$ = NODE(NodeType::GTN, $lft, $rgt); } ;
op4  : op5  | op4[lft] "<"  nls op5[rgt]  { $$ = NODE(NodeType::LTN, $lft, $rgt); } ;
op5  : op6  | op5[lft] ">=" nls op6[rgt]  { $$ = NODE(NodeType::GTE, $lft, $rgt); } ;
op6  : op7  | op6[lft] "<=" nls op7[rgt]  { $$ = NODE(NodeType::LTE, $lft, $rgt); } ;
op7  : op9  | op7[lft] "==" nls op9[rgt]  { $$ = NODE(NodeType::EQ,  $lft, $rgt); } ;
op9  : op10 | op9[lft] "+" nls op10[rgt] { $$ = NODE(NodeType::ADD, $lft, $rgt); } ;
op10 : op11 | op10[lft] "-" nls op11[rgt] { $$ = NODE(NodeType::SUB, $lft, $rgt); } ;
op11 : op12 | op11[lft] "*" nls op12[rgt] { $$ = NODE(NodeType::MUL, $lft, $rgt); } ;
op12 : op13 | op12[lft] "/" nls op13[rgt] { $$ = NODE(NodeType::DIV, $lft, $rgt); } ;
op13 : op14 | op13[lft] "%" nls op14[rgt] { $$ = NODE(NodeType::MOD, $lft, $rgt); } ;
op14 : op15 | NOT op15[rgt]      { $$ = NODE(NodeType::NOT, $rgt); } ;
op15 : op16 | "#" op16[rgt] { $$ = NODE(NodeType::LEN, $rgt); } ;
op16 : term ;

/* Terms */
term : "(" nls exp nls ")" { $$ = $exp; }
     | path
     | int
     | STRING      { $$ = new_string_node(ast, NodeType::STR, @$, pool, $1); }
     | NIL         { $$ = new_nil_node(ast, @$); }
     | FALSE       { $$ = new_false_node(ast, @$); }
     | TRUE        { $$ = new_true_node(ast, @$); }
     | CHAR        { $$ = new_char_node(ast, @$, $1); }
     | record_literal
     | array_literal
     ;

record_literal
  : "{" field_list opt_field_sep "}" { $$ = $2; }
  ;

field_list
  : field {
    $$ = new_list_node(ast, NodeType::RECORD_LITERAL);
    $$ = list_append_node(ast, $$, $1);
  }
  | field_list[prefix] field_sep field[postfix] {
    $$ = list_append_node(ast, $prefix, $postfix);
  }
  ;

field
  : id "=" exp {
    $$ = NODE(NodeType::FIELD, $1, $3);
  }
  ;

array_literal
  : "[" element_list opt_field_sep "]" { $$ = $2; }
  ;

element_list
  : term {
    $$ = new_list_node(ast, NodeType::ARRAY_LITERAL);
    $$ = list_append_node(ast, $$, $1);
  }
  | element_list[prefix] field_sep term[postfix] {
    $$ = list_append_node(ast, $prefix, $postfix);
  }
  ;

field_sep : ";" ;

opt_field_sep : %empty | field_sep ;

id : ID { $$ = new_string_node(ast, NodeType::ID, @$, pool, $1); } ;

int : NUMBER { $$ = new_number_node(ast, @$, $1); } ;

type_declaration
	: TYPE id "=" type_expression {
		$$ = NODE(NodeType::TYPE_DECL, $2, $4);
	}
	;

type_expression
	: type_instantiation
	| record_type_literal
	| array_type_literal
	| id
	;

type_instantiation
	: id "<" type_argument_list ">"
	{ $$ = NODE(NodeType::INSTANCE, $1, $3); }
	;

type_argument_list
  : type_argument {
    $$ = new_list_node(ast, NodeType::METALIST);
    $$ = list_append_node(ast, $$, $1);
  }
  | type_argument_list[prefix] "," type_argument[postfix]
  { $$ = list_append_node(ast, $prefix, $postfix); }
  ;

type_argument
  : type_expression
  | int
  ;

record_type_literal
  : "{" field_type_list opt_field_sep "}" { $$ = $2; }
  ;

field_type_list
  : field_type {
    $$ = new_list_node(ast, NodeType::RECORD_TYPE_LITERAL);
    $$ = list_append_node(ast, $$, $1);
  }
  | field_type_list[prefix] field_sep field_type[postfix] {
    $$ = list_append_node(ast, $prefix, $postfix);
  }
  ;

field_type
  : id ":" type_expression {
    $$ = NODE(NodeType::FIELD_TYPE, $1, $3);
  }
  ;

array_type_literal
  : "[" type_expression "]" { $$ = NODE(NodeType::ARRAY_TYPE_LITERAL, $2); }
  ;

%%

void yy::parser::error(const location_type& loc, const std::string& msg) {
	fprintf(
		stderr,
		"ERROR from byte %d (%d, %d) to byte %d (%d, %d): %s",
		loc.begin.byte_offset,
		loc.begin.line + 1,
		loc.begin.column + 1,
		loc.end.byte_offset,
		loc.end.line + 1,
		loc.end.column + 1,
		msg.c_str()
	);
}

// TODO: not implemented
std::ostream& operator<<(std::ostream& st, Location) { return st; }

AST parse(Reader* reader, StringPool& pool, bool is_repl) {
	Lexer lexer {reader, is_repl};
	AST ast {};
	ast.file_name = lexer.file->get_path();
	yy::parser parser {&lexer, &ast, pool};
	if (parser.parse() != 0) throw std::runtime_error("Failed to parse");
	ast.lines = lexer.get_lines();
	return ast;
}
