#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "bucket.h"
#include "cachesim.h"

class Region {
public:
  char *region_;
  std::vector<Bucket> global_buckets_;
  std::vector<Bucket> global_buckets_entry_;

  std::vector<Bucket *> buckets_;
  std::vector<Bucket *> buckets_entry_;
  // std::unordered_map<CacheSim, std::vector<Bucket>> region_buckets_;
  // std::unordered_map<CacheSim, std::vector<Bucket>> region_buckets_on_entry_;

  Region(char *region);
  ~Region();
  void register_datastruct();
  void on_region_entry();
  void on_region_exit();
  void demangle_name();
  void print_csv(FILE *csv_out);

private:
};

// TODO: should use some smart pointer here but PIN stl port does not really
// provide it... ?!
extern std::unordered_map<char *, Region *> g_regions;
