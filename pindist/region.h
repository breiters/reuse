#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "bucket.h"

class Region {
public:
  char *region_;
  std::vector<std::vector<Bucket>> region_buckets_;
  std::vector<std::vector<Bucket>> region_buckets_on_entry_;

  Region(char *region, size_t num_buckets);
  ~Region();
  void register_datastruct();
  void on_region_entry();
  void on_region_exit();
  void print_csv(FILE *csv_out);

private:
};

// TODO: should use some smart pointer here but PIN stl port does not really
// provide it... ?!
extern std::unordered_map<void *, Region *> g_regions;
