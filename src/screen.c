/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include "config.h"

#ifdef HAVE_LINUX_PERF_COUNTER_H
#include <linux/perf_counter.h>
#elif defined(HAVE_LINUX_PERF_EVENT_H)
#include <linux/perf_event.h>
#else
#error Sorry, performance counters not supported on this system.
#endif

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "conf.h"
#include "options.h"
#include "process.h"
#include "screen.h"
#include "utils-expression.h"
#include "error.h"


static const int alloc_chunk = 10;

static int num_screens = 0;
static int num_alloc_screens = 0;
static screen_t** screens = NULL;

/*
 * The following functions factorize counters used in various columns
 * of a screen.
 */

/* Navigate into expressions, and mark used counters */
static void check_counters_used(expression* e, screen_t* s, int* error)
{

  int i = 0;
  int found;

  if (e->type == ELEM && e->ele->type == COUNT) {
    found = -1;
    if (strcmp(e->ele->alias, "CPU_TOT") == 0 ||
        strcmp(e->ele->alias, "CPU_SYS") == 0 ||
        strcmp(e->ele->alias, "CPU_USER") == 0 ||
        strcmp(e->ele->alias, "PROC_ID") == 0 )
      return ;

    for(i=0; i < s->num_counters; i++) {
      assert(s->counters[i].alias != NULL);
      if (strcmp(e->ele->alias, s->counters[i].alias) == 0)
        found = i;
    }

    if (found >= 0)
      s->counters[found].used++;
    else{
      error_printf("Undeclared counter '%s' in screen '%s': column ignored\n",
                   e->ele->alias, s->name);
      (*error)++;
    }
  }
  else if (e->type == OPER && e->op != NULL) {
    check_counters_used(e->op->exp1, s, error);
    check_counters_used(e->op->exp2, s, error);
  }
}


/* delete unmarked counters */
static void delete_and_shift_counters(int sc, int co)
{
  int i;
  int nbc = screens[sc]->num_counters;
  counter_t* tmp = &screens[sc]->counters[co];

  if (tmp->alias)
    free(tmp->alias);

  for(i=co; i < nbc-1; i++) {
    tmp = &screens[sc]->counters[i];
    tmp->type   = screens[sc]->counters[i+1].type;
    tmp->config = screens[sc]->counters[i+1].config;
    tmp->alias  = screens[sc]->counters[i+1].alias;
    tmp->used   = screens[sc]->counters[i+1].used;
  }
  screens[sc]->num_counters--;
}


/* Remove declared but unused counter from the "counters" list */
void tamp_counters ()
{
  int i,j;
  int nbs = num_screens;
  for(i=0; i<nbs; i++) {
    j=0;
    while (j < screens[i]->num_counters)
      if (screens[i]->counters[j].used == 0) {
        error_printf("Unused counter '%s' in screen '%s'\n",
                     screens[i]->counters[j].alias,
                     screens[i]->name);
        delete_and_shift_counters(i, j);
      }
      else
        j++;
  }
}


static screen_t* alloc_screen()
{
  screen_t* s = malloc(sizeof(screen_t));
  s->name = NULL;
  s-> desc = NULL;
  s->counters = NULL;
  s->columns = NULL;

  s->num_counters = 0;
  s->num_alloc_counters = 0;
  s->num_columns = 0;
  s->num_alloc_columns = 0;

  return s;
}


static void init_column(column_t* c)
{
  c->header = NULL;
  c->format = NULL;
  c->empty_field = NULL;
  c->error_field = NULL;
  c->expression = NULL;
  c->description = NULL;
}


int screen_pos(const screen_t* s)
{
  int i;
  for(i=0; i < num_screens; i++) {
    if (screens[i] == s)
      return i;
  }
  assert(0);
  return -1;
}


