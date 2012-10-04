/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "screen.h"
#include "target.h"


enum tables { A_UNK, A_1, A_2, A_4, A_6, A_10, A_11, A_12, A_22 };

#if 0
static const char* table_names[] = {
  "unknown", "A-1", "A-2", "A-4", "A-6", "A-10", "A-11", "A-12", "A-22"
};
#endif

const char* const arch_selector = "x86";


screen_t* screen_uop(void);
screen_t* screen_fp(void);
screen_t* screen_mem(void);
screen_t* screen_imix(void);


enum targets get_target()
{
  return X86;
}


int match_target(const char* tgt)
{
  if (strcasecmp(tgt, arch_selector) == 0)
    return 1;
  return 0;
}


static int disp_family_model()
{
  int a;
  int disp_model, disp_family;

  asm("mov $1, %%eax; " /* a into eax */
      "cpuid;"
      "mov %%eax, %0;"  /* eax into a */
      :"=r"(a) /* output */
      :
      :"%eax","%ebx","%ecx","%edx" /* clobbered register */
     );
  disp_model = ((a >> 4) & 0xf) | (((a >> 16) & 0xf) << 4);
  disp_family = ((a >> 8) & 0xf) | (((a >> 20) & 0xff) << 4);
  return (disp_family << 8) | disp_model;
}


static enum tables table()
{
  int disp_fam = disp_family_model();

  if (disp_fam == 0x062a)
    return A_2;
  else if ((disp_fam == 0x061a) || (disp_fam == 0x061e) ||
           (disp_fam == 0x061f) || (disp_fam == 0x062e))
    return A_4;
  else if ((disp_fam == 0x0625) || (disp_fam == 0x062c))
    return A_6;
  else
    return A_UNK;
}


char* get_model()
{
  int disp_fam = disp_family_model();
  char* model = malloc(6);
  sprintf(model, "%02X_%02X", disp_fam >> 8, disp_fam & 0xff);
  return model;
}


int match_model(const char* model)
{
  char cur_model[6];
  int disp_fam = disp_family_model();
  sprintf(cur_model, "%02X_%02X", disp_fam >> 8, disp_fam & 0xff);
  if (strstr(model, cur_model))  /* a bit too permissive? */
    return 1;

  return 0;
}


/* Generate a target-dependent string, displayed in the help window. */
void target_dep_string(char* buf, int size)
{
  int disp_fam = disp_family_model();

  snprintf(buf, size, "Target: x86, model = %02X_%02X",
           disp_fam >> 8, disp_fam & 0xff);
}


void screens_hook()
{
  screen_uop();
  screen_fp();
  screen_mem();
  screen_imix();
}


#define FP_COMP_OPS_EXE_X87                   0x0110  /* A-2, A-4, A-6 */
#define FP_COMP_OPS_EXE_SSE_SINGLE_PRECISION  0x4010  /* A-2, A-4, A-6 */
#define FP_COMP_OPS_EXE_SSE_DOUBLE_PRECISION  0x8010  /* A-2, A-4, A-6 */
#define FP_ASSIST_1                           0x1eca  /* A-2 */
#define FP_ASSIST_2                           0x01f7  /* A-4, A-6 */


#define PERF_TYPE_RAW 0x04


screen_t* screen_fp()
{
  int assist_cnt;
  screen_t* s;

  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6))
    return NULL;

  s = new_screen("FP", "Floating point instructions", 0);

  /* setup counters */

  add_counter(s, "C", "CPU_CYCLES", "HARDWARE");
  add_counter(s, "I", "INSTRUCTIONS",  "HARDWARE");

  add_counter_by_value(s, "x87", FP_COMP_OPS_EXE_X87, PERF_TYPE_RAW);
  add_counter_by_value(s, "sp",  FP_COMP_OPS_EXE_SSE_SINGLE_PRECISION,
                                                        PERF_TYPE_RAW);
  add_counter_by_value(s, "dp",  FP_COMP_OPS_EXE_SSE_DOUBLE_PRECISION,
                                                        PERF_TYPE_RAW);

  if (tab == A_2)
    assist_cnt = FP_ASSIST_1;
  else
    assist_cnt = FP_ASSIST_2;

  add_counter_by_value(s, "Assist", assist_cnt, PERF_TYPE_RAW);

  /* add columns */
  add_column(s, " %CPU"    , "%5.1f",  "CPU usage", "CPU_TOT");
  add_column(s, "  Mcycle", "%8.2f",
             "Cycles (millions)",
             "delta(C) / 1000000");
  add_column(s, "  Minstr", "%8.2f",
             "Instructions (millions)",
             "delta(I) / 1000000");
  add_column(s, " IPC",     "%4.2f",
             "Executed instructions per cycle",
             "delta(I)/delta(C)");
  add_column(s," %x87", "%5.1f",
             "FP computational uops (FP_COMP_OPS_EXE.X87) per insn",
             "100*delta(x87)/delta(I)");
  add_column(s, "       SSES", "   %8.1f",
             "SSE* FP single precision uops per insn per 1000",
             "100*delta(sp)/delta(I)");

  add_column(s, "       SSED", "   %8.1f",
             "SSE* FP double precision uops per insn per 1000",
             "100*delta(dp)/delta(I)");

  add_column(s, "%assist", "  %5.1f",
             "FP op that required micro-code assist per instruction",
             "100*delta(Assist)/delta(I)");

  return s;
}


