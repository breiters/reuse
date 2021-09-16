/* Pin Tool for
 * calculation of the Stack Reuse Distance Histogram
 *
 * (C) 2015, Josef Weidendorfer / LRR-TUM
 * GPLv2+ (see COPYING)
 */

#include "pin.H"

#include "bucket.h"
#include "dist.h"
#include "imgload.h"

#include "rrlock.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdatomic.h>
#include <unistd.h>

#include "parallel.cpp"

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

#define RD_PARALLEL 1
#if !(RD_PARALLEL)
#define RD_ROUND_ROBIN 0
#else
#define RD_ROUND_ROBIN 1
#endif

#define ITERS_FOR_SYNC 1200

PIN_MUTEX g_mtx;
PIN_RWMUTEX g_rwlock;
PIN_RWMUTEX g_add_thread_lock;

RRlock rrlock;
// Barrier barr;

unsigned g_max_threads = 0;
unsigned num_threads = 0;
// atomic_uint_fast32_t main_exit;
bool main_exit = 0;

void on_exit_main() {
  // atomic_store(&main_exit, 1);
  main_exit = 1;
  printf("exit main... thread: %d\n", PIN_ThreadId());
}

/* ===================================================================== */
/* Command line options                                                  */
/* ===================================================================== */

KNOB<int> KnobMinDist(KNOB_MODE_WRITEONCE, "pintool", "m", "4096", "minimum bucket distance");
KNOB<int> KnobDoubleSteps(KNOB_MODE_WRITEONCE, "pintool", "s", "1", "number of buckets for doubling distance");
KNOB<bool> KnobPIDPrefix(KNOB_MODE_WRITEONCE, "pintool", "p", "0", "prepend output by --PID--");

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

// atomic_uint_fast32_t syncpoint;

/* ===================================================================== */
/* Direct Callbacks                                                      */
/* ===================================================================== */

// size: #bytes accessed
void memAccess(ADDRINT addr, UINT32 size) {
  // bytes accessed: [addr, ... , addr+size-1]
#if RD_ROUND_ROBIN
  // static int threads;
  // lock.lock();
#else
#endif

  // atomic_fetch_add(&syncpoint, 1);

  // calculate memory block (cacheline) of low address
  Addr a1 = (void *)(addr & MEMBLOCK_MASK);
  // calculate memory block (cacheline) of high address
  Addr a2 = (void *)((addr + size - 1) & MEMBLOCK_MASK);

  // PIN_MutexLock(&g_mtx);
  rrlock.lock();
  PIN_RWMutexReadLock(&g_rwlock);

  // PIN_RWMutexReadLock(&g_add_thread_lock);

  // threads++;

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

  PIN_RWMutexUnlock(&g_rwlock);
  rrlock.unlock();
  // PIN_RWMutexUnlock(&g_add_thread_lock);
  // PIN_RWMutexUnlock(&g_rwlock);

/*
  if (num_threads == 12 && atomic_load(&syncpoint) >= ITERS_FOR_SYNC) {
    eprintf("thread %d barrier arrival\n", PIN_ThreadId());
    barr.arrive_and_wait(PIN_ThreadId());
    eprintf("thread %d barrier end\n", PIN_ThreadId());
  }
  */

    /*
      if(atomic_load(&syncpoint) == ITERS_FOR_SYNC) {
        atomic_store(&syncpoint, 0);
      }
      */

    // if (threads != 1)
    // puts("failed sync");
    // threads--;

  // PIN_MutexUnlock(&g_mtx);

#if RD_ROUND_ROBIN
    // assert(threads == 1);
    // lock.unlock();
#else
#endif
}

VOID memRead(THREADID tid, ADDRINT addr, UINT32 size) {
#if !(RD_PARALLEL)
  if (tid > 0) {
    // we are NOT thread-safe, ignore access
    ignoredReads++;
    return;
  }
#endif

  if (VERBOSE > 1)
    fprintf(stderr, "R %p/%d", (void *)addr, size);

  memAccess(addr, size);
}

VOID memWrite(THREADID tid, ADDRINT addr, UINT32 size) {
#if !(RD_PARALLEL)
  if (tid > 0) {
    // we are NOT thread-safe, ignore access
    ignoredWrites++;
    return;
  }
#endif

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
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)stackAccess, IARG_END);
    return;
  }

  UINT32 memOperands = INS_MemoryOperandCount(ins);

  for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
    if (INS_MemoryOperandIsRead(ins, memOp))
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)memRead, IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp,
                               IARG_UINT32, INS_MemoryOperandSize(ins, memOp), IARG_END);

    if (INS_MemoryOperandIsWritten(ins, memOp))
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)memWrite, IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp,
                               IARG_UINT32, INS_MemoryOperandSize(ins, memOp), IARG_END);
  }
}

