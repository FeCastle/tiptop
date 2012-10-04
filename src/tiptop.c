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

#include <assert.h>

#ifdef HAVE_LIBCURSES
#include <curses.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "conf.h"
#include "debug.h"
#include "error.h"
#include "helpwin.h"
#include "options.h"
#include "pmc.h"
#include "process.h"
#include "requisite.h"
#include "screen.h"
#include "spawn.h"
#include "utils-expression.h"

struct option options;

static struct timeval tv;

static char* message = NULL;
static char* header = NULL;

static int active_col = 0;

static enum sorting_order {
  DESCENDING,
  ASCENDING
} sorting_order = DESCENDING;

static int (*sorting_fun)(const void *, const void *);


/* Utility functions used by qsort to sort processes according to
   active column */
static int cmp_double(const void* p1, const void* p2)
{
  int res;
  struct process* proc1 = *(struct process**)p1;
  struct process* proc2 = *(struct process**)p2;
  if (proc1->u.d > proc2->u.d)
    res = -1;
  else if (proc1->u.d == proc2->u.d)
    res = 0;
  else
    res = 1;
  if (sorting_order == DESCENDING)
    return res;
  else
    return -res;
}

static int cmp_int(const void* p1, const void* p2)
{
  int res;
  struct process* proc1 = *(struct process**)p1;
  struct process* proc2 = *(struct process**)p2;
  if (proc1->u.i > proc2->u.i)
    res = -1;
  else if (proc1->u.i == proc2->u.i)
    res = 0;
  else
    res = 1;
  if (sorting_order == DESCENDING)
    return res;
  else
    return -res;
}

#if 0
/* unused for now */
static int cmp_long(const void* p1, const void* p2)
{
  struct process* proc1 = *(struct process**)p1;
  struct process* proc2 = *(struct process**)p2;
  int res;
  if (proc1->u.l > proc2->u.l)
    res = -1;
  else if (proc1->u.l == proc2->u.l)
    res = 0;
  else
    res = 1;
  if (sorting_order == DESCENDING)
    return res;
  else
    return -res;
}
#endif

static int cmp_string(const void* p1, const void* p2)
{
  struct process* proc1 = *(struct process**)p1;
  struct process* proc2 = *(struct process**)p2;
  int res;
  if (options.show_cmdline)
    res = strcmp(proc1->cmdline, proc2->cmdline);
  else
    res = strcmp(proc1->name, proc2->name);
  if (sorting_order == ASCENDING)
    return res;
  else
    return -res;
}


/* For each process/thread in the list, generate the text form, ready
 * to be printed.
 */
