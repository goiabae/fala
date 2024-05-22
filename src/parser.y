%require "3.0.5"
%language "c"

%define api.pure full
%define parse.trace
%define parse.error verbose

%locations

%code requires {
#define YYLTYPE Location
}

%lex-param {void *scanner}
%parse-param {void *scanner}{AST* ast}{SymbolTable* syms}

/* necessary for node functions */
%code requires {
#include "ast.h"
#include "lexer.h"

#define yylex lexer_lex
}

%{
#include <stdio.h>
#include <stdbool.h>

#include "parser.h"

void error_report(FILE* fd, Location* yyloc, const char* msg);
#define yyerror(LOC, SCAN, AST, SYMS, MSG) error_report(stderr, LOC, MSG)
%}

/* type of $$ in grammar rules */
%union {
  int num;
	char* str;
	Node node;
}

%token YYEOF 0 /* old versions of bison don't predefine this */

/* values */
%token <num> NUMBER
%token <str> ID
%token <str> STRING

/* keywords and constants */
%token DO END IF THEN ELSE WHEN FOR FROM TO STEP WHILE BREAK CONTINUE VAR LET IN FUN
%token NIL TRUE

/* pontuation */
%token PAREN_OPEN "(" PAREN_CLOSE ")"
%token BRACKET_OPEN "[" BRACKET_CLOSE "]"
%token SEMICOL ";"
%token COMMA ","
%token DOT "."

/* binary operators */
%token EQ "="
%token OR AND
%token GREATER_EQ ">=" LESSER_EQ "<=" EQ_EQ "==" GREATER ">" LESSER "<"
%token PLUS "+" MINUS "-"
%token ASTER "*" SLASH "/" PERCT "%"
%token NOT

%left DOT
%left OR AND
%left GREATER_EQ LESSER_EQ EQ_EQ GREATER LESSER
%left PLUS MINUS
%left ASTER SLASH PERCT
%nonassoc NOT

/* non-terminals */
%type <node> exp exps infix term id decl decls var more-args params

%%

%start program ;

program : %empty { ast->root = new_number_node(yyloc, 0); if (is_interactive(scanner)) YYACCEPT; }
        | exp    { ast->root = $1; if (is_interactive(scanner)) YYACCEPT; }
        ;

exps : exp ";"      { $$ = new_list_node(); $$ = list_append_node($$, $1); }
     | exps exp ";" { $$ = list_append_node($$, $2); }
     ;

exp : DO exps END                          { $$ = $2; }
    | IF exp THEN exp ELSE exp             { $$ = new_node(AST_IF,    3, (Node[3]){$2, $4, $6}); }
    | WHEN exp THEN exp                    { $$ = new_node(AST_WHEN,  2, (Node[2]){$2, $4}); }
    | FOR VAR var FROM exp TO exp THEN exp { $$ = new_node(AST_FOR,   4, (Node[4]){$3, $5, $7, $9}); }
    | FOR VAR var FROM exp TO exp STEP exp THEN exp { $$ = new_node(AST_FOR,   5, (Node[5]){$3, $5, $7, $9, $11}); }
    | WHILE exp THEN exp                   { $$ = new_node(AST_WHILE, 2, (Node[2]){$2, $4}); }
    | LET decls IN exp                     { $$ = new_node(AST_LET,   2, (Node[2]){$2, $4}); }
    | decl
    | var "=" exp                          { $$ = new_node(AST_ASS,   2, (Node[2]){$1, $3}); }
    | infix
    | BREAK exp     { $$ = new_node(AST_BREAK,    1, (Node[1]) {$2}); }
    | CONTINUE exp  { $$ = new_node(AST_CONTINUE, 1, (Node[1]) {$2}); }
    ;

decl : VAR var               { $$ = new_node(AST_DECL, 1, (Node[1]) {$2}); }
     | VAR var "=" exp       { $$ = new_node(AST_DECL, 2, (Node[2]) {$2, $4}); }
     | FUN id params "=" exp { $$ = new_node(AST_DECL, 3, (Node[3]) {$2, $3, $5}); }
     ;

params : %empty    { $$ = new_list_node(); }
       | params id { $$ = list_append_node($$, $2);}
       ;

decls : decl           { $$ = new_list_node(); $$ = list_append_node($$, $1); }
      | decls "," decl { $$ = list_append_node($$, $3); }
      ;

var : id             { $$ = new_node(AST_VAR, 1, (Node[1]){$1}); }
    | id "[" exp "]" { $$ = new_node(AST_VAR, 2, (Node[2]){$1, $3}); }
    ;

id : ID { $$ = new_string_node(AST_ID, yyloc, syms, $1); }

infix : term
      | id term more-args      { $$ = new_node(AST_APP, 2, (Node[2]){$1, list_prepend_node($3, $2)}); }
      | infix "." id more-args { $$ = new_node(AST_APP, 2, (Node[2]){$3, list_prepend_node($4, $1)}); }
      | infix OR   infix { $$ = new_node(AST_OR,  2, (Node[2]){$1, $3}); }
      | infix AND  infix { $$ = new_node(AST_AND, 2, (Node[2]){$1, $3}); }
      | infix ">"  infix { $$ = new_node(AST_GTN, 2, (Node[2]){$1, $3}); }
      | infix "<"  infix { $$ = new_node(AST_LTN, 2, (Node[2]){$1, $3}); }
      | infix ">=" infix { $$ = new_node(AST_GTE, 2, (Node[2]){$1, $3}); }
      | infix "<=" infix { $$ = new_node(AST_LTE, 2, (Node[2]){$1, $3}); }
      | infix "==" infix { $$ = new_node(AST_EQ,  2, (Node[2]){$1, $3}); }
      | infix "+"  infix { $$ = new_node(AST_ADD, 2, (Node[2]){$1, $3}); }
      | infix "-"  infix { $$ = new_node(AST_SUB, 2, (Node[2]){$1, $3}); }
      | infix "*"  infix { $$ = new_node(AST_MUL, 2, (Node[2]){$1, $3}); }
      | infix "/"  infix { $$ = new_node(AST_DIV, 2, (Node[2]){$1, $3}); }
      | infix "%"  infix { $$ = new_node(AST_MOD, 2, (Node[2]){$1, $3}); }
      | NOT infix        { $$ = new_node(AST_NOT, 1, (Node[1]){$2}); }
      ;

more-args : %empty         { $$ = new_list_node(); }
          | more-args term { $$ = list_append_node($$, $2); }
          ;

term : "(" exp ")" { $$ = $2; }
     | var
     | NUMBER      { $$ = new_number_node(yyloc, $1); };
     | STRING      { $$ = new_string_node(AST_STR, yyloc, syms, $1); }
     | NIL         { $$ = new_nil_node(yyloc); }
     | TRUE        { $$ = new_true_node(yyloc); }
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
