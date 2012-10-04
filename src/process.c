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
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "error.h"
#include "hash.h"
#include "options.h"
#include "pmc.h"
#include "process.h"
#include "screen.h"
#include "spawn.h"

static int num_files = 0;
static int num_files_limit = 0;

static int   clk_tck;


/*
 * Build the (empty) list of processes/threads.
 */
struct process_list* init_proc_list()
{
  struct process_list* l;
  char  name[100] = { 0 };  /* needs to fit the name /proc/xxxx/limits */
  char  line[100];
  FILE* f;

  clk_tck = sysconf(_SC_CLK_TCK);

  l = malloc(sizeof(struct process_list));
  l->processes = NULL;
  l->num_alloc = 20;
  l->proc_ptrs = malloc(l->num_alloc * sizeof(struct process*));
  l->num_tids = 0;
  l->most_recent_pid = 0;

  hash_init();

  num_files_limit = 0;
  snprintf(name, sizeof(name) - 1, "/proc/%d/limits", getpid());
  f = fopen(name, "r");
  if (f) {
    while (fgets(line, 100, f)) {
      int n;
      n = sscanf(line, "Max open files %d", &num_files_limit);
      if (n)
        break;
    }
    fclose(f);
  }

  num_files_limit -= 10; /* keep some slack */
  if (num_files_limit == 0)  /* something went wrong */
    num_files_limit = 200;  /* reasonable default? */
  num_files = 0;
  return l;
}


/* Free memory for all fields of the process. */
static void done_proc(struct process* const p)
{
  int val_idx;
  
  if (p->cmdline)
    free(p->cmdline);
  free(p->name);
  free(p->txt);
  if (p->username)
    free(p->username);

  for(val_idx=0; val_idx < p->num_events; val_idx++) {
    if (p->fd[val_idx] != -1) {
      close(p->fd[val_idx]);
      num_files--;
    }
  }
}


/*
 * Deletes the list of processes. Free memory.
 */
void done_proc_list(struct process_list* list)
{
  struct process* p;

  assert(list && list->proc_ptrs);
  p = list->processes;
  while (p) {
    struct process* old = p;
    done_proc(p);
    p = p->next;
    free(old);
  }

  free(list->proc_ptrs);
  free(list);
  hash_fini();
}


/* Retrieve the command line of the process from
   /proc/PID/cmdline. The subtlety comes from the fact that args are
   separated by '\0', the command line itself by two consecutive '\0'
   (see "man proc"). Some processes have no command line: first
   character is '\0'. */
static char* get_cmdline(int pid, char* result, int size)
{
  FILE* f;
  char  name[50] = { 0 };  /* needs to fit /proc/xxxx/cmdline */
  char* res = NULL;

  snprintf(name, sizeof(name) - 1, "/proc/%d/cmdline", pid);
  f = fopen(name, "r");
  if (f) {    
    res = fgets(result, size, f);
    if (res && res[0]) {
      int j;
      for(j=0; j < size-1; j++) {
        if (result[j] == '\0') {
          if (result[j+1] == '\0')  /* two zeroes in a row, stop */
            break;
          else
            result[j] = ' '; /* only one, it is a separator, it becomes ' ' */
        }
      }
    }
    fclose(f);
  }
  if (!res) {
    strncpy(result, "[null]", size-1);
    result[size-1] = '\0';
  }

  return result;
}