static void build_rows(struct process_list* proc_list, screen_t* s, int width)
{
  int row_width;
  struct process* p;
  assert(TXT_LEN > 20);



  row_width = TXT_LEN;
  if ((width != -1) && (width < row_width))
    row_width = width;

  /* For the time being, column -1 is the PID, columns 0 to
     "num_columns-1" are the columns specified in the screen, and
     column "num_columns" is the task name. */

  /* select appropriate sorting function */
  if (active_col == s->num_columns)  /* proc name or command */
    sorting_fun = cmp_string;
  else if ((active_col == -1))  /* PID */
    sorting_fun = cmp_int;
  else
    sorting_fun = cmp_double;  /* (computed) expression */

  /* For all processes/threads */
  for(p = proc_list->processes; p; p = p->next) {
    int   col, written;
    char* row = p->txt;  /* the row we are building */
    int   remaining = row_width;  /* remaining bytes in row */
    int   thr = ' ';

    p->skip = 1;  /* first, assume not ready */


    /* dead, not changing anymore, the row should be up-to-date. */
    if ((p->dead) && (!options.sticky))
      continue;

    /* not active, skip */
    if (!options.idle && (p->cpu_percent < options.cpu_threshold))
      continue;

    /* only some tasks are monitored, skip those that do not qualify */
    if (((options.only_pid) && (p->tid != options.only_pid)) ||
        (options.only_name && options.show_cmdline &&
                                  !strstr(p->cmdline, options.only_name)) ||
        (options.only_name && !options.show_cmdline &&
                                  !strstr(p->name, options.only_name)))
      continue;

    if (active_col == -1)  /* column -1 is the PID */
      p->u.i = p->tid;

    /* display a '+' or '-' sign after processes made of multiple threads */
    if (p->num_threads > 1) {
      if (p->tid == p->pid)
        thr = '+';
      else
        thr = '-';
    }

    if (options.show_user)
      written = snprintf(row, remaining, "%5d%c %-10s ", p->tid, thr,
                                                         p->username);
    else
      written = snprintf(row, remaining, "%5d%c ", p->tid, thr);
    row += written;
    remaining -= written;

    for(col = 0; col < s->num_columns; col++) {
      double res = 0;
      int error = 0;  /* used to track error situations requiring an
                         error_field (code 1) or an empty_field (code 2) */
      const char* const fmt = s->columns[col].format;

      /* zero the sorting field. The double '.d' is the longest field. */
      if (active_col == col)
        p->u.d = 0.0;

      res = evaluate_column_expression(s->columns[col].expression,
                                s->counters,
                                s->num_counters,
                                p, &error);

      if (error == 1)
        written = snprintf(row, remaining, "%s", s->columns[col].error_field);
      else if (error == 2)
        written = snprintf(row, remaining, "%s", s->columns[col].empty_field);
      else {
        written = snprintf(row, remaining, fmt, res);
      }
      if (active_col == col)
        p->u.d = res;

      /* man snprintf: The functions snprintf() and vsnprintf() do not
       write more than size bytes (including the trailing '\0').  If
       the output was truncated due to this limit then the return
       value is the number of characters (not including the trailing
       '\0') which would have been written to the final string if
       enough space had been available.  Thus, a return value of size
       or more means that the output was truncated.  */
      if (written >= remaining) {
        remaining = 0;
        break;  /* line is full */
      }

      row += written;
      remaining -= written;

      /* add space after column, if it fits */
      if (remaining >= 2) {
        row[0] = ' ';
        row[1] = '\0';
        row++;
        remaining--;
      }
    }

    if (options.show_cmdline)
      strncpy(row, p->cmdline, remaining);
    else
      strncpy(row, p->name, remaining);

    if (remaining)
      row[remaining-1] = '\0';

    p->skip = 0;
  }
}


/* Main execution loop in batch mode. Builds the list of processes,
 * collects statistics, and prints. Repeats after some delay.
 */