#define MEM_INST_RETIRED_LOADS       0x010b  /* A-4, A-6 */
#define MEM_INST_RETIRED_STORES      0x020b  /* A-4, A-6 */
#define BR_INST_RETIRED_ALL_BRANCHES 0x00c4  /* A-2, A-4, A-6, A-10, A-11, A-12, A-22 */
#define INST_RETIRED_X87             0x02c0  /* A-2, A-4, A-6 */

screen_t* screen_imix()
{
  screen_t* s;
  enum tables tab = table();

  if ((tab != A_4) && (tab != A_6))
    return NULL;

  s = new_screen("imix","Instruction mix", 0);

  /* setup counters */
  add_counter(s, "C", "CPU_CYCLES", "HARDWARE");
  add_counter(s, "I", "INSTRUCTIONS",  "HARDWARE");
  add_counter_by_value(s, "LD",  MEM_INST_RETIRED_LOADS,       PERF_TYPE_RAW);
  add_counter_by_value(s, "ST",  MEM_INST_RETIRED_STORES,      PERF_TYPE_RAW);
  add_counter_by_value(s, "x87", INST_RETIRED_X87,             PERF_TYPE_RAW);
  add_counter_by_value(s, "brr", BR_INST_RETIRED_ALL_BRANCHES, PERF_TYPE_RAW);

  /* add columns */
  add_column(s, " %CPU", "%5.1f", "CPU usage", "CPU_TOT");
  add_column(s, "  Mcycle", "%8.2f",
             "Cycles (millions)"  ,
             "delta(C) / 1000000");
  add_column(s, "  Minstr", "%8.2f",
             "Instructions (millions)",
             "delta(I) / 1000000");
  add_column(s, " IPC", "%4.2f",
             "Executed instructions per cycle",
             "delta(I)/delta(C)");
  add_column(s, "     %LD/I", "  %8.1f",
             "Fraction of load",
             "100*delta(LD)/delta(I)");
  add_column(s, "    %ST/I", "  %7.1f",
             "Fraction of stores",
             "100*delta(ST)/delta(I)");
  add_column(s, " %FP/I", "  %4.1f",
             "Fraction of x87","100*delta(x87)/delta(I)");
  add_column(s, "     %BR/I","  %8.1f",
             "Fraction of branch instructions",
             "100*delta(brr)/delta(I)");

  return s;
}


#define ICACHE_MISSES_1              0x0280  /* A-2, A-11 */
#define ICACHE_MISSES_2              0x0081  /* A-12 */
#define L1I_MISSES                   0x0081  /* A-4, A-6, A-10 */

#define L2_RQSTS_LD_MISS             0x0224  /* A-4, A-6 */
#define L2_RQSTS_CODE_RD_MISS        0x2024  /* A-2 */
#define L2_RQSTS_IFETCH_MISS         0x2024  /* A-4, A-6 */


