/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _ERROR_H
#define _ERROR_H

#include <config.h>

#ifdef HAVE_LIBCURSES
#include <curses.h>

WINDOW* prepare_error_win(int nb_tids);
void show_error_win(WINDOW* win, int nb_proc);

void scroll_up(void);
void scroll_down(void);
void scroll_page_up(void);
void scroll_page_down(void);
void scroll_home(void);
void scroll_end(void);

#endif  /* HAVE_LIBCURSES */

void init_errors(int batch_mode, const char* error_file_name);
void error_printf(char* fmt, ...);
void close_error();
int num_errors();

#endif  /* _ERROR_H */