static void batch_mode(struct process_list* proc_list, screen_t* screen)
{
  int   num_iter = 0;
  int   num_printed;
  int   pos;
  FILE* out = options.out;
  struct process** p;

  tv.tv_sec = 0;
  tv.tv_usec = 200000;  /* 200 ms for first iteration */

  /* Print various information about this run */
  fprintf(out, "tiptop - ");
  fflush(out);

  { /* uptime */
    FILE* f;
    float val1, val5, val15, up;
    int days, hours, minutes, n;
    f = fopen("/proc/loadavg", "r");
    n = fscanf(f, "%f %f %f", &val1, &val5, &val15);
    if (n != 3)
      val1 = val5 = val15 = 0.0;  /* something went wrong, no sure what */
    fclose(f);
    f = fopen("/proc/uptime", "r");
    n = fscanf(f, "%f", &up);
    if (n != 1)
      up = 0.0;
    fclose(f);
    days = up / 86400;
    hours = (up - days*86400) / 3600;
    minutes = (up - days*86400 - hours*3600) / 60;
    fprintf(out, "up %d days, %d:%02d, load average: %.2f, %.2f, %.2f\n",
            days, hours, minutes, val1, val5, val15);
  }

  { /* date */
    char outstr[200];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    if (tmp) {
      strftime(outstr, sizeof(outstr), "%a %b %e %H:%M:%S %Z %Y", tmp);
      fprintf(out, "%s\n", outstr);
    }
  }

  pos = screen_pos(screen);

  fprintf(out, "delay: %.2f  idle: %d  threads: %d\n",
          options.delay, (int)options.idle, (int)options.show_threads);
  if (options.watch_pid)
    fprintf(out, "watching pid %d\n", options.watch_pid);
  else if (options.watch_name)
    fprintf(out, "watching pid '%s'\n", options.watch_name);

  if (options.only_pid)
    fprintf(out, "only pid %d\n", options.only_pid);
  else if (options.only_name)
    fprintf(out, "only pid '%s'\n", options.only_name);

  if (options.watch_uid != -1) {
    struct passwd* passwd = getpwuid(options.watch_uid);
    assert(passwd);
    fprintf(out, "watching uid %d '%s'\n", options.watch_uid, passwd->pw_name);
  }

  header = gen_header(screen, &options, TXT_LEN - 1, active_col);

  fprintf(out, "Screen %d: %s\n", pos, screen->name);
  fprintf(out, "\n%s\n", header);

  for(num_iter=0; !options.max_iter || num_iter<options.max_iter; num_iter++) {
    unsigned int epoch = 0;
    int i, num_dead;

    /* update the list of processes/threads and accumulate info if needed */
    if (options.show_epoch)
      epoch = time(NULL);

    num_dead = update_proc_list(proc_list, screen, &options);

    if (!options.show_threads)
      accumulate_stats(proc_list);

    p = proc_list->proc_ptrs;

    /* generate the text version of all rows */
    build_rows(proc_list, screen, -1);

    /* sort by %CPU */
    qsort(p, proc_list->num_tids, sizeof(struct process*), sorting_fun);

    num_printed = 0;
    for(i=0; i < proc_list->num_tids; i++) {

      if (p[i]->skip)
        continue;

      if (options.show_threads || (p[i]->pid == p[i]->tid)) {
        if (options.show_timestamp)
          fprintf(out, "%6d ", num_iter);
        if (options.show_epoch)
          fprintf(out, "%10u ", epoch);
        fprintf(out, "%s%s", p[i]->txt, p[i]->dead ? " DEAD" : "");

        /* if the process is being watched */
        if ((p[i]->tid == options.watch_pid) ||
            (options.watch_name && options.show_cmdline &&
             strstr(p[i]->cmdline, options.watch_name)) ||
            (options.watch_name && !options.show_cmdline &&
                                  strstr(p[i]->name, options.watch_name)))
          fprintf(out, " <---");
        fprintf(out, "\n");
        num_printed++;
      }
    }

    if (num_printed)
      fprintf(out, "\n");
    fflush(out);

    if (options.command_done && options.sticky)
      break;

    if ((num_dead) && (!options.sticky))
      compact_proc_list(proc_list);

    /* Wait some delay. Note that this syscall may be interrupted when
       we receive a signal, such as SICHLD. This is ok, it will force
       a refresh. */
    select(0, NULL, NULL, NULL, &tv);

    /* prepare for next select */
    tv.tv_sec = options.delay;
    tv.tv_usec = (options.delay - tv.tv_sec) * 1000000.0;
  }
  free(header);
}


#ifdef HAVE_LIBCURSES
/* Handle a key press.  Assumes that a key has been pressed and is
 * ready to read (will block otherwise).
 * Return 1 if 'q' (quit).
 */
