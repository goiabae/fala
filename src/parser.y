%define api.pure full
%define parse.trace
%define parse.error verbose

%lex-param {void *scanner}
%parse-param {void *scanner}{AST* ast}{SymbolTable* syms}

/* necessary for node functions */
%code requires {
  #include "ast.h"
}

%{
#include <stdio.h>

#include "parser.h"
#include "lexer.h"

#define yyerror(SCAN, AST, SYMS, ...) fprintf (stderr, __VA_ARGS__)
%}

/* type of terminal values */
%union {
  int num;
	char* str;
	Node node;
}

/* terminal values */
%token <num> NUMBER
%token <str> ID
%token <str> STRING

/* keywords and constants */
%token DO END IF THEN ELSE WHEN FOR FROM TO WHILE VAR LET IN
%token NIL TRUE

/* pontuation */
%token PAREN_OPEN "(" PAREN_CLOSE ")"
%token BRACKET_OPEN "[" BRACKET_CLOSE "]"
%token SEMICOL ";"
%token COMMA ","

/* binary operators */
%token EQ "="
%token OR AND
%token GREATER_EQ ">=" LESSER_EQ "<=" EQ_EQ "==" GREATER ">" LESSER "<"
%token PLUS "+" MINUS "-"
%token ASTER "*" SLASH "/" PERCT "%"
%token NOT

%left OR AND
%left GREATER_EQ LESSER_EQ EQ_EQ GREATER LESSER
%left PLUS MINUS
%left ASTER SLASH PERCT
%nonassoc NOT

%type <node> exp exps infix term id decl decls var args func

%%

%start program ;

program : %empty { ast->root = new_number_node(0); }
        | exp    { ast->root = $1; }
        ;

exps : exp ";"      { $$ = new_list_node(); $$ = list_append_node($$, $1); }
     | exps exp ";" { $$ = list_append_node($$, $2); }
     ;

exp : DO exps END                          { $$ = $2; }
    | IF exp THEN exp ELSE exp             { $$ = new_node(AST_IF,    3, (Node[3]){$2, $4, $6}); }
    | WHEN exp THEN exp                    { $$ = new_node(AST_WHEN,  2, (Node[2]){$2, $4}); }
    | FOR VAR var FROM exp TO exp THEN exp { $$ = new_node(AST_FOR,   4, (Node[4]){$3, $5, $7, $9}); }
    | WHILE exp THEN exp                   { $$ = new_node(AST_WHILE, 2, (Node[2]){$2, $4}); }
    | LET decls IN exp                     { $$ = new_node(AST_LET,   2, (Node[2]){$2, $4}); }
    | decl
    | var "=" exp                          { $$ = new_node(AST_ASS,   2, (Node[2]){$1, $3}); }
    | infix
    | func args                            { $$ = new_node(AST_APP,   2, (Node[2]){$1, $2}); }
    ;

func : id | "(" exp ")" { $$ = $2; } ;

args : term      { $$ = new_list_node(); $$ = list_append_node($$, $1); }
     | args term { $$ = list_append_node($$, $2); }
     ;

decl : VAR var         { $$ = new_node(AST_DECL, 1, (Node[1]) {$2}); }
     | VAR var "=" exp { $$ = new_node(AST_DECL, 2, (Node[2]) {$2, $4}); }
     ;

decls : decl           { $$ = new_list_node(); $$ = list_append_node($$, $1); }
      | decls "," decl { $$ = list_append_node($$, $3); }
      ;

var : id             { $$ = new_node(AST_VAR, 1, (Node[1]){$1}); }
    | id "[" exp "]" { $$ = new_node(AST_VAR, 2, (Node[2]){$1, $3}); }
    ;

id : ID { $$ = new_string_node(AST_ID, syms, $1); }

infix : term
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

term : "(" exp ")" { $$ = $2; }
     | NUMBER      { $$ = new_number_node($1); };
     | var
     | STRING      { $$ = new_string_node(AST_STR, syms, $1); }
     | NIL         { $$ = new_nil_node(); }
     | TRUE        { $$ = new_true_node(); }
     ;