screen_t* new_screen(char* name, char* desc, int prepend)
{
  screen_t* the_screen = alloc_screen();
  the_screen->name = strdup(name);
  if (desc == NULL || strlen(desc) == 0)
    the_screen->desc = strdup("(no desc)");
  else
    the_screen->desc = strdup(desc);
  the_screen->num_counters = 0;
  the_screen->num_alloc_counters = alloc_chunk;
  the_screen->counters =  malloc(alloc_chunk * sizeof(counter_t));
  the_screen->num_columns = 0;
  the_screen->num_alloc_columns = alloc_chunk;
  the_screen->columns = malloc(alloc_chunk * sizeof(column_t));

  if (num_screens >= num_alloc_screens) {
    num_alloc_screens += alloc_chunk;
    screens = realloc(screens, num_alloc_screens * sizeof(screen_t*));
  }
  if (prepend) {
    /* shift existing screens by 1 */
    memmove(&screens[1], &screens[0], num_screens * sizeof(screen_t*));
    screens[0] = the_screen;
  }
  else {
    screens[num_screens] = the_screen;
  }

  num_screens++;
  return the_screen;
}


struct predefined_event {
  int   perf_hw_id;
  char* name;
};

struct predefined_event events[] = {
  /*
   * Derived from /usr/include/linux/perf_event.h
   * Common hardware events, generalized by the kernel:
   */
  { PERF_COUNT_HW_CPU_CYCLES, "CPU_CYCLES" },
  { PERF_COUNT_HW_INSTRUCTIONS, "INSTRUCTIONS" },
  { PERF_COUNT_HW_CACHE_REFERENCES, "CACHE_REFERENCES" },
  { PERF_COUNT_HW_CACHE_MISSES, "CACHE_MISSES" },
  { PERF_COUNT_HW_BRANCH_INSTRUCTIONS, "BRANCH_INSTRUCTIONS" },
  { PERF_COUNT_HW_BRANCH_MISSES, "BRANCH_MISSES" },
  { PERF_COUNT_HW_BUS_CYCLES, "BUS_CYCLES" },

  /* Add by antoine */

  { PERF_COUNT_HW_CACHE_L1D, "L1D"},
  { PERF_COUNT_HW_CACHE_L1I, "L1I"},
  { PERF_COUNT_HW_CACHE_LL, "LL"},
  { PERF_COUNT_HW_CACHE_DTLB, "DTLB" },
  { PERF_COUNT_HW_CACHE_ITLB, "ITLB" },
  { PERF_COUNT_HW_CACHE_BPU, "BPU" },

  { PERF_COUNT_HW_CACHE_OP_READ, "OP_READ"},
  { PERF_COUNT_HW_CACHE_OP_WRITE, "OP_WRITE"},
  { PERF_COUNT_HW_CACHE_OP_PREFETCH, "OP_PREFETCH"},
  { PERF_COUNT_HW_CACHE_RESULT_ACCESS, "RESULT_ACCESS"},
  { PERF_COUNT_HW_CACHE_RESULT_MISS, "RESULT_MISS"},

#if 0
  /* Appear in Linux 3.0 */
  { PERF_COUNT_HW_STALLED_CYCLES_FRONTEND, "STALLED_CYCLES_FRONTEND" },
  { PERF_COUNT_HW_STALLED_CYCLES_BACKEND, "STALLED_CYCLES_BACKEND" },
  /* Appear in Linux 3.3 */
  { PERF_COUNT_HW_REF_CPU_CYCLES, "REF_CPU_CYCLES" },
#endif
  { PERF_COUNT_HW_MAX, NULL }
};


struct predefined_type {
  int   perf_type_id;
  char* name;
};

struct predefined_type types[] = {
  { PERF_TYPE_HARDWARE, "HARDWARE" },
  { PERF_TYPE_SOFTWARE, "SOFTWARE" },
  { PERF_TYPE_TRACEPOINT, "TRACEPOINT" },
  { PERF_TYPE_HW_CACHE, "HW_CACHE" },
  { PERF_TYPE_RAW, "RAW" },
#if 0
  /* Appear in Linux 2.6.33 */
  { PERF_TYPE_BREAKPOINT, "BREAKPOINT" },
#endif
  { PERF_TYPE_MAX, NULL }
};


char* get_counter_type_name(uint32_t type)
{
  int i = 0;

  while (types[i].perf_type_id != PERF_TYPE_MAX &&
         types[i].perf_type_id != type)
    i++;

  if (types[i].perf_type_id == PERF_TYPE_MAX)  /* not found */
    return NULL;
  else
    return types[i].name;
}