static int handle_key()
{
  int c;

  c = getch();
  if (c == 'q')
    ; /* nothing */
  else if (c == 'g')
    options.debug = 1 - options.debug;

  else if (c == 'c')
    options.show_cmdline = 1 - options.show_cmdline;

  else if ((c == 'd') || (c == 's')) {
    mvprintw(2, 0, "Change delay from %.2f to: ", options.delay);
    echo();
    nocbreak();
    scanw("%f", &options.delay);
    if (options.delay < 0.1)
      options.delay = 1.0;
    tv.tv_sec = options.delay;
    tv.tv_usec = (options.delay - tv.tv_sec)*1000000.0;
    cbreak();
    noecho();
  }

  else if (c == 'H') {
    options.show_threads = 1 - options.show_threads;
  }

  else if (c == 'i')
    options.idle = 1 - options.idle;

  else if (c == 'K') {
    if (options.show_kernel) {
      options.show_kernel = 0;
      message = "Kernel mode Off";
    }
    else {
      if (options.euid == 0) {
        options.show_kernel = 1;
        message = "Kernel mode On";
      }
      else {
        message = "Kernel mode only available to root.";
        c = ' ';  /* do not return the 'K' to the upper level, since
                     it was ignored. */
      }
    }
  }


  else if (c == 'k') {  /* initialize string to 0s */
    char str[10] = { 0 };
    int  kill_pid, kill_sig, kill_res;
    mvprintw(2, 0, "PID to kill: ");
    echo();
    nocbreak();
    getnstr(str, sizeof(str) - 1);
    if (!isdigit(str[0]))
      message = "Not valid";
    else {
      kill_pid = atoi(str);
      mvprintw(2, 0, "Kill PID %d with signal [15]: ", kill_pid);
      getnstr(str, sizeof(str) - 1);
      kill_sig = atoi(str);
      if (kill_sig == 0)
        kill_sig = 15;
      kill_res = kill(kill_pid, kill_sig);
      if (kill_res == -1) {
        char tmp_message[100];
        snprintf(tmp_message, sizeof(tmp_message),
                 "Kill of PID '%d' with '%d' failed: %s",
                 kill_pid, kill_sig, strerror(errno));
        message = tmp_message;
      }
    }
    cbreak();
    noecho();
  }

  else if (c == 'p') {
    char str[100] = { 0 };  /* initialize string to 0s */
    mvprintw(2, 0, "Only display process: ");
    echo();
    nocbreak();
    getnstr(str, sizeof(str)-1);  /* keep final '\0' as string delimiter */
    options.only_pid = atoi(str);
    if (options.only_name) {
      free(options.only_name);
      options.only_name = NULL;
    }
    if (!options.only_pid && strcmp(str, "")) {
      options.only_name = strdup(str);
    }
    cbreak();
    noecho();
  }

  else if (c == 'R')
    sorting_order = (enum sorting_order) (1 - (int)sorting_order);

  else if (c == 'S')
    options.sticky = 1 - options.sticky;

  else if (c == 'U')
    options.show_user = 1 - options.show_user;

  else if (c == 'u') {
    char str[100] = { 0 };  /* initialize string to 0s */
    mvprintw(2, 0, "Which user (blank for all): ");
    echo();
    nocbreak();
    getnstr(str, sizeof(str) - 1);  /* keep final '\0' as string delimiter */
    cbreak();
    noecho();
    if (str[0] == '\0')  /* blank */
      options.watch_uid = -1;
    else if (isdigit(str[0]))
      options.watch_uid = atoi(str);
    else {
      struct passwd*  passwd;
      passwd = getpwnam(str);
      if (passwd)
        options.watch_uid = passwd->pw_uid;
      else
        message = "User name does not exist.";
    }
  }

  else if (c == 'w') {
    char str[100] = { 0 };  /* initialize string to 0s */
    mvprintw(2, 0, "Watch process: ");
    echo();
    nocbreak();
    getnstr(str, sizeof(str)-1);  /* keep final '\0' as string delimiter */
    options.watch_pid = atoi(str);
    if (options.watch_name) {
      free(options.watch_name);
      options.watch_name = NULL;
    }
    if (!options.watch_pid && strcmp(str, "")) {
      options.watch_name = strdup(str);
    }
    cbreak();
    noecho();
  }

  else if (c == 'h')
    options.help = 1 - options.help;

  else if (c == 'W') {
    if (export_screens(&options) < 0)
      message = ".tiptoprc not written: already exists in current directory?";
    else
      message = ".tiptoprc written";
  }

  else if (c == KEY_UP)
    scroll_up();
  else if (c == KEY_PPAGE)
    scroll_page_up();
  else if (c == KEY_DOWN)
    scroll_down();
  else if (c == KEY_NPAGE)
    scroll_page_down();
  else if (c == KEY_HOME)
    scroll_home();
  else if (c == KEY_END)
    scroll_end();

  return c;
}


/* Main execution loop in live mode. Builds the list of processes,
 * collects statistics, and prints using curses. Repeats after some
 * delay, also catching key presses.
 */
