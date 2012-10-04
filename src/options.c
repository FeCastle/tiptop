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

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "options.h"
#include "version.h"


static void usage(const char* name)
{
  fprintf(stderr, "Usage: %s [option]\n", name);
#ifdef HAVE_LIBCURSES
  fprintf(stderr, "\t-b             run in batch mode\n");
#else
  fprintf(stderr, "\t-b             ignored, for compatibility with batch mode\n");
#endif
  fprintf(stderr, "\t-c             use command line instead of process name\n");
  fprintf(stderr, "\t--cpu-min m    minimum %%CPU to display a process\n");
  fprintf(stderr, "\t-d delay       delay in seconds between refreshes\n");
  fprintf(stderr, "\t-E filename    file where errors are logged\n");
  fprintf(stderr, "\t--epoch        add epoch at beginning of each line\n");
#ifdef ENABLE_DEBUG
  fprintf(stderr, "\t-g             debug\n");
#endif
  fprintf(stderr, "\t-h --help      print this message\n");
  fprintf(stderr, "\t-H             show threads\n");
  fprintf(stderr, "\t-K --kernel    show kernel activity (only for root)\n");
  fprintf(stderr, "\t-i             also display idle processes\n");
  fprintf(stderr, "\t--list-screens display list of available screens\n");
  fprintf(stderr, "\t-n num         max number of refreshes\n");
  fprintf(stderr, "\t-o outfile     output file in batch mode\n");
  fprintf(stderr, "\t--only-conf    Disable default screen, only configuration\n");
  fprintf(stderr, "\t-p --pid pid|name  only display task with this PID/name\n");
  fprintf(stderr, "\t-S num         screen number to display\n");
  fprintf(stderr, "\t--sticky       keep final status of dead processes\n");
  fprintf(stderr, "\t--timestamp    add timestamp at beginning of each line\n");
  fprintf(stderr, "\t-u userid      only show user's processes\n");
  fprintf(stderr, "\t-U             show user name\n");
  fprintf(stderr, "\t-v             print version and exit\n");
  fprintf(stderr, "\t--version      print legalese and exit\n");
  fprintf(stderr, "\t-W path        Used configuration file pointed by path\n");
  fprintf(stderr, "\t-w pid|name    watch this process (highlighted)\n");
  return;
}


void init_options(struct option* opt)
{
  /* default status for options */
  memset(opt, 0, sizeof(*opt));
#ifndef HAVE_LIBCURSES
  /* make batch mode default if curses is not available */
  opt->batch = 1;
#endif
  opt->cpu_threshold = 0.00001;
  opt->default_screen = 1;
  opt->delay = 2;
  opt->euid = geteuid();
  opt->out = stdout;
  opt->watch_uid = -1;
  opt->error = 0;
  opt->path_error_file = NULL;
}

void free_options(struct option* options)
{
  if (options->path_error_file)
    free(options->path_error_file);
  if (options->watch_name)
    free(options->watch_name);
  if (options->only_name)
    free(options->only_name);
}


/* Look for a user specified path in the command line for the
   configuration file (flag -W). Return it if found, otherwise return
   NULL. */
char* get_path_to_config(int argc, char* argv[])
{
  int i;

  for(i=1; i < argc; i++) {
    if (strcmp(argv[i], "-W") == 0) {
      if (i+1 < argc)
        return argv[i+1];
      else {
        fprintf(stderr, "Missing file name after -W.\n");
        exit(EXIT_FAILURE);
      }
    }
  }
  return NULL;
}


