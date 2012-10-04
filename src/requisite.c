/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "pmc.h"
#include "requisite.h"


void check()
{
  int fd, cpu, grp, flags, pid;
  struct utsname os;
  struct STRUCT_NAME events = {0, };

  events.disabled = 0;
  events.exclude_hv = 1;
  events.exclude_kernel = 1;
  /* try basic event: cycles */
  events.type = PERF_TYPE_HARDWARE;
  events.config = PERF_COUNT_HW_CPU_CYCLES;

  cpu = -1;  /* CPU to monitor, -1 = per thread */
  grp = -1;
  flags = 0;
  pid = 0;   /* self */
  fd = sys_perf_counter_open(&events, pid, cpu, grp, flags);
  if (fd == -1) {
    perror("syscall");
    fprintf(stderr, "Could not perform syscall.\n");
    uname(&os);
    if (strcmp(os.sysname, "Linux") != 0) {
      fprintf(stderr, "Is this OS a Linux (OS identifies itself as '%s').\n",
              os.sysname);
    }
    else if (strcmp(os.release, "2.6.31") < 0) {  /* lexicographic order */
      fprintf(stderr, "Linux 2.6.31+ is required, OS reports '%s'.\n",
              os.release);
    }
    else {
      fprintf(stderr, "Don't know why...\n");
    }
    exit(EXIT_FAILURE);
  }
  
  close(fd);
}
