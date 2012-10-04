/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <config.h>

#ifdef ENABLE_DEBUG

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "options.h"

extern struct option options;

static const char* const debug_file_name = "tiptop.debug";
static FILE* debug_file = NULL;


void debug_printf(char* fmt, ...)
{
  va_list args;

  if (!options.debug)
    return;

  /* open file the first time we get here. */
  if (!debug_file) {
    debug_file = fopen(debug_file_name, "w");
    if (!debug_file) {
      perror("fopen");
      fprintf(stderr, "Cannot open debug file.\n");
      exit(EXIT_FAILURE);
    }
  }

  va_start(args, fmt);
  vfprintf(debug_file, fmt, args);
}

#endif  /* ENABLE_DEBUG */