void new_processes(struct process_list* const list,
                   const screen_t* const screen,
                   const struct option* const options)
{
  struct dirent* pid_dirent;
  DIR*           pid_dir;
  int            num_tids, val, n;
  struct STRUCT_NAME events = {0, };
  FILE*          f;
  uid_t          my_uid = -1;

  const int cpu = -1;
  const int grp = -1;
  const int flags = 0;

  /* To avoid scanning the entire /proc directory, we first check if
     any process has been created since last time. /proc/loadavg
     contains the PID of the most recent process. We compare with our
     own most recent. */
  f = fopen("/proc/loadavg", "r");
  n = fscanf(f, "%*f %*f %*f %*d/%*d %d", &val);
  fclose(f);
  /* if no new process has been created since last time, just quit. */
  if ((n == 1) && (val == list->most_recent_pid))
    return;

  list->most_recent_pid = val;

  num_tids = list->num_tids;

  events.disabled = 0;
  events.pinned = 1;
  events.exclude_hv = 1;
  /* events.exclude_idle = 1; ?? */
  if (options->show_kernel == 0)
    events.exclude_kernel = 1;

  /* check all directories of /proc */
  pid_dir = opendir("/proc");
  while ((pid_dirent = readdir(pid_dir))) {
    int   uid, pid, num_threads, req_info;
    char  name[50] = { 0 }; /* needs to fit /proc/xxxx/{status,cmdline} */
    char  line[100]; /* line of /proc/xxxx/status */
    char  proc_name[100];
    int   skip_by_pid, skip_by_user;
    char  cmdline[100];

    if (pid_dirent->d_type != DT_DIR)  /* not a directory */
      continue;

    if ((pid = atoi(pid_dirent->d_name)) == 0)  /* not a number */
      continue;

    snprintf(name, sizeof(name) - 1, "/proc/%d/status", pid);
    f = fopen(name, "r");
    if (!f)
      continue;

    /* collect basic information about process */
    req_info = 3;  /* need 3 pieces of information */
    while (fgets(line, sizeof(line), f) && req_info) {
      /* read the name now, since we will encounter it before Uid anyway */
      if (strncmp(line, "Name:", 5) == 0) {
        sscanf(line, "%*s %s", proc_name);
        req_info--;
      }
      if (strncmp(line, "Uid:", 4) == 0) {
        sscanf(line, "%*s %d", &uid);
        req_info--;
      }
      if (strncmp(line, "Threads:", 8) == 0) {
        sscanf(line, "%*s %d", &num_threads);
        req_info--;
      }
    }
    fclose(f);

    if (req_info != 0) {  /* could not read all 3 info. Process is gone? */
      error_printf("Could not read info for process %d (gone already?)\n", pid);
      continue;
    }

    cmdline[0] = '\0';
    skip_by_pid = 0;
    /* if "only" filter is set */
    if (options->only_pid && (pid != options->only_pid))
      skip_by_pid = 1;

    if (options->only_name) {
      if (options->show_cmdline) {  /* show_cmdline is on */
        get_cmdline(pid, cmdline, sizeof(cmdline));
        if (strstr(cmdline, options->only_name) == NULL) {
          skip_by_pid = 1;
        }
      }
      else {  /* show_cmdline is off */
        if (strstr(proc_name, options->only_name) == NULL)
          skip_by_pid = 1;
      }
    }

    /* my process, or somebody else's process and I am root (skip
       root's processes because they are too many). */
    skip_by_user = 1;
    my_uid = options->euid;
    if (((my_uid != 0) && (uid == my_uid)) ||  /* not root, monitor mine */
        ((my_uid == 0) && (uid != 0)))         /* I am root, monitor all others */
      skip_by_user = 0;

    if ((skip_by_user == 0) && (skip_by_pid == 0)) {
      DIR* thr_dir;
      struct dirent* thr_dirent;
      int  tid;
      char task_name[50] = { 0 };

      snprintf(task_name, sizeof(task_name) - 1, "/proc/%d/task", pid);
      thr_dir = opendir(task_name);
      if (!thr_dir)  /* died just now? Will be marked dead at next iteration. */
        continue;

      /* Iterate over all threads in the process */
      while ((thr_dirent = readdir(thr_dir))) {
        int   zz, fail;
        struct process* ptr;
        struct passwd*  passwd;

        tid = atoi(thr_dirent->d_name);
        if (tid == 0)
          continue;

        ptr = hash_get(tid);
        if (ptr)  /* already known */
          continue;


        /* We have a new thread. */

        /* allocate memory */
        ptr = malloc(sizeof(struct process));

        /* insert into list of processes */
        ptr->next = list->processes;
        list->processes = ptr;

        /* update helper data structures */
        if (num_tids == list->num_alloc) {
          list->num_alloc += 20;
          list->proc_ptrs = realloc(list->proc_ptrs,
                                    list->num_alloc*sizeof(struct process));
        }
        list->proc_ptrs[num_tids] = ptr;
        hash_add(tid, ptr);

        /* fill in information for new process */
        ptr->tid = tid;
        ptr->pid = pid;
        ptr->proc_id = -1;
        ptr->dead = 0;
#if 0
        ptr->attention = 0;
#endif
        ptr->u.d = 0.0;

        passwd = getpwuid(uid);
        if (passwd)
          ptr->username = strdup(passwd->pw_name);
        else
          ptr->username = NULL;

        ptr->num_threads = (short)num_threads;
        if (cmdline[0] == '\0')
          get_cmdline(pid, cmdline, sizeof(cmdline));
        ptr->cmdline = strdup(cmdline);
        ptr->name = strdup(proc_name);
        ptr->timestamp.tv_sec = 0;
        ptr->timestamp.tv_usec = 0;
        ptr->prev_cpu_time_s = 0;
        ptr->prev_cpu_time_u = 0;
        ptr->cpu_percent = 0.0;
        ptr->cpu_percent_s = 0.0;
        ptr->cpu_percent_u = 0.0;

        /* Get number of counters from screen */
        ptr->num_events = screen->num_counters;

        for(zz = 0; zz < ptr->num_events; zz++)
          ptr->prev_values[zz] = 0;

        ptr->txt = malloc(TXT_LEN * sizeof(char));

        fail = 0;
        for(zz = 0; zz < ptr->num_events; zz++) {
          int fd;
          events.type = screen->counters[zz].type;  /* eg PERF_TYPE_HARDWARE */
          events.config = screen->counters[zz].config;

          if (num_files < num_files_limit) {
            fd = sys_perf_counter_open(&events, tid, cpu, grp, flags);
            if (fd == -1) {
              error_printf("Could not attach counter '%s' to PID %d (%s): %s\n",
                           screen->counters[zz].alias,
                           tid,
                           ptr->name,
                           strerror(errno));
            }
          }
          else {
            fd = -1;
            error_printf("Files limit reached for PID %d (%s)\n",
                         tid, ptr->name);
          }

          if (fd == -1)
            fail++;
          else
            num_files++;
          ptr->fd[zz] = fd;
          ptr->values[zz] = 0;
        }

#if 0
        if (fail) {
          /* at least one counter failed, mark it */
          ptr->attention = 1;
        }

        if (fail != ptr->num_events) {
          /* at least one counter succeeded, insert the thread in list */
          list->num_tids++;
          num_tids++;
        }
#endif
        list->num_tids++;  /* insert in any case */
        num_tids++;
      }
      closedir(thr_dir);
    }
  }
  closedir(pid_dir);
}


