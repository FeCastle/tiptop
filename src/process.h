/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _PROCESS_H
#define _PROCESS_H

#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

#include "screen.h"


#define MAX_EVENTS 16
#define TXT_LEN   200  /* max size of the text representation (or row) */


/* An instance of this union is owned by each process. It is used to
   sort rows according to the active column. */
union sorting_column {
  double   d;
  uint64_t l;
  int      i;
  char     c;
};


/* Main structure describing a thread */
struct process {
  pid_t    tid;           /* thread ID */
  pid_t    pid;           /* process ID. For owning process, tip == pid */
  short    proc_id;       /* processor ID on which process was last seen */
  short    num_threads;   /* number of threads in brotherhood */
  int      num_events;

  double   cpu_percent;   /* %CPU as displayed by top */
  double   cpu_percent_s; /* %CPU system */
  double   cpu_percent_u; /* %CPU user */

  struct timeval timestamp;         /* timestamp of last update */
  unsigned long prev_cpu_time_s;    /* system */
  unsigned long prev_cpu_time_u;    /* user */

  int       fd[MAX_EVENTS];           /* file handles */
  uint64_t  values[MAX_EVENTS];       /* values read from counters */
  uint64_t  prev_values[MAX_EVENTS];  /* previous iteration */
  char* txt;  /* text representation of the process (what is displayed) */

  union sorting_column u;

  char* username;
  char* cmdline;       /* command line */
  char* name;          /* name of process */

  unsigned int dead : 1;  /* is the process dead? */
  unsigned int skip : 1;  /* do not display, for any reason (dead, idle...) */
#if 0
  unsigned int attention : 1;
#endif
  struct process* next;
};


/* List of processes/threads */
struct process_list {
  int  num_tids;
  int  num_alloc;
  pid_t most_recent_pid;

  struct process* processes;
  struct process** proc_ptrs;
};


struct process_list* init_proc_list();
void done_proc_list(struct process_list*);
void new_processes(struct process_list* const list,
                   const screen_t* const screen,
                   const struct option* const options);
int  update_proc_list(struct process_list* const,
                      const screen_t* const,
                      struct option* const);
void compact_proc_list(struct process_list* const);
void accumulate_stats(const struct process_list* const);
void reset_values(const struct process_list* const);

void update_name_cmdline(int pid, int name_only);

#endif  /* _PROCESS_H */
