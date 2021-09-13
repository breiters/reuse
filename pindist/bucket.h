#pragma once

#include "memoryblock.h"
#include <vector>

#define BUCKET_INF_DIST 0

class Bucket {
public:
  Bucket();
  unsigned long aCount;
  unsigned long aCount_excl;
  Marker marker;

  static std::vector<unsigned> mins;

  void add_sub(const Bucket &addend, const Bucket &minuend);
};