char* get_counter_config_name(uint64_t conf)
{
  int i = 0;

  while (events[i].perf_hw_id != PERF_COUNT_HW_MAX &&
         events[i].perf_hw_id != conf)
    i++;

  if (events[i].perf_hw_id == PERF_COUNT_HW_MAX)  /* not found */
    return NULL;
  else
    return events[i].name;
}


/* the result is stocked in variable result, return 0 for a HIT else -1 */
int get_counter_config(char* config, uint64_t* result)
{
  int i = 0;

  if (config == NULL)
    return -1;

  if (isdigit(*config)) {
    /* If config is given with a constant */

    uint64_t val_conf;
    errno = 0;
    val_conf = strtol(config, NULL, 0);
    if (errno != ERANGE) {   /* make sure we read the config value */
      *result = (uint64_t) val_conf;
      return 0;
    }
  }
  else {
    while (events[i].perf_hw_id != PERF_COUNT_HW_MAX) {
      if (strcmp(config, events[i].name) == 0){
        *result  = events[i].perf_hw_id;
        return 0;
      }
      i++;
    }
  }
  /* not found */
  return -1;
}


static uint32_t get_counter_type(char* type, int* error)

{
  int i = 0;
  *error = 0;

  if (type == NULL)
    return PERF_TYPE_HARDWARE;

  if (isdigit(*type)) {
    uint32_t val_type;
    errno = 0;
    val_type = strtol((char*)type, NULL, 0);

    if (errno != ERANGE)  /* make sure we read the type value */
      return (uint32_t) val_type;
  }
  else {

    while (types[i].perf_type_id != PERF_TYPE_MAX) {
      if (strcmp(type, types[i].name) == 0)
        return types[i].perf_type_id;

      i++;
    }
  }
  /* not found */
  *error = 1;
  return 0;
}


/* Adding a new counter in counters list */
int add_counter(screen_t* const s, char* alias, char* config, char* type)
{
  uint64_t int_conf = 0;
  uint32_t int_type = 0;
  int err=0;
  int n;
  expression* expr = NULL;

  if (s->num_counters >=  MAX_EVENTS) {
    error_printf("Too many counters (max %d) in screen '%s', ignoring '%s'\n"
                 "(change MAX_EVENTS and recompile)\n",
                 MAX_EVENTS, s->name, alias);
    return -1;
  }

  int_type = get_counter_type(type, &err);

  if (err > 0) {
    /* error*/
    error_printf("Bad type '%s': ignoring counter '%s'\n", type, alias);
    return -1;
  }

  /* Parse the configuration */
  expr = parser_expression(config);

  err=0;
  int_conf = evaluate_counter_expression(expr, &err);

  free_expression(expr);

  if (err > 0) {
    /* error*/
    error_printf("Bad config '%s': ignoring counter '%s'\n", config, alias);
    return -1;
  }

  n = s->num_counters;
  /* check max available hw counter */
  if (n == s->num_alloc_counters) {
    s->counters = realloc(s->counters, sizeof(counter_t) * (n + alloc_chunk));
    s->num_alloc_counters += alloc_chunk;
  }
  /* initialisation */
  s->counters[n].used = 0;
  s->counters[n].type = int_type;
  s->counters[n].config = int_conf;
  s->counters[n].alias = strdup(alias);
  s->num_counters++;
  return n;
}


/* Adding a new counter in counters list */
int add_counter_by_value(screen_t* const s, char* alias,
                         uint64_t config_val, uint32_t type_val)
{
  int n = s->num_counters;

  if (n >= MAX_EVENTS) {
    error_printf("Too many counters (max %d) in screen '%s', ignoring '%s'\n"
                 "(change MAX_EVENTS and recompile)",
                 MAX_EVENTS, s->name,  alias);
    return -1;
  }

  /* check max available hw counter */
  if (n == s->num_alloc_counters) {
    s->counters = realloc(s->counters, sizeof(counter_t) * (n + alloc_chunk));
    s->num_alloc_counters += alloc_chunk;
  }
  /* initialisation */
  s->counters[n].used = 0;
  s->counters[n].config = config_val;
  s->counters[n].alias = strdup(alias);
  s->counters[n].type = type_val;
  s->num_counters++;
  return n;
}


