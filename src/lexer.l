%{
  #include "parser.h"
  #include "main.h"
%}

%option noyywrap

%%

"or"    { return OR; }
"and"   { return AND; }
"not"   { return NOT; }

[a-zA-Z_][a-zA-Z0-9_]* { yylval.str = yytext; return ID; }

[0-9]+  { yylval.num = atoi(yytext); return NUMBER; }

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

[ \t\n]+ ;

.       { yyerror(NULL, "LEXICAL_ERR: Unrecognized token \"%s\""); }
