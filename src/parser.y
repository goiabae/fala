%parse-param {Context* ctx}

%define parse.trace

%code requires {
  #include "main.h"
}

%{
#include <stdio.h>

#include "lexer.h"
#include "parser.h"

int yylex(void);
%}

%union {
  int num;
	char* str;
	Node node;
}

%token <num> NUMBER
%token <str> ID

%token OR "or"
%token AND "and"

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

%%

%start program ;

program : %empty
        | exps { ctx->ast.root = $1; };

exps : exp
     | exp exps { $$ = new_node(FALA_THEN, NULL, 2, (Node[2]){$1, $2}); }
     ;

exp : infix ;

infix : ass-exp ;

/* a = b. assignment lol */
ass-exp : logi-exp
        | id EQ exp { $$ = new_node(FALA_ASS, NULL, 2, (Node[2]){$1, $3}); }
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

term : '(' exp ')' { $$ = $2; }
     | number
     | id
     ;

number : NUMBER {
  int* tmp = malloc(sizeof(int));
	*tmp = $1;
  $$ = new_node(FALA_NUM, (void*)tmp, 0, NULL);
};

id : ID { $$ = new_node(FALA_ID, (void*)$1, 0, NULL); }
