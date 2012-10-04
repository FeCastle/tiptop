/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _PMC_H
#define _PMC_H

#include <config.h>
/*
 * The sys_perf_counter_open syscall and header files changed names
 * between Linux 2.6.31 and 2.6.32. Do the mangling here. */
#ifdef HAVE_LINUX_PERF_COUNTER_H
#include <linux/perf_counter.h>
#define STRUCT_NAME perf_counter_attr

#elif HAVE_LINUX_PERF_EVENT_H
#include <linux/perf_event.h>
#define STRUCT_NAME perf_event_attr

#else
#error Sorry, performance counters not supported on this system.
#endif

#include <sys/types.h>


/* Declare the syscall with proper naming. */
long sys_perf_counter_open(struct STRUCT_NAME *hw_event,
                           pid_t pid,
                           int cpu,
                           int group_fd,
                           unsigned long flags);

#endif  /* _PMC_H */
