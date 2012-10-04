/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "conf.h"
#include "options.h"
#include "utils-expression.h"
#include "xml-parser.h"

static const char* const config_file = ".tiptoprc";

/* Read configuration file.
 *
 * First check the path specified on the command line (if any), then
 * the TIPTOP environment variable, then the local directory, finally
 * $HOME. */

int read_config(char* path_conf_file, struct option* options)
{
#ifdef HAVE_LIBXML2
  char* path = NULL;
  char* file = NULL;
  int   res;

  /* Check path with '-W' in tiptop options */
  if (path_conf_file != NULL) {
    path = path_conf_file;
  }

  if (!path) {
      /* Check Env. Var.  $TIPTOP */
      path = getenv("TIPTOP");
    }

  if (!path) {
      /* Check Current Folder */
      if (access(config_file, R_OK) == 0) {
        path = ".";
      }
    }

  if (!path) {
    /* Check $HOME */
    path = getenv("HOME");
  }

  if (!path)
    return -1;

  file = malloc(strlen(path) + strlen(config_file) + 2);
  sprintf(file, "%s/%s", path, config_file);

  if (access(file, R_OK) == -1) {
    free(file);
    return -1;
  }

  res = parse_doc(file, options);
  free(file);
  return res;

#else  /* HAVE_LIBXML2 */

  fprintf(stderr, "No xml support, cannot read config file.\n");
  return -1;

#endif  /* !HAVE_LIBXML2 */
}


static const char* const tok_opt_sta = "\t\t<option name=\"";
static const char* const tok_opt_mid = "\" value=\"";
static const char* const tok_opt_end ="\" />";


static int dump_option_string(FILE* out, char* name, char* value)
{
  if (value == NULL)
    return 0;
  return fprintf(out, "%s%s%s%s%s\n",
                 tok_opt_sta, name, tok_opt_mid, value, tok_opt_end);
}


static int dump_option_int(FILE* out, char* name, int value)
{
  return fprintf(out, "%s%s%s%d%s\n",
                 tok_opt_sta, name, tok_opt_mid, value, tok_opt_end);
}

static int dump_option_float(FILE* out, char* name, float value)
{
  return fprintf(out, "%s%s%s%f%s\n",
                 tok_opt_sta, name, tok_opt_mid, value, tok_opt_end);
}

static const char* const opt_sta = "\t<options>";
static const char* const opt_clo = "\t</options>";

static int dump_options(FILE* out, struct option* opt)
{
  if (fprintf(out, "%s\n", opt_sta) < 0)
    return -1;
  if (dump_option_float(out, "cpu_threshold", opt->cpu_threshold ) < 0)
    return -1;
  if (dump_option_float(out, "delay", opt->delay ) < 0)
    return -1;
  if (dump_option_string(out, "watch_name", opt->watch_name) < 0)
    return -1;
  if (dump_option_int(out, "max_iter", opt->max_iter) < 0)
    return -1;
  if (dump_option_string(out, "only_name", opt->only_name) < 0)
    return -1;
  if (dump_option_int(out, "only_pid",(int) opt->only_pid) < 0)
    return -1;
  if  (dump_option_int(out, "debug", opt->debug) < 0)
    return -1;
  if (dump_option_int(out, "batch", opt->batch) < 0)
    return -1;
  if (fprintf(out, "%s\n", opt_clo) < 0)
    return -1;

  return 0;
}


static const char* const cou_sta  = "\t\t<counter alias=\"";
static const char* const cou_mid1 = "\" config=\"";
static const char* const cou_mid2 = "\" type=\"";
/* static const char* const cou_arch = "\" arch=\""; */
/* static const char* const cou_model = "\" model=\""; */
static const char* const cou_clo  = "\" />";
static char* const default_type  = "PERF_TYPE_HW";


