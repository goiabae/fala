%parse-param {void* var_table}

%{
#include <stdio.h>

#include "lexer.h"
#include "parser.h"
#include "main.h"

int yylex(void);
%}

%union {
	int num;
	const char* str;
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

%type <num> exp
%type <num> infix
%type <num> ass-exp
%type <num> logi-exp
%type <num> rela-exp
%type <num> sum-exp
%type <num> mul-exp
%type <num> not-exp
%type <num> term
%type <num> number

%%

%start program ;

program : exp { printf("%d\n", $1); } ;

exp : infix ;

infix : ass-exp ;

/* a = b. assignment lol */
ass-exp : logi-exp
        | ID EQ exp { $$ = 0; /* TODO */ yyerror(var_table, "SEMANT_ERR: Not implemented yet"); }
        ;

/* a and b or c */
logi-exp : rela-exp
         | logi-exp OR  rela-exp { $$ = $1 || $3; }
         | logi-exp AND rela-exp { $$ = $1 && $3; }
         ;

/* a == b */
rela-exp : sum-exp
         | rela-exp GREATER    sum-exp { $$ = $1 >  $3; }
         | rela-exp LESSER     sum-exp { $$ = $1 <  $3; }
         | rela-exp GREATER_EQ sum-exp { $$ = $1 >= $3; }
         | rela-exp LESSER_EQ  sum-exp { $$ = $1 <= $3; }
         | rela-exp EQ_EQ      sum-exp { $$ = $1 == $3; }
         ;

/* a + b */
sum-exp : mul-exp
        | sum-exp PLUS  mul-exp { $$ = $1 + $3; }
        | sum-exp MINUS mul-exp { $$ = $1 - $3; }
        ;

/* a * b */
mul-exp : not-exp
        | mul-exp ASTER not-exp { $$ = $1 * $3; }
        | mul-exp SLASH not-exp { $$ = $1 / $3; }
        | mul-exp PERCT not-exp { $$ = $1 % $3; }
        ;

/* not a */
not-exp : term
        | NOT term { $$ = ! $2; }
        ;

term : '(' exp ')' { $$ = $2; }
     | number      { $$ = $1; }
     | ID          { $$ = 0; /* TODO */ yyerror(var_table, "SEMANT_ERR: Not implemented yet"); }
     ;

number : NUMBER ;