int add_column(screen_t* const s, char* header, char* format, char* desc,
               char* expr)
{
  int col_width, err=0;
  int n = s->num_columns;

  expression* e = parser_expression(expr);

  if (e == NULL || e->type == ERROR) {
    free_expression(e);
    error_printf("Invalid expression in column '%s', screen '%s': column ignored\n",
                 header, s->name);
    return -1;
  }

  check_counters_used(e, s, &err);
  if( err > 0 ) {
    free_expression(e);
    return -1;
  }

  if (n == s->num_alloc_columns) {
    s->columns = realloc(s->columns, sizeof(column_t) * (n + alloc_chunk));
    s->num_alloc_columns += alloc_chunk;
  }
  init_column(&s->columns[n]);
  s->columns[n].expression = e;
  s->columns[n].header = strdup(header);
  s->columns[n].format = strdup(format);

  col_width = strlen(header);
  /* setup an empty field with proper width */
  s->columns[n].empty_field = malloc(col_width + 1);
  memset(s->columns[n].empty_field, ' ', col_width - 1);
  s->columns[n].empty_field[col_width - 1] = '-';
  s->columns[n].empty_field[col_width] = '\0';

  /* setup an error field with proper width */
  s->columns[n].error_field = malloc(col_width + 1);
  memset(s->columns[n].error_field, ' ', col_width - 1);
  s->columns[n].error_field[col_width - 1] = '?';
  s->columns[n].error_field[col_width] = '\0';

  if (desc)
    s->columns[n].description = strdup(desc);
  else
    s->columns[n].description = strdup("(unknown)");

  s->num_columns++;
  return n;
}


/* This is the default screen, it uses only target-independent
   counters defined in the Linux header file. */
static screen_t* default_screen()
{
  screen_t* s = new_screen("default", "Screen by default", 1);

  /* setup counters */
  add_counter_by_value(s, "CYCLE", PERF_COUNT_HW_CPU_CYCLES,    PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "INSN",  PERF_COUNT_HW_INSTRUCTIONS,  PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "MISS",  PERF_COUNT_HW_CACHE_MISSES,  PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "BR",    PERF_COUNT_HW_BRANCH_MISSES, PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "BUS",   PERF_COUNT_HW_BUS_CYCLES,    PERF_TYPE_HARDWARE);

  /* add columns */
  add_column(s, " %CPU", "%5.1f", "Total CPU usage", "CPU_TOT");
  add_column(s, " %SYS", "%5.1f", "System CPU usage", "CPU_SYS");
  add_column(s, "   P", "  %2.0f", "Processor where last seen", "PROC_ID");
  add_column(s, "  Mcycle", "%8.2f", "Cycles (millions)",
             "delta(CYCLE) / 1e6");
  add_column(s, "  Minstr", "%8.2f", "Instructions (millions)",
             "delta(INSN) / 1e6");
  add_column(s, "  IPC",     " %4.2f", "Executed instructions per cycle",
             "delta(INSN)/delta(CYCLE)");
  add_column(s, " %MISS",   "%6.2f", "Cache miss per 100 instructions",
             "100*delta(MISS)/delta(INSN)");
  add_column(s, " %BMIS",   "%6.2f", "Mispredicted branches per 100 instructions",
             "100*delta(BR)/delta(INSN)");
  add_column(s, " %BUS",    "%5.1f", "Bus cycles per executed instruction",
             "delta(BUS)/delta(INSN)");
  return s;
}


static screen_t* branch_pred_screen()
{
  screen_t* s = new_screen("branch", "Branch prediction statistics", 1);

  /* setup counters */
  add_counter_by_value(s, "INSTR", PERF_COUNT_HW_INSTRUCTIONS, PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "BR", PERF_COUNT_HW_BRANCH_INSTRUCTIONS, PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "BMISS", PERF_COUNT_HW_BRANCH_MISSES, PERF_TYPE_HARDWARE);

  /* add columns */
  add_column(s, "  %CPU", " %5.1f", "CPU usage", "CPU_TOT");
  add_column(s, "   %MIS/I", "   %6.2f",
             "Mispredictions per 100 instructions",
             "100 * delta(BMISS) / delta(INSTR)");

  add_column(s, "   %MISP", "   %5.2f",
             "Mispredictions per 100 branch instructions",
             "100 * delta(BMISS) / delta(BR)");

  add_column(s, "  %BR/I", "  %5.1f",
             "Proportion of branch instructions",
             "100 * delta(BR) / delta(INSTR)");
  return s;
}


