#pragma once

#include "bucket.h"
#include "memoryblock.h"
#include <list>
#include <unordered_map>
#include <vector>

class CacheSim {
public:
  CacheSim();
  // ~CacheSim();

  const StackIterator on_block_new(MemoryBlock mb);
  int on_block_seen(const StackIterator &it);
  void add_datastruct(int ds_num);
  bool contains(int ds_num) const;

  inline void incr_access(int bucket) { buckets_[bucket].aCount++; }
  inline void incr_access_excl(int bucket) { buckets_[bucket].aCount_excl++; }

  inline void incr_access_inf() { incr_access(buckets_.size() - 1); }
  inline void incr_access_excl_inf() { incr_access_excl(buckets_.size() - 1); }

  // bool operator==(const CacheSim &other) const { return cs_num_ == other.cs_num_ && ds_nums_ == other.ds_nums_; }

  inline const std::list<MemoryBlock> &stack() const { return stack_; }
  inline std::vector<Bucket> &buckets() { return buckets_; }
  // inline int cs_num() const { return cs_num_; }

  // inline const std::vector<int> &ds_nums() const { return ds_nums_; }

  void print_csv(FILE *csv_out, const char *region) const;
  void print_csv(FILE *csv_out, const char *region, const std::vector<Bucket> &buckets) const;

  static std::vector<CacheSim> cachesims;

  std::list<MemoryBlock> stack_; // Stack structure with MemoryBlock as element

  inline void print_stack() {
#if RD_DEBUG
    eprintf("\nstack:\n");
    for (auto it = stack_.begin(); it != stack_.end(); it++) {
      it->print();
      for (auto &m : buckets_) {
        if (m.marker == it) {
          eprintf("^~~~ Marker\n");
        }
      }
    }
    eprintf("\n");
#endif
  }

private:
  int next_bucket_;             //
  int cs_num_;                  // idx in cachesims
  std::vector<Bucket> buckets_; //
  std::vector<int> ds_nums_;    // all datastructs that are included

  void move_markers(int);
  void move_markers2(int);
  void on_next_bucket_gets_active();
};

#if NEED_HASHMAP
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
#endif
