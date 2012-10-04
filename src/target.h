/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _TARGET_H
#define _TARGET_H

enum targets {
  X86,
  UNKNOWN_TARGET
};


enum targets get_target(void);
int match_target(const char* tgt);
char* get_model(void);
int match_model(const char* model);
void target_dep_string(char* buf, int size);


#endif  /* _TARGET_H */
