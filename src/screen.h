/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _SCREEN_H
#define _SCREEN_H

#include <inttypes.h>

#include "formula-parser.h"
#include "options.h"

typedef struct {
  uint32_t  type;
  uint64_t  config;  /* Constant defined in configuration */
  char* alias;
  int used;
} counter_t;


typedef struct {
  char* header;
  char* format;  /* as in printf */
  char* empty_field;
  char* error_field;
  expression* expression;
  char* description;
} column_t;


typedef struct {
  char*   name;
  char*   desc;
  int        num_counters;
  int        num_alloc_counters;
  counter_t* counters;
  int        num_columns;
  int        num_alloc_columns;
  column_t*  columns;
} screen_t;


char* get_counter_config_name(uint64_t conf);
int get_counter_config(char* config, uint64_t* result);
char* get_counter_type_name(uint32_t type);

int screen_pos(const screen_t* s);
screen_t* new_screen(char* name, char* desc, int prepend);
int add_counter(screen_t* const s, char* alias, char* config, char* type);

int add_counter_by_value(screen_t* const s, char* alias,
                         uint64_t config_val, uint32_t type_val);

int add_column(screen_t* const s, char* header, char* format, char* desc,
               char* expr);

void tamp_counters(void);

void init_screen(void);
screen_t* get_screen(int);
screen_t* get_screen_by_name(const char* name);

int get_num_screens();

void list_screens(void);

char* gen_header(const screen_t* const s, const struct option* const,
                 int width, int active_col);

void delete_screen(screen_t* s);
void delete_screens();

void screens_hook(void);  /* to be implemented by target specific files */

int export_screens(struct option* o);

#endif  /* _SCREEN_H */
