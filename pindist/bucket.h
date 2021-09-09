#pragma once

#include <list>
#include <string>
#include <vector>
#include "memoryblock.h"

class Bucket {
public:
  Bucket();
  Bucket(int m);

  using Marker = std::list<MemoryBlock>::iterator;

  unsigned long aCount;
  unsigned int min;
  Marker marker;

  // one marker and (exclusive) access counter for each datastruct
  std::vector<unsigned long> ds_aCount;
  std::vector<unsigned long> ds_aCount_excl;
  std::vector<Marker> ds_markers;

  void register_datastruct();
  void print_csv(const char *region);
};

extern std::vector<Bucket> buckets;
