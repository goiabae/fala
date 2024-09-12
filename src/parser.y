%require "3.0.5"
%language "c"

%define api.pure full
%define parse.trace
%define parse.error verbose

%locations

%code requires {
#define YYLTYPE Location
}

%lex-param {LEXER lexer}
%parse-param {LEXER lexer}{AST* ast}{STR_POOL pool}

/* necessary for node functions */
%code requires {
#include "str_pool.h"
#include "ast.h"
#include "lexer.h"

#define yylex lexer_lex
}

%{
#include <stdio.h>
#include <stdbool.h>

#include "parser.h"

void error_report(FILE* fd, Location* yyloc, const char* msg);
#define yyerror(LOC, LEX, AST, POOL, MSG) error_report(stderr, LOC, MSG)
#define NODE(TYPE, ...) \
  new_node( \
    ast, \
    TYPE, \
    sizeof((NodeIndex[]){__VA_ARGS__}) / sizeof(NodeIndex), \
    (NodeIndex[]) {__VA_ARGS__})
%}

/* type of symbols ($N and $$) in grammar actions */
%union {
	int num;
	char* str;
	char character;
	NodeIndex node;
}

%token YYEOF 0 /* old versions of bison don't predefine this */

/* values */
%token <num> NUMBER
%token <str> ID
%token <str> STRING
%token <character> CHAR

/* keywords and constants */
%token DO END IF THEN ELSE WHEN FOR FROM TO STEP WHILE BREAK CONTINUE VAR LET IN FUN BOOL INT UINT AS
%token NIL TRUE

/* pontuation */
%token PAREN_OPEN "(" PAREN_CLOSE ")"
%token BRACKET_OPEN "[" BRACKET_CLOSE "]"
%token SEMICOL ";" COLON ":"
%token COMMA ","
%token DOT "."

/* binary operators */
%token EQ "="
%token OR AND
%token GREATER_EQ ">=" LESSER_EQ "<=" EQ_EQ "==" GREATER ">" LESSER "<"
%token PLUS "+" MINUS "-"
%token ASTER "*" SLASH "/" PERCT "%"
%token NOT

/* non-terminals */
%type <node> exp

%type <node> do block
%type <node> cond
%type <node> loop step
%type <node> jump
%type <node> let decls
%type <node> decl params opt-type
%type <node> app func args arg
%type <node> ass path
%type <node> op op0 op1 op2 op3 op4 op5 op6 op7 op9 op10 op11 op12 op13 op14 op15

%type <node> term id int
%type <node> type-literal type-primitive

%%

%start program ;

program : %empty { if (is_interactive(lexer)) YYACCEPT; }
        | exp    { ast_set_root(ast, $1); if (is_interactive(lexer)) YYACCEPT; }
        ;

exp : do
    | cond
    | loop
    | jump
    | let
    | decl
    | app
    | ass
    | op
    ;

/* Expression sequences */
do : DO block END { $$ = $2; }

block : exp           { $$ = new_list_node(ast);
                        $$ = list_append_node(ast, $$, $1); }
      | block ";" exp { $$ = list_append_node(ast, $$, $3); }
      | block ";"
      ;

/* Conditionals */
cond : IF exp THEN exp ELSE exp { $$ = NODE(AST_IF, $2, $4, $6); }
     | WHEN exp THEN exp        { $$ = NODE(AST_WHEN, $2, $4); }
     ;

/* Loops */
loop : WHILE exp THEN exp            { $$ = NODE(AST_WHILE, $2, $4); }
     | FOR decl TO exp step THEN exp { $$ = NODE(AST_FOR, $2, $4, $5, $7); }
     ;

step : %empty   { $$ = new_empty_node(ast); }
     | STEP exp { $$ = $2; }
     ;

/* Jumps */
jump : BREAK exp    { $$ = NODE(AST_BREAK, $2); }
     | CONTINUE exp { $$ = NODE(AST_CONTINUE, $2); }
     ;

/* Let bindings */
let : LET decls     IN exp { $$ = NODE(AST_LET, $2, $4); }
    | LET decls "," IN exp { $$ = NODE(AST_LET, $2, $5); }
    ;

decls : decl           { $$ = new_list_node(ast); $$ = list_append_node(ast, $$, $1); }
      | decls "," decl { $$ = list_append_node(ast, $$, $3); }
      ;