/* ===================================================================== */
/* Callbacks from Pin                                                    */
/* ===================================================================== */

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v) {
  fprintf(stderr, "Thread %d started\n", tid);

  // PIN_RWMutexWriteLock(&g_add_thread_lock);
  num_threads++;
  g_max_threads = num_threads > g_max_threads ? num_threads : g_max_threads;
  rrlock.add_thread();
  // PIN_RWMutexUnlock(&g_add_thread_lock);

  // atomic_fetch_add(&num_threads, 1);
  // lock.add_thread();
}

VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 flags, VOID *v) {

  PIN_RWMutexWriteLock(&g_add_thread_lock);
  num_threads--;
  PIN_RWMutexUnlock(&g_add_thread_lock);

  /*
  if(!active[tid]) {
    return;
  }
  fprintf(stderr, "Thread %d finished\n", tid);

#if RD_ROUND_ROBIN
  // PIN_GetLock(&lock, tid);
  PIN_MutexLock(&mtx);

  active[tid] = false;

  THREADID t = atomic_load(&turn);
  if (t == tid) {
    set_next_active_thread(tid);
  }

  num_threads--;

  // PIN_ReleaseLock(&lock);
  PIN_MutexUnlock(&mtx);
#endif
  */
  fprintf(stderr, "Thread %d finish exit\n", tid);
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

  fprintf(out, "%s  ignored accesses by thread != 0: %lu reads, %lu writes\n", pStr, ignoredReads, ignoredWrites);

  // original code has min = 0 to define infinite distance
  // change to max limit to distinguish buckets zero dist from buckets inf dist
  Bucket::mins.back() = std::numeric_limits<unsigned>::max();
  RD_print_csv();
}

/* ===================================================================== */
/* Usage/Main Function of the Pin Tool                                   */
/* ===================================================================== */

INT32 Usage() {
  PIN_ERROR("PinDist: Get the Stack Reuse Distance Histogram\n" + KNOB_BASE::StringKnobSummary() + "\n");
  return -1;
}

int main(int argc, char *argv[]) {
  if (PIN_Init(argc, argv))
    return Usage();

  PIN_InitSymbols();
  IMG_AddInstrumentFunction(ImageLoad, 0);

  INS_AddInstrumentFunction(Instruction, 0);
  PIN_AddFiniFunction(Exit, 0);
  PIN_AddThreadStartFunction(ThreadStart, 0);
  PIN_AddThreadFiniFunction(ThreadFini, 0);

  // lock = RRlock{};
  PIN_RWMutexInit(&g_add_thread_lock);
  PIN_RWMutexInit(&g_rwlock);
  PIN_MutexInit(&g_mtx);

  rrlock = RRlock{};

  // atomic_store(&syncpoint, 0);

#if USE_OLD_CODE
  // add buckets [0-1023], [1K - 2K-1], ... [1G - ]
  double d = KnobMinDist.Value();
  int s = KnobDoubleSteps.Value();
  double f = pow(2, 1.0 / s);

  RD_init((int)(d / MEMBLOCKLEN));
  for (d *= f; d < 1024 * 1024 * 1024; d *= f) {
    // printf("add bucket: %d\n", (int)(d / MEMBLOCKLEN));
    RD_addBucket((int)(d / MEMBLOCKLEN));
  }
#endif /* USE_OLD_CODE */

  // required buckets for a64fx:
  // 4-way L1d 64KiB => 4 Buckets with distance 64KiB / 4
  // 16-way L2 8MiB => 16 Buckets with distance 8MiB / 16
  //
  int KiB = 1024;
  int MiB = 1024 * KiB;

  [[maybe_unused]] int L1d_capacity_per_way = 64 * KiB / 4;
  int L2_capacity_per_way = 8 * MiB / 16;

  Bucket::mins.push_back(0);

#define RD_HISTOGRAM 1
#if RD_HISTOGRAM
  Bucket::mins.push_back(KiB / MEMBLOCKLEN);
  for (int i = 0; i < 4; i++)
    Bucket::mins.push_back(L1d_capacity_per_way * (i + 1) / MEMBLOCKLEN);
  Bucket::mins.push_back(MiB / 2 / MEMBLOCKLEN);
  for (int i = 1; i < 16; i += 2)
    Bucket::mins.push_back(L2_capacity_per_way * (i + 1) / MEMBLOCKLEN);
  Bucket::mins.push_back(L2_capacity_per_way * 18 / MEMBLOCKLEN);

#else
  Bucket::mins.push_back(KiB / MEMBLOCKLEN);
  Bucket::mins.push_back(L2_capacity_per_way * 4 / MEMBLOCKLEN);
  Bucket::mins.push_back(L2_capacity_per_way * 8 / MEMBLOCKLEN);
  Bucket::mins.push_back(L2_capacity_per_way * 16 / MEMBLOCKLEN);
  Bucket::mins.push_back(L2_capacity_per_way * 18 / MEMBLOCKLEN);
#endif

  Bucket::mins.push_back(BUCKET_INF_DIST);

  RD_init();

  stackAccesses = 0;

  PIN_StartProgram();
  return 0;
}
