%{
  #include <string.h>

  #include "parser.h"
  #include "main.h"
%}

/* don't generate yyunput and input functions */
%option nounput noinput
%option noyywrap

%%

"or"    { return OR; }
"and"   { return AND; }
"not"   { return NOT; }

"do"    { return DO; }
"end"   { return END; }
"if"    { return IF; }
"then"  { return THEN; }
"else"  { return ELSE; }
"in"    { return IN; }
"out"   { return OUT; }
"when"  { return WHEN; }

[a-zA-Z_][a-zA-Z0-9_]* { yylval.str = strdup(yytext); return ID; }

[0-9]+  { yylval.num = atoi(yytext); return NUMBER; }

\".*\"  { yylval.str = strdup(yytext); return STRING; }

\(      { return PAREN_OPEN; }
\)      { return PAREN_CLOSE; }

\=\=    { return EQ_EQ; }

\>\=    { return GREATER_EQ; }
\<\=    { return LESSER_EQ; }

\=      { return EQ; }
\>      { return GREATER; }
\<      { return LESSER; }
\+      { return PLUS; }
\-      { return MINUS; }
\*      { return ASTER; }
\/      { return SLASH; }
\%      { return SLASH; }

[ \t\n] ;

.       { yyerror(NULL, "LEXICAL_ERR: Unrecognized token \"%s\""); }
