/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _UTILS_EXPRESSION_H
#define _UTILS_EXPRESSION_H

#include <stdio.h>

#include "formula-parser.h"
#include "process.h"


operation* alloc_operation();
expression* alloc_expression();
unit* alloc_unit();

void free_expression (expression* e);
void free_unit(unit* u);
void free_operation(operation* p);


void parcours_expression(expression* e);

int build_expression(expression* e, FILE* fd);

expression* parser_expression (char* txt);

double evaluate_column_expression(expression* e, counter_t* c, int nbc,
                           struct process* p, int* error);
uint64_t evaluate_counter_expression(expression* e, int* error);

#endif  /* _UTILS_EXPRESSION_H */