void parse_command_line(int argc, char* argv[],
                        struct option* const options,
                        int* list_scr,
                        int* screen_num)
{
  int i;

  /* Note: many flags are toggles. They invert what is in the
     configuration file. */
  for(i=1; i < argc; i++) {

    if (strcmp(argv[i], "--") == 0) {  /* command to be spawned by tiptop */
      if (argc > i+1)  /* at least something after -- */
        options->spawn_pos = i+1;
      else {
        fprintf(stderr, "No command after --, aborting.\n");
        exit(EXIT_FAILURE);
      }
      break;
    }

    if (strcmp(argv[i], "-b") == 0) {
#ifdef HAVE_LIBCURSES
      options->batch = 1 - options->batch;
#endif
      continue;
    }

    if (strcmp(argv[i], "-c") == 0) {
      options->show_cmdline = 1 - options->show_cmdline;
      continue;
    }

    if (strcmp(argv[i], "--cpu-min") == 0) {
      if (i+1 < argc) {
        options->cpu_threshold = (float)atof(argv[i+1]);
        i++;
        continue;
      }
      else {
        fprintf(stderr, "Missing value after --cpu-min.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-d") == 0) {
      if (i+1 < argc) {
        options->delay = (float)atof(argv[i+1]);
        if (options->delay < 0.1)
          options->delay = 1;
        i++;
        continue;
      }
      else {
        fprintf(stderr, "Missing delay after -d.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-E") == 0) {
      if (i+1 < argc) {
        options->path_error_file = strdup(argv[i+1]);
        i++;
        continue;
      }
      else {
        fprintf(stderr, "Missing file name after -E.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "--epoch") == 0) {
      options->show_epoch = 1 - options->show_epoch;
      continue;
    }

    if (strcmp(argv[i], "-g") == 0) {
#ifdef ENABLE_DEBUG
      options->debug = 1 - options->debug;
      continue;
#else
      fprintf(stderr, "Debug not supported (try configure --enable-debug)\n");
#endif
    }

    if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
      usage(argv[0]);
      exit(0);
    }

    if (strcmp(argv[i], "-H") == 0) {
      options->show_threads = 1 - options->show_threads;
      continue;
    }

    if (strcmp(argv[i], "-i") == 0) {
      options->idle = 1 - options->idle;
      continue;
    }

    if ((strcmp(argv[i], "-K") == 0) || (strcmp(argv[i], "--kernel") == 0)) {
      if (options->euid == 0) {
        options->show_kernel = 1 - options->show_kernel;
        continue;
      }
      else {
        fprintf(stderr, "Kernel mode (-K --kernel) not available.\n");
        fprintf(stderr, "You are not root, or the binary is not setuid.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "--list-screens") == 0) {
      *list_scr = 1;
      continue;
    }

    if (strcmp(argv[i], "-n") == 0) {
      if (i+1 < argc) {
        options->max_iter = atoi(argv[i+1]);
        i++;
        continue;
      }
      else {
        fprintf(stderr, "Missing number of iterations after -n.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-o") == 0) {
      if (i+1 < argc) {
        int euid = geteuid();
        int res = seteuid(getuid());  /* temporarily drop privileges */
        if (res != 0) {
          /* do not proceed as root */
          fprintf(stderr, "Could not create output file\n");
          exit(EXIT_FAILURE);
        }
        options->out = fopen(argv[i+1], "w");
        if (!options->out) {
          perror("fopen");
          fprintf(stderr, "Could not open '%s'\n", argv[i+1]);
          exit(EXIT_FAILURE);
        }
        seteuid(euid);  /* restore privileges */
        i++;
        continue;
      }
      else {
        fprintf(stderr, "Missing filename after -o.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "--only-conf") == 0) {
      options->default_screen = 0;
      continue;
    }

    if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--pid") == 0)) {
      if (i+1 < argc) {
        options->only_pid = atoi(argv[i+1]);
        if (options->only_pid == 0)
          options->only_name = strdup(argv[i+1]);
        i++;
        continue;
      }
      else {
        fprintf(stderr, "Missing pid/name after -p.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-S") == 0) {
      if (i+1 < argc) {
        char* endptr;
        errno = 0;
        *screen_num = strtol(argv[i+1], &endptr, 10);
        if (errno  || (endptr == argv[i+1])) {
          *screen_num = -(i+1);  /* position of the argv to read later */
        }
        i++;
        continue;
      }
      else {
        fprintf(stderr, "Missing screen number after -S.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "--sticky") == 0) {
      options->sticky = 1 - options->sticky;
      continue;
    }

    if (strcmp(argv[i], "--timestamp") == 0) {
      options->show_timestamp = 1 - options->show_timestamp;
      continue;
    }

    if (strcmp(argv[i], "-U") == 0) {
      options->show_user = 1 - options->show_user;
      continue;
    }

    if (strcmp(argv[i], "-u") == 0) {
      if (i+1 < argc) {
        if (isdigit(argv[i+1][0])) {
          options->watch_uid = atoi(argv[i+1]);
        }
        else {
          struct passwd* passwd = getpwnam(argv[i+1]);
          if (!passwd) {
            fprintf(stderr, "User name '%s' does not exist.\n", argv[i+1]);
            exit(EXIT_FAILURE);
          }
          options->watch_uid = passwd->pw_uid;
        }
        i++;
        continue;
      }
      else {
        fprintf(stderr, "Missing user name after -u.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-v") == 0) {
      print_version();
      exit(0);
    }

    if (strcmp(argv[i], "--version") == 0) {
      print_legal();
      exit(0);
    }

    if (strcmp(argv[i], "-w") == 0) {
      if (i+1 < argc) {
        options->watch_pid = atoi(argv[i+1]);
        if (options->watch_pid == 0)
          options->watch_name = strdup(argv[i+1]);
        i++;
        continue;
      }
      else {
        fprintf(stderr, "Missing pid/name after -w.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-W") == 0) {
      /* error case already handled by previous function */
      i++;
      continue;
    }

    if (strstr(argv[0], "ptiptop")) {
      /* in case we are ptiptop, handle this argument as in tiptop's -p */
      options->only_pid = atoi(argv[i]);
      if (options->only_pid == 0)
        options->only_name = strdup(argv[i]);
    }
    else {
      fprintf(stderr, "Unknown flag: '%s'\n", argv[i]);
      exit(EXIT_FAILURE);
    }
  }
}
