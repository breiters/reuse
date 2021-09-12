#pragma once

#include "memoryblock.h"
#include <list>
#include <string>
#include <vector>

#define BUCKET_INF_DIST 0

class Bucket {
public:
  Bucket();
  Bucket(int m);

  unsigned long aCount;
  unsigned long aCount_excl;
  unsigned int min;
  Marker marker;

  void add_sub(const Bucket &addend, const Bucket &minuend);
};

extern std::vector<Bucket> g_buckets;
