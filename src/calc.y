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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "utils-expression.h"

extern expression* res_expr;

expression* build_node_operation(expression* exp1, expression* exp2, int opera);
expression* build_node_counter(char* alias, int delta);
expression* build_node_constant(char* txt);


int yylex();
int yyerror(char *s);

%}


%union {
    expression* e;
    char* txt ;
 };

%token <txt> NUMBER
%token <txt> COUNTER
%token <txt> EXPOSANT
%token <txt> BASE16

%token ADD SUB MUL DIV
%token OR AND SHR SHL
%token B_LEFT B_RIGHT
%token END
%token DELTA

%type <e> Expression Line

%left ADD SUB AND OR SHR SHL
%left MUL  DIV
%right  POWER

%start Line
%%

Line: Expression     { res_expr = $1; };

Expression:

BASE16   {
    $$ = build_node_counter($1, 0);
}
|
NUMBER   {
    $$ = build_node_constant($1);
}
| DELTA B_LEFT COUNTER B_RIGHT  {
    $$ = build_node_counter($3, DELT);
}
| COUNTER {
    $$ = build_node_counter($1, 0);
}
| Expression ADD Expression  {
    $$ = build_node_operation($1,$3, '+');
}
| Expression SUB Expression {
    $$ = build_node_operation($1,$3, '-');
}
| Expression MUL Expression  {
    $$ = build_node_operation($1,$3, '*');
}
| Expression DIV Expression {
    $$ = build_node_operation($1,$3, '/');
}
| Expression OR Expression {
    $$ = build_node_operation($1,$3, '|');
}
| Expression AND Expression {
    $$ = build_node_operation($1,$3, '&');
}
| Expression SHR Expression {
    $$ = build_node_operation($1,$3, '>');
}
| Expression SHL Expression {
    $$ = build_node_operation($1,$3, '<');
}
| B_LEFT Expression B_RIGHT  {
    $$=$2;
}

%%


expression* build_node_operation(expression* exp1, expression* exp2, int opera)
{
  expression* tmp = alloc_expression();
  tmp->type = OPER;
  tmp->op = alloc_operation();
  tmp->op->exp1 = exp1;
  tmp->op->exp2 = exp2;
  tmp->op->operator = opera;
  return tmp;
}


expression* build_node_counter(char* alias, int delta)
{
  expression* tmp = alloc_expression();
  tmp->ele = alloc_unit();
  tmp->ele->alias = alias;
  tmp->ele->type = COUNT;
  if (delta == DELT) {
    tmp->ele->delta = DELT;
  }
  tmp->type = ELEM;
  return tmp;
}


expression* build_node_constant(char* txt)
{
  expression* tmp = alloc_expression();
  tmp->ele = alloc_unit();
  errno = 0;
  tmp->ele->val = strtod(txt, NULL);
  if (errno != 0) {
    error_printf("Error while reading float in '%s'.\n", txt);
    tmp->type = ERROR;
  }
  else {
    tmp->ele->type = CONST;
    tmp->type = ELEM;
  }
  free(txt);
  return tmp;
}
