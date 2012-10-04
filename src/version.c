/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <stdio.h>

#include "config.h"
#include "version.h"


#if defined(PACKAGE_VERSION)
static const char tiptop_version[] = PACKAGE_VERSION ;
#else
static const char tiptop_version[] = "unknown" ;
#endif

#if defined(COMPILE_DATE)
static const char compile_date[] = COMPILE_DATE ;
#else
static const char compile_date[] = "unknown" ;
#endif

#if defined(COMPILE_HOST)
static const char compile_host[] = COMPILE_HOST ;
#else
static const char compile_host[] = "unknown" ;
#endif


void print_version()
{
  printf("This is tiptop version %s.\n", tiptop_version);
  printf("tiptop was compiled on \"%s\"\non host \"%s\".\n",
         compile_date, compile_host);
}

void print_legal()
{
  printf("tiptop %s.\n", tiptop_version);
  printf("Copyright (C) 2011, 2012 Inria.\n");
  printf("This is free software. No warranty.\n");
}
