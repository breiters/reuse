#pragma once

#include "memoryblock.h"
#include <vector>

#define BUCKET_INF_DIST 0

extern std::vector<int> g_bucket_mins;

class Bucket {
public:
  Bucket();
  Bucket(int m);

  unsigned long aCount;
  unsigned long aCount_excl;
  unsigned int min;
  Marker marker;

  // void add_sub(const Bucket &addend, const Bucket &minuend);
};
