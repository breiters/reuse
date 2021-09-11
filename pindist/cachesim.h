#pragma once

#include "bucket.h"
#include "memoryblock.h"
#include <list>
#include <vector>

class CacheSim {
private:
  int next_bucket_;
  // int next_bucket_other_;
  int ds_num_; // element in g_cachesims
  // std::list<MemoryBlock *> stack_other_;
  std::list<MemoryBlock *> stack_;
  std::vector<int> ds_nums_; // all datastructs that are included

  void move_markers(int);
  void on_next_bucket_gets_active();

public:
  std::vector<Bucket> buckets_;
  CacheSim(int ds_num);
  // ~CacheSim();
  inline std::list<MemoryBlock *> &stack() { return stack_; }
  const Marker on_new_block(MemoryBlock *mb);
  void on_block_seen(const Marker &m);
  void add_datastruct(int ds_num);
  bool contains(int ds_num) const;
  void print_csv(const char *region, FILE *csv_out);
};

extern std::vector<CacheSim> g_cachesims;
extern std::vector<CacheSim> g_cachesims_combined;
