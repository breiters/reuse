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
#include <unistd.h>

// Consistency checks?
#define DEBUG 0

// 2: Huge amount of debug output, 1: checks, 0: silent
#define VERBOSE 0

// uses INS_IsStackRead/Write: misleading with -fomit-frame-pointer
#define IGNORE_STACK 1

// collect addresses in chunk buffer before? (always worse)
#define MERGE_CHUNK 0
#define CHUNKSIZE 4096

static unsigned long stackAccesses;
static unsigned long ignoredReads, ignoredWrites;
static unsigned long abefore_cnt;
static unsigned num_threads = 0;
static PIN_MUTEX rr_mutex;   // add / remove threads
static PIN_MUTEX mtx;     // test purpose to compare vs. rrlock (can be removed)
static RRlock rrlock;     // round-robin lock

PIN_RWMUTEX g_rwlock;     // required for new datastruct and region entry/exit events (see imgload.cpp)
bool g_main_exit = false; // set true -> gracefully leave the rrlock /** TODO: implement correct concurrent list **/
// Barrier barr;
unsigned g_max_threads = 0;

/** On Linux IA-32 architectures, Pintools are built non-PIC (Position Independent Code), which allows the compiler to
 * inline both local and global functions. Tools for Linux Intel(R) 64 architectures are built PIC, but the compiler
 * will not inline any globally visible function due to function pre-emption. Therefore, it is advisable to declare the
 * subroutines called by the analysis function as 'static' on Linux Intel(R) 64 architectures. **/

void on_exit_main() {
  // PIN_ExitApplication(0);
  g_main_exit = 1;
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

/* ===================================================================== */
/* Direct Callbacks                                                      */
/* ===================================================================== */

// size: #bytes accessed
static void memAccess(ADDRINT addr, UINT32 size) {
  // bytes accessed: [addr, ... , addr+size-1]
  // atomic_fetch_add(&syncpoint, 1);

  // calculate memory block (cacheline) of low address
  Addr a1 = (void *)(addr & MEMBLOCK_MASK);
  // calculate memory block (cacheline) of high address
  Addr a2 = (void *)((addr + size - 1) & MEMBLOCK_MASK);

#if RD_DO_NOT_COUNT_ZERO_DISTANCE
  static Addr addr_last_arr[MAX_THREADS] = {0};
  bool accessed_before = (a1 == a2) && (a1 == addr_last_arr[PIN_ThreadId()]);
  addr_last_arr[PIN_ThreadId()] = a1;

  if (accessed_before) {
#if RD_ROUND_ROBIN
    rrlock.tick();
#endif /* RD_ROUND_ROBIN */
    return;
  }
#endif

#if RD_PARALLEL
  static int threads;
#if RD_VERBOSE
  static int iters;
  iters++;
  if (iters > 1000000) {
    printf("thread %d alive\n", PIN_ThreadId());
    iters = 0;
  }
#endif /* RD_VERBOSE */
#if RD_ROUND_ROBIN
  rrlock.lock();
#else
  PIN_MutexLock(&mtx);
#endif /* RD_ROUND_ROBIN */
  PIN_RWMutexReadLock(&g_rwlock);
  threads++;
  assert(threads == 1);
#endif /* RD_PARALLEL */

  if (accessed_before)
    abefore_cnt++;

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

#if RD_PARALLEL
  assert(threads == 1);
  threads--;
  PIN_RWMutexUnlock(&g_rwlock);
#if RD_ROUND_ROBIN
  rrlock.unlock();
#else
  PIN_MutexUnlock(&mtx);
#endif
#endif
}

static VOID memRead(THREADID tid, ADDRINT addr, UINT32 size) {
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

static VOID memWrite(THREADID tid, ADDRINT addr, UINT32 size) {
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

static VOID stackAccess() { stackAccesses++; }

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

  // atomic_fetch_add(&at_num_threads, 1);

  PIN_MutexLock(&rr_mutex);
  num_threads++;
  g_max_threads = num_threads > g_max_threads ? num_threads : g_max_threads;

  rrlock.add_thread();
  PIN_MutexUnlock(&rr_mutex);

  // lock.add_thread();
}

VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 flags, VOID *v) {
  fprintf(stderr, "Thread %d finished\n", tid);

  // atomic_fetch_sub(&at_num_threads, 1);
  PIN_MutexLock(&rr_mutex);
  num_threads--;

  rrlock.remove_thread();
  PIN_MutexUnlock(&rr_mutex);
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

  PIN_RWMutexInit(&g_rwlock);
  PIN_MutexInit(&mtx);
  PIN_MutexInit(&rr_mutex);
  rrlock = RRlock{};

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
  // Bucket::mins.push_back(4);
  // Bucket::mins.push_back(12);

#define RD_HISTOGRAM 1
#define RD_MIN_BUCKETS 1

#if RD_HISTOGRAM
  Bucket::mins.push_back(KiB / MEMBLOCKLEN);
  for (int i = 0; i < 4; i++)
    Bucket::mins.push_back(L1d_capacity_per_way * (i + 1) / MEMBLOCKLEN);
  Bucket::mins.push_back(MiB / 2 / MEMBLOCKLEN);
  for (int i = 1; i < 16; i += 2)
    Bucket::mins.push_back(L2_capacity_per_way * (i + 1) / MEMBLOCKLEN);
  // Bucket::mins.push_back(L2_capacity_per_way * 18 / MEMBLOCKLEN);

#else
  // Bucket::mins.push_back(KiB / MEMBLOCKLEN);
  Bucket::mins.push_back(L1d_capacity_per_way * 4 / MEMBLOCKLEN);
  Bucket::mins.push_back(L1d_capacity_per_way * 4 * 12 / MEMBLOCKLEN);
  // Bucket::mins.push_back(L2_capacity_per_way * 4 / MEMBLOCKLEN);
  Bucket::mins.push_back(L2_capacity_per_way * 8 / MEMBLOCKLEN);
  Bucket::mins.push_back(L2_capacity_per_way * 16 / MEMBLOCKLEN);
#endif

  /*
    Bucket::mins.push_back(KiB / MEMBLOCKLEN);
    Bucket::mins.push_back(L2_capacity_per_way * 4 / MEMBLOCKLEN);
    Bucket::mins.push_back(L2_capacity_per_way * 8 / MEMBLOCKLEN);
    Bucket::mins.push_back(L2_capacity_per_way * 16 / MEMBLOCKLEN);
    Bucket::mins.push_back(L2_capacity_per_way * 18 / MEMBLOCKLEN);
  */

  Bucket::mins.push_back(BUCKET_INF_DIST);

  RD_init();

  stackAccesses = 0;

  PIN_StartProgram();
  return 0;
}
