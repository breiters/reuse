#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "bucket.h"
#include "cachesim.h"

class Region {
public:
  char *region_;

  std::vector<std::vector<Bucket>> buckets_;
  std::vector<std::vector<Bucket>> buckets_entry_;

  Region(char *region);
  ~Region();
  void register_datastruct();
  void on_region_entry();
  void on_region_exit();
  void demangle_name();
  void print_csv(FILE *csv_out);

private:
};

// should use unique_ptr here but PIN stl port does not really provide it.
extern std::unordered_map<char *, Region *> g_regions;