static int live_mode(struct process_list* proc_list, screen_t* screen)
{
  WINDOW*         help_win = NULL;
  WINDOW*         error_win = NULL;
  fd_set          fds;
  struct process** p;
  int             num_iter = 0;
  int             with_colors = 0;
  int             pos;

  /* start curses */
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  /* Prepare help window */
  help_win = prepare_help_win(screen);

  if (has_colors()) {
    /* initialize curses colors */
    with_colors = 1;
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(5, COLOR_RED, COLOR_BLACK);
    attron(COLOR_PAIR(0));
  }

  tv.tv_sec = 0;
  tv.tv_usec = 200000; /* 200 ms for first iteration */

  header = gen_header(screen, &options, COLS - 1, active_col);

  pos = screen_pos(screen);

  for(num_iter=0; !options.max_iter || num_iter<options.max_iter; num_iter++) {
    int  i, zz, printed, num_fd, num_dead;

    /* print various info */
    erase();
    mvprintw(0, 0, "tiptop -");

    if ((num_errors() > 0) && (COLS >= 37))
      mvprintw(LINES-1, 30, "[errors]");
    if ((options.config_file == 1) && (COLS >= 60))
      mvprintw(0, COLS-60, "[conf]");
    if ((options.euid == 0) && (COLS >= 54))
      mvprintw(0, COLS-54, "[root]");
    if ((options.watch_uid != -1) && (COLS >= 48))
      mvprintw(0, COLS-48, "[uid]");
    if ((options.only_pid || options.only_name) && (COLS >= 43))
      mvprintw(0, COLS-43, "[pid]");
    if (options.show_kernel && (COLS >= 38))
      mvprintw(0, COLS-38, "[kernel]");
    if (options.sticky && (COLS >= 30))
      mvprintw(0, COLS-30, "[sticky]");
    if (options.show_threads && (COLS >= 22))
      mvprintw(0, COLS-22, "[threads]");
    if (options.idle && (COLS >= 13))
      mvprintw(0, COLS-13, "[idle]");
    if (options.debug && (COLS >= 7))
      mvprintw(0, COLS-7, "[debug]");

    if (options.show_epoch && (COLS >= 18))
      mvprintw(LINES-1, COLS-18, "Epoch: %u", time(NULL));

    if (options.show_timestamp)
      mvprintw(LINES-1, 0, "Iteration: %u", num_iter);

    /* print main header */
    if (with_colors)
      attron(COLOR_PAIR(1));
    mvprintw(3, 0, "%s", header);
    for(zz=strlen(header); zz < COLS-1; zz++)
      printw(" ");
    printw("\n");
    if (with_colors)
      attroff(COLOR_PAIR(1));

    /* update the list of processes/threads and accumulate info if needed */
    num_dead = update_proc_list(proc_list, screen, &options);

    if (!options.show_threads)
      accumulate_stats(proc_list);

    p = proc_list->proc_ptrs;

    /* prepare for select */
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    /* generate the text version of all rows */
    build_rows(proc_list, screen, COLS - 1);

    /* sort by %CPU */
    qsort(p, proc_list->num_tids, sizeof(struct process*), sorting_fun);

    printed = 0;

    /* Iterate over all threads */
    for(i=0; i < proc_list->num_tids; i++) {

      if (p[i]->skip)
        continue;

      /* highlight watched process, if any */
      if (with_colors) {
        if (p[i]->dead) {
          attron(COLOR_PAIR(5));
        }
        else if ((p[i]->tid == options.watch_pid) ||
                 (options.watch_name && options.show_cmdline &&
                                strstr(p[i]->cmdline, options.watch_name)) ||
                 (options.watch_name && !options.show_cmdline &&
                                strstr(p[i]->name, options.watch_name)))
          attron(COLOR_PAIR(3));
      }

      if (options.show_threads || (p[i]->pid == p[i]->tid)) {
        printw("%s\n", p[i]->txt);
        printed++;
      }

      if (with_colors)
        attroff(COLOR_PAIR(3));

      if (printed >= LINES - 5)  /* stop printing at bottom of window */
        break;
    }

    mvprintw(1, 0, "Tasks: %3d total, %3d displayed",
             proc_list->num_tids, printed);
    if (options.sticky)
      printw(", %3d dead", num_dead);

    /* print the screen name, make sure it fits, or truncate */
    if (with_colors)
      attron(COLOR_PAIR(4));
    if (35 + 20 + 11 + strlen(screen->name) < COLS) {
      mvprintw(1, COLS - 11 - strlen(screen->name),
               "screen %2d: %s\n", pos, screen->name);
    }
    else if (COLS >= 35 + 20 + 11) {
      char screen_str[50] = { 0 };
      snprintf(screen_str, sizeof(screen_str) - 1, "%s\n", screen->name);
      screen_str[COLS - 35 - 20 - 11] = '\0';  /* truncate */
      mvprintw(1, 35+20, "screen %2d: %s", pos, screen_str);
    }
    if (with_colors)
      attroff(COLOR_PAIR(4));

    /* print message if any */
    if (message) {
      if (with_colors)
        attron(COLOR_PAIR(1));
      mvprintw(2, 0, "%s", message);
      if (with_colors)
        attroff(COLOR_PAIR(1));
      message = NULL;  /* reset message */
    }

    refresh();  /* display everything */
    if (options.error) {
      if (options.error == 1) {
        options.error = 2;
        show_error_win(error_win, printed);
      }
      else
        show_error_win(error_win, -1);
    }
    if (options.help)
      show_help_win(help_win, screen);

    if ((num_dead) && (!options.sticky))
      compact_proc_list(proc_list);

    /* wait some delay, or until a key is pressed */
    num_fd = select(1 + STDIN_FILENO, &fds, NULL, NULL, &tv);
    if (num_fd > 0) {
      int c = handle_key();
      if (c == 'q')
        break;
      if (c == '>') {
        if (active_col < screen->num_columns )
          active_col++;
        free(header);
        header = gen_header(screen, &options, COLS - 1, active_col);
      }
      if (c == '<') {
        if (active_col > -1)
          active_col--;
        free(header);
        header = gen_header(screen, &options, COLS - 1, active_col);
      }
      if (c == 'H') {
        if (options.show_threads) {
          reset_values(proc_list);
          message = "Show threads On";
        }
        else
          message = "Show threads Off";
      }
      if (c == 'U') {
        free(header);
        header = gen_header(screen, &options, COLS - 1, active_col);
      }
      if ((c == '+') || (c == '-') || (c == KEY_LEFT) || (c == KEY_RIGHT))
        return c;

      if ((c == 'u') || (c == 'K') || (c == 'p')) /* need to rebuild tasks list */
        return c;

      if (c == 'e') {
        if (options.error > 0) {
          options.error = 0;
          delwin(error_win);
          error_win = NULL;
        }
        else
          options.error = 1;
      }
    }
    tv.tv_sec = options.delay;
    tv.tv_usec = (options.delay - tv.tv_sec) * 1000000.0;
  }

  free(header);

  delwin(help_win);

  endwin();  /* stop curses */
  return 'q';
}
#endif  /* HAVE_LIBCURSES */


