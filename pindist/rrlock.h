#include "dist.h"
#include <stdatomic.h>

// extern atomic_uint_fast32_t main_exit;

typedef atomic_uintptr_t atomic_nodeptr;

#define CLS 64

// struct alignas(CLS) RRnode {
struct RRnode {
  atomic_uintptr_t next_;
  atomic_int_fast8_t turn_;
  // atomic_int_fast8_t tick_;
};

// static_assert(alignof(RRnode) == CLS);
// static_assert(sizeof(RRnode) == CLS);

class RRlock {
private:
  atomic_uintptr_t head_;
  atomic_uintptr_t tail_;
  
  RRnode *nodes_[MAX_THREADS] = {0};
  // bool tick_[MAX_THREADS] = {0};

public:
  RRlock();
  ~RRlock();

  void lock();
  void unlock();
  void tick();
  
  void add_thread();
  void remove_thread();
};
