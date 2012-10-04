/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _DEBUG_H
#define _DEBUG_H

#include <config.h>

#ifdef ENABLE_DEBUG
void debug_printf(char* fmt, ...);
#else
#define debug_printf(...)
#endif  /* ENABLE_DEBUG */

#endif  /* _DEBUG_H */