void init_screen()
{
  branch_pred_screen();
  default_screen();

  screens_hook();  /* target dependent screens, if any */
}


screen_t* get_screen(int num)
{
  if (num >= num_screens)
    return NULL;
  return screens[num];
}


/* Return the first screen whose name contains the parameter, and NULL
   if no screen matches. */
screen_t* get_screen_by_name(const char* name)
{
  int i;
  for(i=0; i < num_screens; i++) {
    if (strstr(screens[i]->name, name))
      return screens[i];
  }
  return NULL;
}


char* gen_header(const screen_t* const s, const struct option* options,
                 int width, int active_col)
{
  char* hdr;
  char* ptr;
  int   num_cols, i, written = 0;
  const char sep = ' ';
  const char high_on = '[';
  const char high_off = ']';

  hdr = malloc(width);
  ptr = hdr;

  if (options->show_timestamp && options->batch) {
    written = snprintf(ptr, width, "timest ");
    ptr += written;
    width -= written;
  }

  if (options->show_epoch && options->batch) {
    written = snprintf(ptr, width, "     epoch ");
    ptr += written;
    width -= written;
  }

  if (options->show_user)
    written = snprintf(ptr, width, " %cPID%c user      ",
                       active_col == -1 ? high_on : sep,
                       active_col == -1 ? high_off : sep);
  else
    written = snprintf(ptr, width, " %cPID%c",
                       active_col == -1 ? high_on : sep,
                       active_col == -1 ? high_off : sep);

  ptr += written;
  width -= written;

  num_cols = s->num_columns;
  for(i=0; i < num_cols; i++) {

    /* add space, if it fits */
    if (width >= 2) {
      if (i == active_col)
        ptr[0] = high_on;
      else if ((i-1 == active_col) && (i != 0))
        ptr[0] = high_off;
      else
        ptr[0] = sep;
      ptr[1] = '\0';
      ptr++;
      width--;
    }

    /* add column header */
    written = snprintf(ptr, width, "%s", s->columns[i].header);
    if (written >= width) {
      width = 0;
      break;
    }
    ptr += written;
    width -= written;
  }
  snprintf(ptr, width, "%cCOMMAND%c",
           active_col == s->num_columns-1
                      ? high_off
                      : active_col == s->num_columns ? high_on : sep,
           active_col == s->num_columns ? high_off : sep);
  return hdr;
}


int get_num_screens()
{
  return num_screens;
}


void list_screens()
{
  int i;
  printf("Available screens:\n");
  for(i=0; i < num_screens; i++) {
    printf("%2d: '%s', %s\n", i, screens[i]->name, screens[i]->desc);
  }
}


static void delete_counters (counter_t* c, int nbc)
{
  int i;
  for(i=0;i<nbc;i++){
    if (c[i].alias)
      free(c[i].alias);
  }
  free(c);
}


static void delete_column(column_t* t)
{
  if(t->expression)
    free_expression(t->expression);
  if(t->description)
    free(t->description);
  if(t->format)
    free(t->format);
  if (t->header)
    free(t->header);
  free(t->error_field);
  free(t->empty_field);
}


static void delete_columns (column_t* t, int nbc)
{
  int i;
  for(i=0;i<nbc;i++)
    delete_column(&t[i]);

  free(t);
}


void delete_screen(screen_t* s)
{
  assert(s);
  free(s->name);
  free(s->desc);
  delete_counters(s->counters, s->num_counters);
  delete_columns(s->columns, s->num_columns);
  free(s);
}


/* Delete all screens. */
void delete_screens()
{
  int i;
  for(i=0; i < num_screens; i++) {
    delete_screen(screens[i]);
  }
  free(screens);
}


/* Write in a file options and every screens in the session: screens
   by default and file configuration screens */

int export_screens(struct option* opt)
{
  dump_configuration(screens, num_screens, opt);
  return 0;
}
