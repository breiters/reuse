#pragma once

#include "memoryblock.h"
#include <list>
#include <string>
#include <vector>

class Bucket {
public:
  Bucket();
  Bucket(int m);

  unsigned long aCount;
  unsigned int min;
  Marker marker;

  // one marker and (exclusive) access counter for each datastruct
  std::vector<unsigned long> ds_aCount;
  std::vector<unsigned long> ds_aCount_excl;
  std::vector<Marker> ds_markers;

  void register_datastruct();
  void print_csv(const char *region);
  void add_sub(const Bucket &other_add, const Bucket &other_sub);
};

extern std::vector<Bucket> g_buckets;