static int dump_counter(FILE* out, counter_t* c)
{
  int alloc_c=0, alloc_t=0;
  char* config = NULL;
  char* type = NULL;

  config = get_counter_config_name(c->config);
  type = get_counter_type_name(c->type);

  if (config == NULL) {
    /* Hexadecimal Convertion */
    alloc_c++;
    config = malloc(sizeof(char)*3);
    sprintf(config, "0x%"PRIx64, c->config);
  }
  if (type == NULL && c->type != -1) {
    /* Hexadecimal Convertion */
    alloc_t++;
    type = malloc(sizeof(char)*3);
    sprintf(type,"0x%x", c->type);
  }
  else if (type == NULL && c->type == -1)
    type = default_type;

  if (fprintf(out, "%s%s%s%s%s%s%s\n", cou_sta, c->alias, cou_mid1, config,
              cou_mid2, type, cou_clo) < 0)
    goto end;

  if (alloc_c)
    free(config);
  if (alloc_t)
    free(type);
  return 0;

 end:
  if(alloc_c)
    free(config);
  if(alloc_t)
    free(type);
  return -1;
}


static const char* const col_sta  = "\t\t<column header=\"";
static const char* const col_format = "\" format=\"";
static const char* const col_desc = "\" desc=\"";
static const char* const col_expr = "\" expr=\"";
static const char* const col_clo  = "\" />";

static int dump_column(FILE* out, column_t* c)
{
  if (fprintf(out, "%s%s%s%s%s%s%s",
              col_sta, c->header, col_format, c->format, col_desc,
              c->description, col_expr) < 0)
    return -1;
  if (build_expression(c->expression, out) < 0)
    return -1;
  if (fprintf(out, "%s\n", col_clo) < 0)
    return -1;
  return 0;
 }

static const char* const screen_sta = "\t<screen name=\"";
static const char* const screen_mid = "\" desc=\"";
static const char* const screen_clo = "\">";
static const char* const screen_end = "\t</screen>";

static int dump_screen(FILE* out, screen_t* s)
{
  int i;

  if (s->desc == NULL) {
    if (fprintf(out, "%s%s%s\n", screen_sta, s->name, screen_clo) < 0)
      return -1;
  }
  else {
    if (fprintf(out, "%s%s%s%s%s\n",
                screen_sta, s->name, screen_mid, s->desc, screen_clo) < 0)
      return -1;
  }
  /* Print Counters*/
  for(i=0;i<s->num_counters;i++)
    if (dump_counter(out, &s->counters[i]) < 0)
      return -1;
  /* Print Columns */
  for(i=0;i<s->num_columns;i++)
    if (dump_column(out, &s->columns[i]) < 0)
      return -1;

  if (fprintf(out, "%s\n", screen_end) < 0)
    return -1;
  return 0;
}


static int dump_screens(FILE* out, screen_t** s, int nbs)
{
  int i;

  if (nbs == 0)
    return 0;

  for(i=0; i < nbs; i++)
    if (dump_screen(out, s[i]) < 0)
      return -1;

  return 0;
}

static const char* const tip_sta ="<tiptop>";
static const char* const tip_clo = "</tiptop>";
static const char* const introduction =
"<!-- tiptop configuration file -->\n"
"\n"
"<!-- Rename this file to .tiptoprc,                                       -->\n"
"<!-- and place it either in your current directory, the location          -->\n"
"<!-- specified in $TIPTOP, or in your $HOME.                              -->\n"
;


/* Main function to export a configuration file */
int dump_configuration(screen_t** sc, int num_screens, struct option* o)
{
  FILE* out;

  /* Don't overwrite existing file */
  if (access(config_file, F_OK) != -1)
    return -1;

  out = fopen(config_file, "w");
  if (out == NULL)
    return -1;

  if (fprintf(out, "%s\n%s\n", introduction, tip_sta) < 0)
    return -1;
  if (dump_options(out, o) < 0)
    return -1;
  if (dump_screens(out, sc, num_screens) < 0)
    return -1;
  if (fprintf(out, "%s\n", tip_clo) < 0)
    return -1;

  fclose(out);
  return 0;
}