/* Declarations */
decl : VAR id opt-type "=" exp        { $$ = NODE(AST_DECL, $2, $3, $5); }
     | FUN id params opt-type "=" exp { $$ = NODE(AST_DECL, $2, $3, $4, $6); }
     ;

opt-type : %empty { $$ = new_empty_node(ast); }
         | ":" type-literal { $$ = $2; }
         ;

params : %empty    { $$ = new_list_node(ast); }
       | params id { $$ = list_append_node(ast, $$, $2);}
       ;

/* Function application */
app : func arg args { $$ = NODE(AST_APP, $1, list_prepend_node(ast, $3, $2)); }
    | arg "." func "(" args ")" { $$ = NODE(AST_APP, $3, list_prepend_node(ast, $5, $1)); }
    | app "." func "(" args ")" { $$ = NODE(AST_APP, $3, list_prepend_node(ast, $5, $1)); }
    ;

/* TODO: allow application of function expressions to arguments */
func : id ;

args : %empty   { $$ = new_list_node(ast); }
     | args arg { $$ = list_append_node(ast, $$, $2); }
     ;

arg : term ;

/* Assignment */
ass : path "=" exp { $$ = NODE(AST_ASS, $1, $3); } ;

path : id | id "[" exp "]" { $$ = NODE(AST_AT, $1, $3); }

/* Operators. Infix and prefix */
op : op0;

op0  : op1  | op1 AS type-literal { $$ = NODE(AST_AS, $1, $3); }
op1  : op2  | op1 OR   op2  { $$ = NODE(AST_OR,  $1, $3); }
op2  : op3  | op2 AND  op3  { $$ = NODE(AST_AND, $1, $3); }
op3  : op4  | op3 ">"  op4  { $$ = NODE(AST_GTN, $1, $3); }
op4  : op5  | op4 "<"  op5  { $$ = NODE(AST_LTN, $1, $3); }
op5  : op6  | op5 ">=" op6  { $$ = NODE(AST_GTE, $1, $3); }
op6  : op7  | op6 "<=" op7  { $$ = NODE(AST_LTE, $1, $3); }
op7  : op9  | op7 "==" op9  { $$ = NODE(AST_EQ,  $1, $3); }
op9  : op10 | op9  "+" op10 { $$ = NODE(AST_ADD, $1, $3); }
op10 : op11 | op10 "-" op11 { $$ = NODE(AST_SUB, $1, $3); }
op11 : op12 | op11 "*" op12 { $$ = NODE(AST_MUL, $1, $3); }
op12 : op13 | op12 "/" op13 { $$ = NODE(AST_DIV, $1, $3); }
op13 : op14 | op13 "%" op14 { $$ = NODE(AST_MOD, $1, $3); }
op14 : op15 | NOT op15      { $$ = NODE(AST_NOT, $2); }
op15 : term ;

/* Terms */
term : "(" exp ")" { $$ = $2; }
     | path      { $$ = NODE(AST_PATH, $1); }
     | int
     | STRING      { $$ = new_string_node(ast, AST_STR, yyloc, pool, $1); }
     | NIL         { $$ = new_nil_node(ast, yyloc); }
     | TRUE        { $$ = new_true_node(ast, yyloc); }
     | CHAR        { $$ = new_char_node(ast, yyloc, $1); }
     ;

id : ID { $$ = new_string_node(ast, AST_ID, yyloc, pool, $1); }

int : NUMBER { $$ = new_number_node(ast, yyloc, $1); }

type-literal : type-primitive ;

type-primitive :
  INT int    { $$ = NODE(AST_PRIMITIVE_TYPE, new_number_node(ast, yyloc, 0), $2); }
  | UINT int { $$ = NODE(AST_PRIMITIVE_TYPE, new_number_node(ast, yyloc, 1), $2); }
  | BOOL     { $$ = NODE(AST_PRIMITIVE_TYPE, new_number_node(ast, yyloc, 2)); }
  | NIL      { $$ = NODE(AST_PRIMITIVE_TYPE, new_number_node(ast, yyloc, 3)); }
  ;


%%
void error_report(FILE* fd, Location* yyloc, const char* msg) {
	fprintf(
		fd,
		"ERROR from (%d, %d) to (%d, %d): %s",
		yyloc->first_line + 1,
		yyloc->first_column + 1,
		yyloc->last_line + 1,
		yyloc->last_column + 1,
		msg
	);
}
