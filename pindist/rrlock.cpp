#include "rrlock.h"
#include "pin.H"
#include <stdatomic.h>

extern bool g_main_exit;

#if DBGPRINT
#define dbgprintf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbgprintf(...)
#endif

RRlock::RRlock() {
  atomic_store(&head_, (uintptr_t)NULL);
  atomic_store(&tail_, (uintptr_t)NULL);
}
RRlock::~RRlock() {}

void RRlock::lock() {
  THREADID tid = PIN_ThreadId();
  dbgprintf("thread: %d try acquire lock...\n", tid);

  while (atomic_load(&nodes_[tid]->turn_) != 1) {
    dbgprintf("thread: %d spinning...\n", tid);
    if (g_main_exit == 1) {
      PIN_ExitThread(tid);
    }
  }
  dbgprintf("thread: %d acquired lock!\n", tid);
}

void RRlock::tick() {
  lock();
  unlock();
}

void RRlock::unlock() {
  THREADID tid = PIN_ThreadId();
  atomic_store(&nodes_[tid]->turn_, 0);
  // RRnode *next = (RRnode *)atomic_load(&nodes_[tid]->next_);
  // atomic_store(&next->turn_, 1);
  atomic_store(&((RRnode *)atomic_load(&nodes_[tid]->next_))->turn_, 1);
  dbgprintf("thread: %d released lock!\n", tid);
}

void RRlock::remove_thread() { /** TODO: (not required here for this implementation)  **/
}

void RRlock::add_thread() {
  THREADID tid = PIN_ThreadId();
  RRnode *node = new RRnode();

  atomic_store(&node->turn_, 0);
  // atomic_store(&node->tick_, 0);
  nodes_[tid] = node;

  RRnode *oldhead = NULL;
  if (atomic_compare_exchange_strong(&head_, &oldhead, node)) {
    // current thread was first thread and head_ is now node
    atomic_store(&node->next_, (uintptr_t)node);
    atomic_store(&tail_, (uintptr_t)node);
    atomic_store(&node->turn_, 1);
    dbgprintf("thread: %d was first\n", tid);
    return;
  }

  // current thread is not first thread. wait until first thread has changed tail_
  while ((void *)atomic_load(&tail_) == NULL) {
    /* spin */
  }

  while (!atomic_compare_exchange_weak(&head_, &oldhead, node)) {
    oldhead = (RRnode *)atomic_load(&head_);
  }

  atomic_store(&node->next_, (uintptr_t)oldhead);
  atomic_store(&((RRnode *)atomic_load(&tail_))->next_, (uintptr_t)node);

  dbgprintf("added thread: %d\n", tid);
}
