%define api.pure full

%lex-param {void *scanner}
%parse-param {void *scanner}{Context* ctx}

%define parse.trace

%code requires {
  #include "main.h"
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
%token IN "in"
%token OUT "out"
%token WHEN "when"
%token FOR "for"
%token FROM "from"
%token TO "to"
%token WHILE "while"
%token VAR "var"

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
%type <node> number
%type <node> id
%type <node> string
%type <node> decl
%type <node> var

%%

%start program ;

program : exp { ctx->ast.root = $1; };

exps : %empty   { $$ = new_block_node(); }
     | exps exp { (void)$1; $$ = block_append_node($$, $2); }
     ;

exp : DO exps END                { $$ = $2; }
    | IF exp THEN exp ELSE exp   { $$ = new_node(FALA_IF, NULL, 3, (Node[3]) {$2, $4, $6}); }
    | WHEN exp exp               { $$ = new_node(FALA_WHEN, NULL, 2, (Node[2]) {$2, $3}); }
    | FOR VAR var FROM exp TO exp exp { $$ = new_node(FALA_FOR, NULL, 4, (Node[4]){$3, $5, $7, $8}); }
    | WHILE exp exp              { $$ = new_node(FALA_WHILE, NULL, 2, (Node[2]){$2, $3}); }
    | IN                         { $$ = new_node(FALA_IN, NULL, 0, NULL); }
    | OUT exp                    { $$ = new_node(FALA_OUT, NULL, 1, (Node[1]){$2}); }
    | decl
    | infix
    ;

decl : VAR var { $$ = new_node(FALA_DECL, NULL, 1, (Node[1]) {$2}); }
     | VAR var EQ exp { $$ = new_node(FALA_DECL, NULL, 2, (Node[2]) {$2, $4}); }
     ;

var : id                                { $$ = new_node(FALA_VAR, NULL, 1, (Node[1]){$1}); }
    | id BRACKET_OPEN exp BRACKET_CLOSE { $$ = new_node(FALA_VAR, NULL, 2, (Node[2]){$1, $3}); }
    ;

infix : ass-exp ;

/* a = b. assignment lol */
ass-exp : logi-exp
        | var EQ exp { $$ = new_node(FALA_ASS, NULL, 2, (Node[2]){$1, $3}); }
        ;

/* a and b or c */
logi-exp : rela-exp
         | logi-exp OR  rela-exp { $$ = new_node(FALA_OR,  NULL, 2, (Node[2]){$1, $3}); }
         | logi-exp AND rela-exp { $$ = new_node(FALA_AND, NULL, 2, (Node[2]){$1, $3}); }
         ;

/* a == b */
rela-exp : sum-exp
         | rela-exp GREATER    sum-exp { $$ = new_node(FALA_GREATER,    NULL, 2, (Node[2]){$1, $3}); }
         | rela-exp LESSER     sum-exp { $$ = new_node(FALA_LESSER,     NULL, 2, (Node[2]){$1, $3}); }
         | rela-exp GREATER_EQ sum-exp { $$ = new_node(FALA_GREATER_EQ, NULL, 2, (Node[2]){$1, $3}); }
         | rela-exp LESSER_EQ  sum-exp { $$ = new_node(FALA_LESSER_EQ,  NULL, 2, (Node[2]){$1, $3}); }
         | rela-exp EQ_EQ      sum-exp { $$ = new_node(FALA_EQ_EQ,      NULL, 2, (Node[2]){$1, $3}); }
         ;

/* a + b */
sum-exp : mul-exp
        | sum-exp PLUS  mul-exp { $$ = new_node(FALA_ADD, NULL, 2, (Node[2]){$1, $3}); }
        | sum-exp MINUS mul-exp { $$ = new_node(FALA_SUB, NULL, 2, (Node[2]){$1, $3}); }
        ;

/* a * b */
mul-exp : not-exp
        | mul-exp ASTER not-exp { $$ = new_node(FALA_MUL, NULL, 2, (Node[2]){$1, $3}); }
        | mul-exp SLASH not-exp { $$ = new_node(FALA_DIV, NULL, 2, (Node[2]){$1, $3}); }
        | mul-exp PERCT not-exp { $$ = new_node(FALA_MOD, NULL, 2, (Node[2]){$1, $3}); }
        ;

/* not a */
not-exp : term
        | NOT term { $$ = new_node(FALA_NOT, NULL, 1, (Node[1]){$2}); }
        ;

term : PAREN_OPEN exp PAREN_CLOSE { $$ = $2; }
     | number
     | var
     | string
     ;

number : NUMBER {
  int* tmp = malloc(sizeof(int));
  *tmp = $1;
  $$ = new_node(FALA_NUM, (void*)tmp, 0, NULL);
};

id : ID { $$ = new_node(FALA_ID, (void*)$1, 0, NULL); }

string : STRING { $$ = new_node(FALA_STRING, (void*)$1, 0, NULL); }
