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
%token PAREN_OPEN PAREN_CLOSE
%token BRACKET_OPEN BRACKET_CLOSE
%token SEMICOL
%token COMMA

/* binary operators */
%token EQ
%token OR AND
%token GREATER_EQ LESSER_EQ EQ_EQ GREATER LESSER
%token PLUS MINUS
%token ASTER SLASH PERCT
%token  NOT

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

exps : exp SEMICOL      { $$ = new_list_node(); $$ = list_append_node($$, $1); }
     | exps exp SEMICOL { $$ = list_append_node($$, $2); }
     ;

exp : DO exps END                          { $$ = $2; }
    | IF exp THEN exp ELSE exp             { $$ = new_node(FALA_IF,    3, (Node[3]){$2, $4, $6}); }
    | WHEN exp THEN exp                    { $$ = new_node(FALA_WHEN,  2, (Node[2]){$2, $4}); }
    | FOR VAR var FROM exp TO exp THEN exp { $$ = new_node(FALA_FOR,   4, (Node[4]){$3, $5, $7, $9}); }
    | WHILE exp THEN exp                   { $$ = new_node(FALA_WHILE, 2, (Node[2]){$2, $4}); }
    | LET decls IN exp                     { $$ = new_node(FALA_LET,   2, (Node[2]){$2, $4}); }
    | decl
    | var EQ exp                           { $$ = new_node(FALA_ASS,   2, (Node[2]){$1, $3}); }
    | infix
    | func args                            { $$ = new_node(FALA_APP,   2, (Node[2]){$1, $2}); }
    ;

func : id | PAREN_OPEN exp PAREN_CLOSE { $$ = $2; } ;

args : term      { $$ = new_list_node(); $$ = list_append_node($$, $1); }
     | args term { $$ = list_append_node($$, $2); }
     ;

decl : VAR var        { $$ = new_node(FALA_DECL, 1, (Node[1]) {$2}); }
     | VAR var EQ exp { $$ = new_node(FALA_DECL, 2, (Node[2]) {$2, $4}); }
     ;

decls : decl             { $$ = new_list_node(); $$ = list_append_node($$, $1); }
      | decls COMMA decl { $$ = list_append_node($$, $3); }
      ;

var : id                                { $$ = new_node(FALA_VAR, 1, (Node[1]){$1}); }
    | id BRACKET_OPEN exp BRACKET_CLOSE { $$ = new_node(FALA_VAR, 2, (Node[2]){$1, $3}); }
    ;

id : ID { $$ = new_string_node(FALA_ID, syms, $1); }

infix : term
      | infix OR         infix { $$ = new_node(FALA_OR,         2, (Node[2]){$1, $3}); }
      | infix AND        infix { $$ = new_node(FALA_AND,        2, (Node[2]){$1, $3}); }
      | infix GREATER    infix { $$ = new_node(FALA_GREATER,    2, (Node[2]){$1, $3}); }
      | infix LESSER     infix { $$ = new_node(FALA_LESSER,     2, (Node[2]){$1, $3}); }
      | infix GREATER_EQ infix { $$ = new_node(FALA_GREATER_EQ, 2, (Node[2]){$1, $3}); }
      | infix LESSER_EQ  infix { $$ = new_node(FALA_LESSER_EQ,  2, (Node[2]){$1, $3}); }
      | infix EQ_EQ      infix { $$ = new_node(FALA_EQ,         2, (Node[2]){$1, $3}); }
      | infix PLUS       infix { $$ = new_node(FALA_ADD,        2, (Node[2]){$1, $3}); }
      | infix MINUS      infix { $$ = new_node(FALA_SUB,        2, (Node[2]){$1, $3}); }
      | infix ASTER      infix { $$ = new_node(FALA_MUL,        2, (Node[2]){$1, $3}); }
      | infix SLASH      infix { $$ = new_node(FALA_DIV,        2, (Node[2]){$1, $3}); }
      | infix PERCT      infix { $$ = new_node(FALA_MOD,        2, (Node[2]){$1, $3}); }
      | NOT infix              { $$ = new_node(FALA_NOT,        1, (Node[1]){$2}); }
      ;

term : PAREN_OPEN exp PAREN_CLOSE { $$ = $2; }
     | NUMBER                     { $$ = new_number_node($1); };
     | var
     | STRING                     { $$ = new_string_node(FALA_STRING, syms, $1); }
     | NIL                        { $$ = new_nil_node(); }
     | TRUE                       { $$ = new_true_node(); }
     ;