/*
 * Update all processes in the list with newly collected statistics.
 * Return the number of dead processes.
 */
int update_proc_list(struct process_list* const list,
                     const screen_t* const screen,
                     struct option* const options)
{
  struct process* proc;
  int    num_dead = 0;

  assert(screen);
  assert(list && list->proc_ptrs);

  /* add newly created processes/threads */
  new_processes(list, screen, options);

  /* update statistics */
  for(proc = list->processes; proc; proc = proc->next) {
    FILE*     fstat;
    char      sub_task_name[100] = { 0 };
    double    elapsed;
    unsigned long   utime = 0, stime = 0;
    unsigned long   prev_cpu_time, curr_cpu_time;
    int             proc_id, zz, zombie;
    struct timeval  now;

    if (proc->dead) {
      num_dead++;
      continue;
    }

    /* Compute %CPU, retrieve processor ID. */
    snprintf(sub_task_name, sizeof(sub_task_name) - 1,
             "/proc/%d/task/%d/stat", proc->pid, proc->tid);
    fstat = fopen(sub_task_name, "r");

    if (!fstat) {  /* this task disappeared */
      num_dead++;
      proc->dead = 1;  /* mark dead */
      for(zz=0; zz < proc->num_events; ++zz) {
        if (proc->fd[zz] != -1) {
          close(proc->fd[zz]);
          num_files--;
          proc->fd[zz] = -1;
        }
      }
      continue;
    }

    zombie = 0;
    if (fstat) {
      int n;
      char state;
      n = fscanf(fstat,
           "%*d (%*[^)]) %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
           &state, &utime, &stime);
      if (n != 3)
        utime = stime = 0;

      if (state == 'Z') {  /* zombie */
        zombie = 1;
      }

      /* get processor ID */


      n = fscanf(fstat,
                 "%*d %*d %*d %*d %*d %*d %*d %*u %*d %*u %*u %*u %*u "
                 "%*u %*u %*u %*u %*u %*u %*u %*u %*u %*d %d",
                 &proc_id);
      if (n != 1)
        proc_id = -1;
      fclose(fstat);
    }
    if (!zombie) {
      /* do not update these values for a zombie, they have become invalid */
      gettimeofday(&now, NULL);
      elapsed = (now.tv_sec - proc->timestamp.tv_sec) +
        (now.tv_usec - proc->timestamp.tv_usec)/1000000.0;
      elapsed *= clk_tck;

      proc->timestamp = now;

      prev_cpu_time = proc->prev_cpu_time_s + proc->prev_cpu_time_u;
      curr_cpu_time = stime + utime;
      proc->cpu_percent = 100.0*(curr_cpu_time - prev_cpu_time)/elapsed;
      proc->cpu_percent_s = 100.0*(stime - proc->prev_cpu_time_s)/elapsed;
      proc->cpu_percent_u = 100.0*(utime - proc->prev_cpu_time_u)/elapsed;

      proc->prev_cpu_time_s = stime;
      proc->prev_cpu_time_u = utime;
    }

    proc->proc_id = (short)proc_id;
    /* Backup previous value of counters */
    for(zz = 0; zz < proc->num_events; zz++)
      proc->prev_values[zz] = proc->values[zz];

    /* Read performance counters */
    for(zz = 0; zz < proc->num_events; zz++) {
      uint64_t value = 0;
      int r;
      /* When fd is -1, the syscall failed on that counter */
      if (proc->fd[zz] != -1) {
        r = read(proc->fd[zz], &value, sizeof(value));
        if (r == sizeof(value))
          proc->values[zz] = value;
        else
          proc->values[zz] = 0;
      }
      else  /* no fd, use marker */
        proc->values[zz] = 0xffffffff;
    }
    if (zombie) {
      proc->dead = 1;
      wait_for_child(proc->tid, options);
    }
  }

