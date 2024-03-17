%define api.pure full

%lex-param {void *scanner}
%parse-param {void *scanner}{AST* ast}{SymbolTable* syms}

%define parse.trace

%code requires {
  #include "main.h"
  #include "ast.h"
}

%{
#include <stdio.h>

#include "parser.h"
#include "lexer.h"
%}

%union {
  int num;
	char* str;
	Node node;
}

%token <num> NUMBER
%token <str> ID
%token <str> STRING

%token OR "or"
%token AND "and"

%token DO "do"
%token END "end"
%token IF "if"
%token THEN "then"
%token ELSE "else"
%token WHEN "when"
%token FOR "for"
%token FROM "from"
%token TO "to"
%token WHILE "while"
%token VAR "var"

%token NIL "nil"
%token TRUE "true"

%token EQ    '='

%token PLUS  '+'
%token MINUS '-'
%token ASTER '*'
%token SLASH '/'
%token PERCT '%'

%token GREATER '>'
%token LESSER '<'
%token GREATER_EQ ">="
%token LESSER_EQ "<="
%token EQ_EQ "=="

%token NOT "not"

%token PAREN_OPEN '('
%token PAREN_CLOSE ')'
%token BRACKET_OPEN '['
%token BRACKET_CLOSE ']'

%token SEMICOL ';'

%type <node> exp
%type <node> exps
%type <node> infix
%type <node> ass-exp
%type <node> logi-exp
%type <node> rela-exp
%type <node> sum-exp
%type <node> mul-exp
%type <node> not-exp
%type <node> term
%type <node> id
%type <node> decl
%type <node> var
%type <node> args
%type <node> func

%%

%start program ;

program : %empty { ast->root = new_number_node(0); }
        | exp    { ast->root = $1; }
        ;

exps : exp              { $$ = new_list_node(); $$ = list_append_node($$, $1); }
     | exps SEMICOL exp { $$ = list_append_node($$, $3); }
     ;

exp : DO exps END                          { $$ = $2; }
    | IF exp THEN exp ELSE exp             { $$ = new_node(FALA_IF,    3, (Node[3]) {$2, $4, $6}); }
    | WHEN exp THEN exp                    { $$ = new_node(FALA_WHEN,  2, (Node[2]) {$2, $4}); }
    | FOR VAR var FROM exp TO exp THEN exp { $$ = new_node(FALA_FOR,   4, (Node[4]){$3, $5, $7, $9}); }
    | WHILE exp THEN exp                   { $$ = new_node(FALA_WHILE, 2, (Node[2]){$2, $4}); }
    | decl
    | infix
    | func args                            { $$ = new_node(FALA_APP,   2, (Node[2]){$1, $2}); }
    ;

func : id | PAREN_OPEN exp PAREN_CLOSE { $$ = $2; } ;

args : term   { $$ = new_list_node(); $$ = list_append_node($$, $1); }
     | args term { $$ = list_append_node($$, $2); }
     ;

decl : VAR var { $$ = new_node(FALA_DECL, 1, (Node[1]) {$2}); }
     | VAR var EQ exp { $$ = new_node(FALA_DECL, 2, (Node[2]) {$2, $4}); }
     ;

var : id                                { $$ = new_node(FALA_VAR, 1, (Node[1]){$1}); }
    | id BRACKET_OPEN exp BRACKET_CLOSE { $$ = new_node(FALA_VAR, 2, (Node[2]){$1, $3}); }
    ;

infix : ass-exp ;

/* a = b. assignment lol */
ass-exp : logi-exp
        | var EQ exp { $$ = new_node(FALA_ASS, 2, (Node[2]){$1, $3}); }
        ;

/* a and b or c */
logi-exp : rela-exp
         | logi-exp OR  rela-exp { $$ = new_node(FALA_OR,  2, (Node[2]){$1, $3}); }
         | logi-exp AND rela-exp { $$ = new_node(FALA_AND, 2, (Node[2]){$1, $3}); }
         ;

/* a == b */
rela-exp : sum-exp
         | rela-exp GREATER    sum-exp { $$ = new_node(FALA_GREATER,    2, (Node[2]){$1, $3}); }
         | rela-exp LESSER     sum-exp { $$ = new_node(FALA_LESSER,     2, (Node[2]){$1, $3}); }
         | rela-exp GREATER_EQ sum-exp { $$ = new_node(FALA_GREATER_EQ, 2, (Node[2]){$1, $3}); }
         | rela-exp LESSER_EQ  sum-exp { $$ = new_node(FALA_LESSER_EQ,  2, (Node[2]){$1, $3}); }
         | rela-exp EQ_EQ      sum-exp { $$ = new_node(FALA_EQ,      2, (Node[2]){$1, $3}); }
         ;

/* a + b */
sum-exp : mul-exp
        | sum-exp PLUS  mul-exp { $$ = new_node(FALA_ADD, 2, (Node[2]){$1, $3}); }
        | sum-exp MINUS mul-exp { $$ = new_node(FALA_SUB, 2, (Node[2]){$1, $3}); }
        ;

/* a * b */
mul-exp : not-exp
        | mul-exp ASTER not-exp { $$ = new_node(FALA_MUL, 2, (Node[2]){$1, $3}); }
        | mul-exp SLASH not-exp { $$ = new_node(FALA_DIV, 2, (Node[2]){$1, $3}); }
        | mul-exp PERCT not-exp { $$ = new_node(FALA_MOD, 2, (Node[2]){$1, $3}); }
        ;

/* not a */
not-exp : term
        | NOT term { $$ = new_node(FALA_NOT, 1, (Node[1]){$2}); }
        ;

term : PAREN_OPEN exp PAREN_CLOSE { $$ = $2; }
     | NUMBER                     { $$ = new_number_node($1); };
     | var
     | STRING                     { $$ = new_string_node(FALA_STRING, syms, $1); }
     | NIL                        { $$ = new_nil_node(); }
     | TRUE                       { $$ = new_true_node(); }
     ;

id : ID { $$ = new_string_node(FALA_ID, syms, $1); }
