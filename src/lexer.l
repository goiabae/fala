%{
  #include <string.h>
  #include <stdlib.h>
#ifdef FALA_WITH_REPL
  #include <readline/readline.h>
  #include <readline/history.h>
#endif

  #include "parser.h"
  #include "main.h"

#if FALA_WITH_REPL
  #define YY_INPUT(BUF, RES, MAX) RES = get_input(yyin, BUF, MAX);
#endif

  static char* parse_string(char* str);

#if FALA_WITH_REPL
  static int get_input(FILE* fd, char *buf, int size);
#endif
%}

/* don't generate yyunput and input functions */
%option nounput noinput
%option noyywrap
%option reentrant bison-bridge

%%

#.*\n ;

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
"for"   { return FOR; }
"from"  { return FROM; }
"to"    { return TO; }
"while" { return WHILE; }
"var"   { return VAR; }

[a-zA-Z_][a-zA-Z0-9_]* { yylval->str = strdup(yytext); return ID; }

[0-9]+  { yylval->num = atoi(yytext); return NUMBER; }

\"[^\"]*\"  { yylval->str = parse_string(yytext); return STRING; }

\(      { return PAREN_OPEN; }
\)      { return PAREN_CLOSE; }
\[      { return BRACKET_OPEN; }
\]      { return BRACKET_CLOSE; }

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

.       { yyerror(NULL, NULL, "LEXICAL_ERR: Unrecognized token \"%s\""); }

%%

static char* parse_string(char* str) {
  const size_t len = strlen(str);
  char* new = malloc(sizeof(char) * (len - 2 + 1));
  size_t new_len = 0;
  for (size_t i = 1; i < len-1; i++) {
    if (str[i] == '\\' && str[i+1] == 'n')
        new[i++ - 1] = '\n';
    else
        new[i - 1] = str[i];
    new_len++;
  }
  new[new_len] = '\0';
  return new;
}

#ifdef FALA_WITH_REPL
static int get_input(FILE* fd, char *buf, int size) {
  if (feof(fd)) return YY_NULL;

  char *line = readline("> ");
  if(!line) return YY_NULL;
  if(strlen(line) > (size_t)size-2) {
    fprintf(stderr,"input line too long\n");
    return YY_NULL;
  }
  sprintf(buf,"%s\n",line);
  add_history(line);
  free(line);
  return strlen(buf);
}
#endif