int main(int argc, char* argv[])
{
  char* path_to_config;
  int key = 0;
  int list_scr = 0;
  struct process_list* proc_list;
  screen_t* screen = NULL;
  int screen_num = 0;
  int q;

  /* Check OS to make sure we can run. */
  check();

  init_options(&options);

  path_to_config = get_path_to_config(argc, argv);
  q = read_config(path_to_config, &options);
  if (q == 0) {
    debug_printf("Config file successfully parsed.\n");
    options.config_file = 1;
  }
  else
    debug_printf("Could not parse config file.\n");

  /* Parse command line arguments. */
  parse_command_line(argc, argv, &options, &list_scr, &screen_num);

  init_errors(options.batch, options.path_error_file);

  /* Add default screens */
  if (options.default_screen == 1)
    init_screen();

  /* Remove unused but declared counters */
  tamp_counters();

  if (list_scr) {
    list_screens();
    delete_screens();
    exit(0);
  }

  if (options.spawn_pos)
    spawn(argv + options.spawn_pos);

  do {
    if (screen_num >= 0)
      screen = get_screen(screen_num);
    else
      screen = get_screen_by_name(argv[-screen_num]);

    if (!screen) {
      fprintf(stderr, "No such screen.\n");
      exit(EXIT_FAILURE);
    }

    /* initialize the list of processes, and then run */
    proc_list = init_proc_list();

    if (options.spawn_pos) {
      options.spawn_pos = 0;  /* do this only once */
      new_processes(proc_list, screen, &options);
      start_child();
    }

    if (options.batch) {
      batch_mode(proc_list, screen);
      key = 'q';
    }
#ifdef HAVE_LIBCURSES
    else {
      key = live_mode(proc_list, screen);
      if ((key == '+')  || (key == KEY_RIGHT)) {
        screen_num = (screen_num + 1) % get_num_screens();
        active_col = 0;
        done_proc_list(proc_list);
        free(header);
      }
      if ((key == '-') || (key == KEY_LEFT)) {
        int n = get_num_screens();
        screen_num = (screen_num + n - 1) % n;
        active_col = 0;
        done_proc_list(proc_list);
        free(header);
      }
      if ((key == 'u') || (key == 'K') || (key == 'p')) {
        done_proc_list(proc_list);
      }
    }
#endif
  } while (key != 'q');

  /* done, free memory (makes valgrind happy) */
  close_error();
  delete_screens();
  done_proc_list(proc_list);
  free_options(&options);
  return 0;
}
