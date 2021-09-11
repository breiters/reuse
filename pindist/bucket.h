#pragma once

#include "memoryblock.h"
#include <list>
#include <string>
#include <vector>

#define BUCKET_INFINITE_DISTANCE 0

class Bucket {
public:
  Bucket();
  Bucket(int m);

  unsigned long aCount;
  unsigned int min;
  Marker marker;

  // one marker and (exclusive) access counter for each datastruct
  // std::vector<unsigned long> ds_aCount;
  // std::vector<unsigned long> ds_aCount_excl;
  // std::vector<Marker> ds_markers;

  struct AccessStats {
    unsigned long access_count;
    unsigned long access_count_excl;
    // unsigned long access_count_other;
    Marker marker;
    // Marker marker_other;
  };

  std::vector<AccessStats> accounting;
  std::vector<AccessStats> accounting_combined;

  void register_datastruct();
  void register_combined_datastruct();
  void print_csv(const char *region, FILE *csv_out);
  void add_sub(const Bucket &addend, const Bucket &minuend);
};

extern std::vector<Bucket> g_buckets;