  return num_dead;
}


/* Scan list of processes and deallocates the dead ones, compacting the list. */
void compact_proc_list(struct process_list* const list)
{
  struct process* p;
  int i;

  for(p = list->processes; p; p = p->next) {
    if (p && p->next && p->next->dead) {
      struct process* to_delete = p->next;
      hash_del(to_delete->tid);
      p->next = to_delete->next;
      done_proc(to_delete);
      free(to_delete);
      list->num_tids--;
    }
  }
  /* special case for 1st element */
  if (list->processes->dead) {
    struct process* to_delete = list->processes;
    hash_del(to_delete->tid);
    list->processes = to_delete->next;
    done_proc(to_delete);
    free(to_delete);
    list->num_tids--;
  }

  /* update pointers */
  i = 0;
  for(p = list->processes; p; p = p->next) {
    list->proc_ptrs[i] = p;
    i++;
  }
}


/*
 * When threads are not displayed, this function accumulates
 * per-thread statistics in the parent process (which is also a
 * thread).
 */
void accumulate_stats(const struct process_list* const list)
{
  int zz;
  struct process* p;

  p = list->processes;
  for(p = list->processes; p; p = p->next) {
    if (p->pid != p->tid) {
      struct process* owner;

      if (p->dead)
        continue;

      /* find the owner */
      owner = hash_get(p->pid);
      assert(owner);

      /* accumulate in owner process */
      owner->cpu_percent += p->cpu_percent;
      for(zz = 0; zz < p->num_events; zz++) {
        /* as soon as one thread has invalid value, skip entire process. */
        if (p->values[zz] == 0xffffffff) {
          owner->values[zz] = 0xffffffff;
          break;
        }
        owner->values[zz] += p->values[zz];
      }
    }
  }
}


/*
 * When switching back from show_threads to no_thread, we need to
 * reset the statistics of the parent thread, because they contain
 * accumulated values, much higher than per-thread value. Failing to
 * do so results in transient erratic displayed values.
 */
void reset_values(const struct process_list* const list)
{
  struct process* p;

  for(p = list->processes; p; p = p->next) {
    if (p->dead)
      continue;
    /* only consider 'main' processes (not threads) */
    if (p->pid == p->tid) {
      int zz;
      p->cpu_percent = 0;
      for(zz = 0; zz < p->num_events; zz++) {
        p->values[zz] = 0;
      }
    }
  }
}


/* This is only used when tiptop fires a command itself. Right after
   the fork, the process name and command line are tiptop's. They are
   correct after exec. update_name_cmdline is invoked a little while
   after exec to fix these fields. */
void update_name_cmdline(int pid, int name_only)
{
  FILE* f;
  char  name[50] = { 0 };  /* needs to fit /proc/xxxx/{status,cmdline} */
  char  line[100];  /* line of /proc/xxxx/status */
  char  proc_name[100];

  struct process* p = hash_get(pid);
  if (!p)  /* gone? */
    return;

  /* update name */
  snprintf(name, sizeof(name) - 1, "/proc/%d/status", pid);
  f = fopen(name, "r");
  if (f) {
    while (fgets(line, sizeof(line), f)) {
      if (strncmp(line, "Name:", 5) == 0) {
        sscanf(line, "%*s %s", proc_name);
        if (p->name)
          free(p->name);
        p->name = strdup(proc_name);
        break;
      }
    }
    fclose(f);
  }

  if (!name_only) {  /* update command line */
    char  buffer[100];
    if (p->cmdline)
      free(p->cmdline);

    get_cmdline(pid, buffer, sizeof(buffer));
    p->cmdline = strdup(buffer);
  }
}
