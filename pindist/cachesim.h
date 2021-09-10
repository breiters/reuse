#pragma once

#include "memoryblock.h"
#include <list>

class CacheSim {
private:
  int next_bucket_;
  int datastruct_num_;
  std::list<MemoryBlock *> stack_;

  void move_markers(int);
  void on_next_bucket_gets_active();

public:
  CacheSim(int ds_num);
  // ~CacheSim();
  inline std::list<MemoryBlock *> &stack() { return stack_; }
  const Marker on_new_block(MemoryBlock *);
  void on_block_seen(const Marker &);
};

extern std::vector<CacheSim> g_cachesims;
extern std::vector<CacheSim> g_cachesims_combined;
extern std::vector<CacheSim> g_cachesims_negated;
extern std::vector<CacheSim> g_cachesims_combined_negated;