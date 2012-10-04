/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _CONF_H
#define _CONF_H

#include "options.h"
#include "screen.h"


int read_config(char* path_conf_file, struct option* options);
int dump_configuration(screen_t** sc, int num_screens, struct option* o);

#endif  /* _CONF_H */
