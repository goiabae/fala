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
%parse-param {Lexer* lexer}{AST* ast}{STR_POOL pool}

/* necessary for node functions */
%code requires {
#include "str_pool.h"
#include "ast.hpp"
#include "lexer.hpp"

#define yylex lexer_lex
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
%token DO END IF THEN ELSE WHEN FOR FROM TO STEP WHILE BREAK CONTINUE VAR LET IN FUN BOOL INT UINT AS
%token NIL TRUE FALSE

/* pontuation */
%token PAREN_OPEN "(" PAREN_CLOSE ")"
%token BRACKET_OPEN "[" BRACKET_CLOSE "]"
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
%token NOT

/* non-terminals */
%type <node> exp

%type <node> do block stmts
%type <node> cond
%type <node> loop step
%type <node> jump
%type <node> let decls
%type <node> decl params opt-type
%type <node> app func args arg
%type <node> ass path at
%type <node> op op0 op1 op2 op3 op4 op5 op6 op7 op9 op10 op11 op12 op13 op14 op15

%type <node> term id int
%type <node> type-literal type-primitive

%%

%start program ;

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

block : stmts exp opt-seps { $$ = list_append_node(ast, $1, $2); } ;

opt-seps : %empty | stmt-seps ;

stmts : %empty { $$ = new_list_node(ast); }
      | stmts exp stmt-seps { $$ = list_append_node(ast, $$, $2); }
      | stmts decl stmt-seps { $$ = list_append_node(ast, $$, $2); }
      ;

stmt-seps : stmt-sep | stmt-seps stmt-sep ;

stmt-sep : "\n" | ";" ;

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

decls : decl           { $$ = new_list_node(ast); $$ = list_append_node(ast, $$, $1); }
      | decls nls "," nls decl { $$ = list_append_node(ast, $$, $decl); }
      ;

/* Declarations */
decl : VAR id opt-type "=" nls exp        { $$ = NODE(NodeType::VAR_DECL, $2, $3, $exp); }
     | FUN id params opt-type "=" nls exp { $$ = NODE(NodeType::FUN_DECL, $2, $3, $4, $exp); }
     ;

opt-type : %empty { $$ = new_empty_node(ast); }
         | ":" type-literal { $$ = $2; }
         ;

params : %empty    { $$ = new_list_node(ast); }
       | params id { $$ = list_append_node(ast, $$, $2);}
       ;

/* Function application */
app : func arg args { $$ = NODE(NodeType::APP, $1, list_prepend_node(ast, $3, $2)); };

/* TODO: allow application of function expressions to arguments */
func : id ;

args : %empty   { $$ = new_list_node(ast); }
     | args arg { $$ = list_append_node(ast, $$, $2); }
     ;

arg : term ;

/* Assignment */
ass : path "=" nls exp { $$ = NODE(NodeType::ASS, $1, $exp); } ;

at: id "[" exp "]" { $$ = NODE(NodeType::AT, $1, $3); } ;

path
  : id { $$ = NODE(NodeType::PATH, $1); }
  | at { $$ = NODE(NodeType::PATH, $1); }
  ;

/* Operators. Infix and prefix */
op : op0;

op0  : op1  | op1[lft] AS type-literal[rgt] { $$ = NODE(NodeType::AS, $lft, $rgt); } ;
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
op15 : term ;

/* Terms */
term : "(" nls exp nls ")" { $$ = $exp; }
     | path
     | int
     | STRING      { $$ = new_string_node(ast, NodeType::STR, @$, pool, $1); }
     | NIL         { $$ = new_nil_node(ast, @$); }
     | FALSE       { $$ = new_false_node(ast, @$); }
     | TRUE        { $$ = new_true_node(ast, @$); }
     | CHAR        { $$ = new_char_node(ast, @$, $1); }
     ;

id : ID { $$ = new_string_node(ast, NodeType::ID, @$, pool, $1); } ;

int : NUMBER { $$ = new_number_node(ast, @$, $1); } ;

type-literal : type-primitive ;

type-primitive
  : INT int  { $$ = NODE(NodeType::INT_TYPE, $2); }
  | UINT int { $$ = NODE(NodeType::UINT_TYPE, $2); }
  | BOOL     { $$ = NODE(NodeType::BOOL_TYPE); }
  | NIL      { $$ = NODE(NodeType::NIL_TYPE); }
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