/* VERY EXPERIMENTAL */
screen_t* screen_mem()
{
  screen_t* s;
  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6))
    return NULL;

  s = new_screen("mem", "Memory hierarchy", 0);

  /* setup counters */
  add_counter(s, "I", "INSTRUCTIONS",  "HARDWARE");

  /* L1I accesses */
  if (tab == A_2)
    add_counter_by_value(s, "L1MissI", ICACHE_MISSES_1, PERF_TYPE_RAW);
  else
    add_counter_by_value(s, "L1MissI", L1I_MISSES, PERF_TYPE_RAW);

  /* L2 loads */
  if (tab == A_2) {
    /* no support for L2 data miss in Table A-2 */
    add_counter_by_value(s, "L2MissI", L2_RQSTS_CODE_RD_MISS, PERF_TYPE_RAW);
  }
  else {
    add_counter_by_value(s, "L2MissD", L2_RQSTS_LD_MISS, PERF_TYPE_RAW);
    add_counter_by_value(s, "L2MissI", L2_RQSTS_IFETCH_MISS,PERF_TYPE_RAW);
  }

  add_counter(s, "L3Miss", "CACHE_MISSES", NULL);

  /* add columns */
  add_column(s, " %CPU", "%5.2f", "CPU usage", "CPU_TOT");
  add_column(s, "   I(M)", "%7.2f", "Instruction per Million", "delta(I)/1000000");

  add_column(s, "    L1iMiss", "   %8.1f",
             "Instruction fetches that miss in L1I (L1I.MISSES) (million)",
             "delta(L1MissI)");
  add_column(s, "   L1i", "  %4.1f",
             "Same L1iMiss per instruction",
             "100*delta(L1MissI)/delta(I)");
  add_column(s, "  L2iMiss", " %8.1f",
             "Insn fetches that miss L2 cache (L2_RQSTS.IFETCH_MISS) (million)",
             "delta(L2MissI)");

  add_column(s, "   L2i", "  %4.1f",
             "same L2iMiss per instruction",
             "100*delta(L2MissI) / delta(I)");

  if (tab != A_2) {
    add_column(s, "  L2dMiss", "%9.1f",
               "Loads that miss L2 cache",
               "delta(L2MissD)");
    add_column(s, "  L2d", " %4.1f",
               "   same L2dMiss, per instruction",
               "100*delta(L2MissD)/delta(I)");
  }

  add_column(s, "    L3Miss", " %9.1f",
             "LLC Misses",
             "delta(L3Miss)");
   add_column(s, "   L3", " %4.1f",
             "same L3Miss, per instruction",
             "100*delta(L3Miss) / delta(I)");
  return s;
}


#define UOPS_RETIRED_ALL_1           0x01c2  /* A-2, A-4, A-6 */
#define UOPS_RETIRED_ALL_2           0x0fc2  /* A-10 */
#define UOPS_RETIRED_FUSED           0x07c2  /* A-10 */
#define UOPS_RETIRED_MACRO_FUSED     0x04c2  /* A-4, A-6 */

screen_t* screen_uop()
{
  int fused, macrof;
  screen_t* s = NULL;
  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6) && (tab != A_10))
    return NULL;

  s = new_screen("uOps","micro operations", 0);

  /* setup counters */
  add_counter(s, "C", "CPU_CYCLES", "HARDWARE");
  add_counter(s, "I", "INSTRUCTIONS",  "HARDWARE");

  if (tab == A_10)
    add_counter_by_value(s, "UOP", UOPS_RETIRED_ALL_2, PERF_TYPE_RAW);
  else
    add_counter_by_value(s, "UOP", UOPS_RETIRED_ALL_1, PERF_TYPE_RAW);

  if (tab == A_10)
    fused =  add_counter_by_value(s, "FUSE", UOPS_RETIRED_FUSED, PERF_TYPE_RAW);
  else
    fused = -1;

  if ((tab == A_4) || (tab == A_6))
    macrof = add_counter_by_value(s, "MACROF", UOPS_RETIRED_MACRO_FUSED, PERF_TYPE_RAW);
  else
    macrof = -1;

  /* add columns */
  add_column(s, " %CPU",    "%5.1f", "CPU usage", "CPU_TOT");
  add_column(s, " %SYS",    "%5.1f", "CPU sys", "CPU_SYS");
  add_column(s, "  Mcycle", "%8.2f",
             "Cycles (millions)"  ,
             "delta(C) / 1000000");
  add_column(s, "  Minstr", "%8.2f",
             "Instructions (millions)",
             "delta(I) / 1000000");
  add_column(s, " IPC",     "%4.2f",
             "Executed instructions per cycle",
             "delta(I)/delta(C)");
  add_column(s, "   Muops", "%8.2f",
             "Retired uops (millions)",
             "delta(UOP)/1000000");
  add_column(s, "  uPI", " %4.2f",
             "Retired uops per instruction",
             "delta(UOP)/delta(I)");

  if (fused != -1)
    add_column(s, " fused", "  %4.2f",
               "Fused uops (UOPS_RETIRED.FUSED) per instruction",
               "delta(FUSE)/delta(I)");

  if (macrof != -1)
    add_column(s, " macrof", "   %4.2f",
               "Macro fused uops (UOPS_RETIRED.MACRO_FUSED) per instruction",
               "delta(MACROF)/delta(I)");

  return s;
}
