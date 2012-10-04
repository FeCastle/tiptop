/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "options.h"
#include "process.h"
#include "spawn.h"

static int pipefd[2];
static pid_t my_child = 0;
static int updated = 0;


static void alarm_handler(int sig)
{
  assert(sig == SIGALRM);
  if (!updated) {
    update_name_cmdline(my_child, 0);
    updated = 1;
  }
}

static void child_handler(int sig)
{
  assert(sig == SIGCHLD);
  /* Short running processes may terminate before the timer
     expires. We attempt to update the name now. The command line is
     no longer available for zombies (see man proc). */
  if (!updated) {
    update_name_cmdline(my_child, 1);
    updated = 1;
  }

  /* Do nothing special. We only want the signal to be delivered. It
     will interrupt the 'select' in the main loop (batch/live mode),
     and force an immediate refresh. If 'sticky' mode is on, we also
     quit immediately. */
}


/* Fork a new process, wait for the signal, and execute the command
   passed in parameter. */
void spawn(char** argv)
{
  pid_t    child;
  sigset_t sigs;
  struct sigaction alarm_action, child_action;

  sigemptyset(&sigs);

  alarm_action.sa_handler = alarm_handler;
  alarm_action.sa_flags = 0;
  alarm_action.sa_mask = sigs;
  sigaction(SIGALRM, &alarm_action, NULL);  /* prepare to receive timer */

  child_action.sa_handler = child_handler;
  child_action.sa_flags = 0;
  child_action.sa_mask = sigs;
  sigaction(SIGCHLD, &child_action, NULL); /* prepare to receive SIGCHLD */

  if (pipe(pipefd)) {  /* parent will signal through the pipe when ready */
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  child = fork();
  if (child == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (child == 0) {  /* in the child */
    int  n;
    char buffer;

    close(pipefd[1]);  /* useless */

    /* in case tiptop is set-UID root, we drop privileges in the child */
    if ((geteuid() == 0) && (getuid() != 0)) {
      int res = setuid(getuid());
      if (res != 0) {
        /* do not proceed as root */
        fprintf(stderr, "Could not create command\n");
        exit(EXIT_FAILURE);
      }
    }

    /* wait for parent to signal */
    n = read(pipefd[0], &buffer, 1);  /* blocking read */
    if (n != 1)
      fprintf(stderr, "Something went wrong with the command\n");
    close(pipefd[0]);

    if (execvp(argv[0], argv) == -1) {
      perror("execvp");
      exit(EXIT_FAILURE);
    }
  }

  /* parent */
  close(pipefd[0]);
  my_child = child;
}


/* After the command has been forked, we update the list of
   processes. The newly created process will be discovered and
   hardware counters attached. We then signal the child to proceed
   with the execvp. */
void start_child()
{
  struct timeval tv1, tv2 = { 0 };
  struct itimerval it;
  int n;

  n = write(pipefd[1], "!", 1);  /* write whatever, will unblock the read */
  if (n != 1)
    fprintf(stderr, "Something went wrong with the command...\n");

  close(pipefd[1]);

  /* set timer, we need to update the name and command line of the new
     process in a little while */
  tv1.tv_sec = 0;
  tv1.tv_usec = 200000;  /* 200 ms */
  it.it_value = tv1;
  it.it_interval = tv2;  /* zero, no repeat */
  setitimer(ITIMER_REAL, &it, NULL);
}


void wait_for_child(pid_t pid, struct option* options)
{
  /* only wait for my child */
  if (pid != my_child)
    return;

  wait(NULL);  /* release system resources */
  options->command_done = 1;
}
