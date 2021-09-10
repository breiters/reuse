/* Pin Tool for
 * calculation of the Stack Reuse Distance Histogram
 *
 * (C) 2015, Josef Weidendorfer / LRR-TUM
 * GPLv2+ (see COPYING)
 */

#include "pin.H"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "dist.h"
#include "imgload.h"

// Consistency checks?
#define DEBUG 0

// 2: Huge amount of debug output, 1: checks, 0: silent
#define VERBOSE 0

// uses INS_IsStackRead/Write: misleading with -fomit-frame-pointer
#define IGNORE_STACK 1

// collect addresses in chunk buffer before? (always worse)
#define MERGE_CHUNK 0
#define CHUNKSIZE 4096

unsigned long stackAccesses;
unsigned long ignoredReads, ignoredWrites;

/* ===================================================================== */
/* Command line options                                                  */
/* ===================================================================== */

KNOB<int> KnobMinDist(KNOB_MODE_WRITEONCE, "pintool", "m", "4096",
                      "minimum bucket distance");
KNOB<int> KnobDoubleSteps(KNOB_MODE_WRITEONCE, "pintool", "s", "1",
                          "number of buckets for doubling distance");
KNOB<bool> KnobPIDPrefix(KNOB_MODE_WRITEONCE, "pintool", "p", "0",
                         "prepend output by --PID--");

/* ===================================================================== */
/* Handle Memory block access (aligned at multiple of MEMBLOCKLEN)       */
/* ===================================================================== */

#if MERGE_CHUNK
void accessMerging(Addr a) {
  static Addr mergeBuffer[CHUNKSIZE];
  static int ptr = 0;

  if (ptr < CHUNKSIZE) {
    mergeBuffer[ptr++] = a;
    return;
  }
  sort(mergeBuffer, mergeBuffer + CHUNKSIZE);
  for (ptr = 0; ptr < CHUNKSIZE; ptr++) {
    RD_accessBlock(mergeBuffer[ptr]);
  }
  ptr = 0;
}
#define RD_accessBlock accessMerging
#endif

/* ===================================================================== */
/* Direct Callbacks                                                      */
/* ===================================================================== */

// size: #bytes accessed
void memAccess(ADDRINT addr, UINT32 size) {
  // bytes accessed: [addr, ... , addr+size-1]

  // calculate memory block (cacheline) of low address
  Addr a1 = (void *)(addr & MEMBLOCK_MASK);
  // calculate memory block (cacheline) of high address
  Addr a2 = (void *)((addr + size - 1) & MEMBLOCK_MASK);

  // single memory block accessed
  if (a1 == a2) {
    if (VERBOSE > 1)
      fprintf(stderr, " => %p\n", a1);
    RD_accessBlock(a1);
  }
  // memory access spans across two memory blocks
  // => two memory blocks accessed
  else {
    if (VERBOSE > 1)
      fprintf(stderr, " => CROSS %p/%p\n", a1, a2);
    RD_accessBlock(a1);
    RD_accessBlock(a2);
  }
}

VOID memRead(THREADID t, ADDRINT addr, UINT32 size) {
  if (t > 0) {
    // we are NOT thread-safe, ignore access
    ignoredReads++;
    return;
  }

  if (VERBOSE > 1)
    fprintf(stderr, "R %p/%d", (void *)addr, size);

  memAccess(addr, size);
}

VOID memWrite(THREADID t, ADDRINT addr, UINT32 size) {
  if (t > 0) {
    // we are NOT thread-safe, ignore access
    ignoredWrites++;
    return;
  }

  if (VERBOSE > 1)
    fprintf(stderr, "W %p/%d", (void *)addr, size);

  memAccess(addr, size);
}

VOID stackAccess() { stackAccesses++; }

/* ===================================================================== */
/* Instrumentation                                                       */
/* ===================================================================== */

VOID Instruction(INS ins, VOID *v) {
  if (IGNORE_STACK && (INS_IsStackRead(ins) || INS_IsStackWrite(ins))) {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)stackAccess,
                             IARG_END);
    return;
  }

  UINT32 memOperands = INS_MemoryOperandCount(ins);
  for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
    if (INS_MemoryOperandIsRead(ins, memOp))
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)memRead,
                               IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp,
                               IARG_UINT32, INS_MemoryOperandSize(ins, memOp),
                               IARG_END);

    if (INS_MemoryOperandIsWritten(ins, memOp))
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)memWrite,
                               IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp,
                               IARG_UINT32, INS_MemoryOperandSize(ins, memOp),
                               IARG_END);
  }
}

/* ===================================================================== */
/* Callbacks from Pin                                                    */
/* ===================================================================== */

VOID ThreadStart(THREADID t, CONTEXT *ctxt, INT32 flags, VOID *v) {
  fprintf(stderr, "Thread %d started\n", t);
}

/* ===================================================================== */
/* Output results at exit                                                */
/* ===================================================================== */

VOID Exit(INT32 code, VOID *v) {
  char pStr[20];
  FILE *out = stdout;

  if (KnobPIDPrefix.Value())
    sprintf(pStr, "--%5d-- ", getpid());
  else
    pStr[0] = 0;

  RD_printHistogram(out, pStr, MEMBLOCKLEN);

  fprintf(out, "%s  ignored stack accesses: %lu\n", pStr, stackAccesses);

  fprintf(out, "%s  ignored accesses by thread != 0: %lu reads, %lu writes\n",
          pStr, ignoredReads, ignoredWrites);
}

/* ===================================================================== */
/* Usage/Main Function of the Pin Tool                                   */
/* ===================================================================== */

INT32 Usage() {
  PIN_ERROR("PinDist: Get the Stack Reuse Distance Histogram\n" +
            KNOB_BASE::StringKnobSummary() + "\n");
  return -1;
}

int main(int argc, char *argv[]) {
  if (PIN_Init(argc, argv))
    return Usage();

#if USE_OLD_CODE
  // add buckets [0-1023], [1K - 2K-1], ... [1G - ]
  double d = KnobMinDist.Value();
  int s = KnobDoubleSteps.Value();
  double f = pow(2, 1.0 / s);

  RD_init((int)(d / MEMBLOCKLEN));
  for(d*=f; d< 1024*1024*1024; d*=f) {
    // printf("add bucket: %d\n", (int)(d / MEMBLOCKLEN));
    RD_addBucket((int)(d / MEMBLOCKLEN));
  }
#endif

  // required buckets for a64fx:
  // 4-way L1d 64KiB => 4 Buckets with distance 64KiB / 4
  // 16-way L2 8MiB => 16 Buckets with distance 8MiB / 16
  //
  int KiB = 1024;
  int MiB = 1024 * KiB;

  int L1d_capacity_per_way = 64 * KiB / 4;
  int L2_capacity_per_way = 8 * MiB / 16;

  RD_init(4);

  // RD_init(L1d_capacity_per_way / MEMBLOCKLEN);
  for (int i = 0; i < 4; i++)
    RD_addBucket(L1d_capacity_per_way * (i + 1) / MEMBLOCKLEN);
  for (int i = 1; i < 16; i += 2)
    RD_addBucket(L2_capacity_per_way * (i + 1) / MEMBLOCKLEN);

  // RD_addBucket(L2_capacity_per_way * 16 / MEMBLOCKLEN);

  stackAccesses = 0;
  
  PIN_InitSymbols();

  INS_AddInstrumentFunction(Instruction, 0);
  PIN_AddFiniFunction(Exit, 0);
  PIN_AddThreadStartFunction(ThreadStart, 0);

  IMG_AddInstrumentFunction(ImageLoad, 0);

  PIN_StartProgram();
  return 0;
}
