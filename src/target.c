/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <stdio.h>

#include "config.h"
#include "target.h"

#ifdef TARGET_X86
#include "target-x86.c"
#else



/* Default implementation for unknown target. */


enum targets get_target()
{
  return UNKNOWN_TARGET;
}


int match_target(const char* tgt)
{
  /* unknown target, matches nothing */
  return 0;
}


char* get_model()
{
  return NULL;
}


int match_model(const char* model)
{
  /* unknown model, matches nothing */
  return 0;
}


/* Generate a target-dependent string, displayed in the help window. */
void target_dep_string(char* buf, int size)
{
  snprintf(buf, size, "Unknown processor");
}


void screens_hook()
{
  /* empty */
}

#endif  /* TARGET_xxx */
