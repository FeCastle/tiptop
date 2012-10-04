/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <config.h>

#ifdef HAVE_LIBCURSES
#include <curses.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"


static FILE* error_file = NULL;

static int nb_error = 0;
static int scroll_ = 0;
static int win_height;

int num_errors()
{
  return nb_error;
}


void close_error()
{
  if (error_file)
    fclose(error_file);
}


void init_errors(int batch_mode, const char* error_file_name)
{
  /* if provided, use specified file name */
  if (error_file_name) {
    error_file = fopen(error_file_name, "w+");
    if (!error_file) {
      perror("fopen");
      fprintf(stderr, "Could not open file '%s'.\n", error_file_name);
    }
  }

  /* if not provided, or opening failed, second best choice depends on
     running mode */
  if (!error_file) {
    if (batch_mode)
      error_file = stderr;
    else {
      error_file = tmpfile();
      if (!error_file) {
        perror("tmpfile");
        fprintf(stderr, "Could not create tmp file for errors.\n");
      }
    }
  }
}


void error_printf(char* fmt, ...)
{
  va_list args;
  nb_error++;

  if (!error_file)
    return;

  va_start(args, fmt);
  vfprintf(error_file, fmt, args);
}


#ifdef HAVE_LIBCURSES

void scroll_up()
{
  if (scroll_ > 0)
    scroll_--;
}

void scroll_down()
{
  if (scroll_ < nb_error - 1)
    scroll_++;
}

void scroll_page_up()
{
  scroll_ -= (win_height - 4);
  if (scroll_ < 0)
    scroll_ = 0;
}

void scroll_page_down()
{
  scroll_ += (win_height - 4);
  if (scroll_ > nb_error - 1)
    scroll_ = nb_error - 1;
}

void scroll_home()
{
  scroll_ = 0;
}

void scroll_end()
{
  scroll_ = nb_error - (win_height - 5);
  if (scroll_ < 0)
    scroll_ = 0;
}



WINDOW* prepare_error_win(int nb_tids)
{
  WINDOW* we;
  win_height = LINES - nb_tids - 5;

  /* Keep most of the room on the screen to display the
     processes. However, in the presence of many processes are, make
     sure the error window is at least 10 lines. */
  if (win_height <= 10)
    win_height = 10;

  /* but not more than entire screen */
  if (win_height > LINES)
    win_height = LINES;

  we = newwin(win_height, COLS, LINES - win_height, 0);
  return we;
}


void show_error_win(WINDOW* win, int nb_proc)
{
  long  current_pos;
  int   maxx , maxy , i, pos=1;
  char* buf;
  static int nbp = 0;

  if (!error_file)
    return;

  if (nb_proc != -1)
    nbp = nb_proc;

  if (!win)
    win = prepare_error_win(nbp);

  getmaxyx(win, maxy, maxx);

  buf = alloca(maxx);

  /* char blank[maxx-3]; */
  /* for(i=0; i < maxx-3; i++) */
  /*   blank[i] = ' '; */

  /* Save currrent position in tiptop.error */
  current_pos = ftell(error_file);
  rewind(error_file);

  box(win, 0, 0);
  mvwprintw(win, 0, 5, " %d errors detected (e to close) ", nb_error);
  mvwprintw(win, maxy-1, 5, " %d %% ", 100 * scroll_ / nb_error);

  /* scrolling the file */
  for(i=0;
      i < scroll_ && fgets(buf, maxx-2, error_file) != NULL && i<(nb_error-1);
      i++)
  {
    if (buf[strlen(buf)-1] != '\n')
      i--;
  }

  if (i == 0)
    mvwprintw(win, pos++, 1, "BEGIN");
  else
    mvwprintw(win, pos++, 1, ".....");

  while (fgets(buf, maxx-2, error_file) != NULL && (pos < maxy-2)) {

    if (buf[strlen(buf)-1] == '\n') /* To keep box's border */
      buf[strlen(buf)-1] = 0;

    mvwprintw(win, pos++, 1, "%s", buf);
  }

  if (pos != maxy-2) { /* To complete screen */
    /* while (pos < maxy-2) */
    /*   mvwprintw(win, pos++, 1, "%s", blank); */
    mvwprintw(win, pos, 1, "END", i);
  }
  else
    mvwprintw(win, pos, 1, ".....", i);

  /* restoring older state of tiptop.error */
  fseek(error_file, current_pos, SEEK_SET);
  wrefresh(win);
}

#endif  /* HAVE_LIBCURSES */
