#pragma once

#include "dist.h"
#include <list>
#include <vector>

using std::list;
using std::vector;

extern vector<Bucket> buckets;
using Marker = std::list<MemoryBlock>::iterator;

class CacheSim
{
private:
public:
  unsigned long long stack_size_;
  int next_bucket_;
  // datastruct_info &datastruct;
  int datastruct_num_;
//   vector<Bucket> &buckets_;
  list<MemoryBlock> stack_;
  void move_markers(int);
  CacheSim(int);
  ~CacheSim();
  list<MemoryBlock>& stack() { return stack_; }
  list<MemoryBlock>::iterator on_new_block(MemoryBlock &);
  void on_block_seen(list<MemoryBlock>::iterator &);
};