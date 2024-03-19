%{
  #include <string.h>
  #include <stdlib.h>
	#include <stdbool.h>
#ifdef FALA_WITH_REPL
  #include <readline/readline.h>
  #include <readline/history.h>
#endif

  int fileno(FILE *stream);
  char *strdup(const char *s);

  #include "parser.h"
  #include "main.h"

  static char* parse_string(char* str);
	bool is_interactive(void* scanner);

#ifdef FALA_WITH_REPL
  static int get_input(FILE* fd, char *buf, int size);
  #define YY_INPUT(BUF, RES, MAX) RES = get_input(yyin, BUF, MAX);
#endif

  #define yyerror(SCAN, AST, SYMS, ...) fprintf (stderr, __VA_ARGS__)
%}

/* don't generate yyunput and input functions */
%option nounput noinput
%option noyywrap
%option reentrant bison-bridge

%%

#.*\n ;

"do"    return DO;
"end"   return END;
"if"    return IF;
"then"  return THEN;
"else"  return ELSE;
"when"  return WHEN;
"for"   return FOR;
"from"  return FROM;
"to"    return TO;
"while" return WHILE;
"var"   return VAR;
"let"   return LET;
"in"    return IN;
"or"    return OR;
"and"   return AND;
"not"   return NOT;

"nil"   return NIL;
"true"  return TRUE;

[a-zA-Z_][a-zA-Z0-9_]* { yylval->str = strdup(yytext); return ID; }

[0-9]+  { yylval->num = atoi(yytext); return NUMBER; }

\"[^\"]*\"  { yylval->str = parse_string(yytext); return STRING; }

\(      return PAREN_OPEN;
\)      return PAREN_CLOSE;
\[      return BRACKET_OPEN;
\]      return BRACKET_CLOSE;
\;      return SEMICOL;
\,      return COMMA;

\>\=    return GREATER_EQ;
\<\=    return LESSER_EQ;
\=\=    return EQ_EQ;
\=      return EQ;
\>      return GREATER;
\<      return LESSER;
\+      return PLUS;
\-      return MINUS;
\*      return ASTER;
\/      return SLASH;
\%      return PERCT;

[ \t] ;

\n {
  #ifdef FALA_WITH_REPL
    if (is_interactive(yyscanner)) return EOF;
  #endif
}

. { yyerror(NULL, NULL, NULL, "LEXICAL_ERR: Unrecognized token \"%s\"\n", yytext); }

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
  if (fd != stdin) {
    if (!fgets(buf, size, fd)) return YY_NULL;
    return strlen(buf);
  }

  char* line = readline("fala> ");
  add_history(line);

  if(!line) return YY_NULL;
  const size_t len = strlen(line);
  if(len == 0) return YY_NULL;
  if(len > (size_t)size-2) {
    fprintf(stderr,"input line too long\n");
    return YY_NULL;
  }
  sprintf(buf,"%s\n",line);
  free(line);
  return strlen(buf);
}
#endif

bool is_interactive(void* scanner) {
  struct yyguts_t* yyg = (struct yyguts_t*)scanner;
	return yyg->yyin_r == stdin;
}
