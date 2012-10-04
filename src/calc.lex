/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

%{
#include <stdio.h>
#include <stdlib.h>

#include "formula-parser.h"
#include "y.tab.h"


  /* Do not read from stdin, but from a string. my_yyinput takes care
     of filling in the buffer */
#undef YY_INPUT
int my_yyinput(char*, int);
#define YY_INPUT(t,r,m) (r = my_yyinput(t,m))


%}

blank     [ \t\n]+

letter    [A-Za-z]

base16    0x[0-9A-Fa-f]+

exponent  [eE][+-]?[0-9]+

word      [A-Za-z]([A-Za-z0-9]|_)*

constant  [0-9]+("."[0-9])?{exponent}?|{base16}

%%


{blank}  { /* ignored */ }


"delta"  return DELTA;
"and"    return AND;
"or"     return OR;
"shr"    return SHR;
"shl"    return SHL;

"|"      return OR;
"+"      return ADD;
"-"      return SUB;

"*"      return MUL;
"/"      return DIV;

"^"      return POWER;

"("      return B_LEFT;
")"      return B_RIGHT;

{base16}  {
    yylval.txt = strdup(yytext);
    return BASE16;
}

{constant} {
    yylval.txt = strdup(yytext);
    return NUMBER;
}

{word}  {
    yylval.txt = strdup(yytext);
    return COUNTER;
}

%%

int yywrap(void)
{
  return 1;
}


int yyerror(char *s)
{
  printf("error: %s at '%s'\n", s, yytext);
  return 0;
}
