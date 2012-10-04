/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _SPAWN_H
#define _SPAWN_H

#include <sys/types.h>

#include "options.h"

void spawn(char** argv);
void start_child(void);
void wait_for_child(pid_t pid, struct option* options);

#endif  /* _SPAWN_H */
