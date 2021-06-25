#pragma once

#include "dist.h"
#include <list>
#include <vector>

using std::list;
using std::vector;
using Marker = std::list<MemoryBlock>::iterator;

extern vector<Bucket> buckets;

class CacheSim {
private:
  int next_bucket_;
  int datastruct_num_;
  list<MemoryBlock> stack_;
  // datastruct_info &datastruct;
  // vector<Bucket> &buckets_;

  void move_markers(int);
  void on_next_bucket_gets_active();

public:
  CacheSim(int);
  // ~CacheSim();
  inline list<MemoryBlock> &stack() { return stack_; }
  const list<MemoryBlock>::iterator on_new_block(const MemoryBlock &);
  void on_block_seen(const list<MemoryBlock>::iterator &);
};