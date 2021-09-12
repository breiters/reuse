#pragma once

#include "bucket.h"
#include "memoryblock.h"
#include <list>
#include <vector>

class CacheSim {
public:
  CacheSim(int ds_num);
  // ~CacheSim();
  const Marker on_new_block(MemoryBlock mb);
  void on_block_seen(const Marker &m);
  void add_datastruct(int ds_num);
  bool contains(int ds_num) const;
  void print_csv(FILE *csv_out, const char *region);
  void print_csv(FILE *csv_out, const char *region,
                 std::vector<Bucket> &buckets);
  void print_csv(FILE *csv_out, const char *region, Bucket *buckets,
                 size_t num_buckets);

  bool operator==(const CacheSim &other) const {
    return ds_num_ == other.ds_num_ && ds_nums_ == other.ds_nums_;
    // return ds_num_ == other.ds_num_;
    // return this == &other;
  }

  inline const std::list<MemoryBlock> &stack() const { return stack_; }
  inline std::vector<Bucket> &buckets() { return buckets_; }
  inline const std::vector<int> &ds_nums() const { return ds_nums_; }
  inline int ds_num() const { return ds_num_; }

private:
  int next_bucket_;
  int ds_num_; // element in g_cachesims
  std::list<MemoryBlock> stack_;
  std::vector<Bucket> buckets_;
  std::vector<int> ds_nums_; // all datastructs that are included

  void move_markers(int);
  void on_next_bucket_gets_active();
};

namespace std {
template <> struct hash<CacheSim> {
  std::size_t operator()(CacheSim const &cs) const noexcept {
    size_t hash = std::hash<int>{}(cs.ds_num());
    int shift = 0;
    for (int x : cs.ds_nums()) {
      hash >>= shift++;
      hash ^= std::hash<int>{}(x) << shift;
    }
    return hash;

    return std::hash<int>{}(cs.ds_num());
  }
};
} // namespace std

extern std::vector<CacheSim> g_cachesims;
extern std::vector<CacheSim> g_cachesims_combined;
