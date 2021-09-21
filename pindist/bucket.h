#pragma once

#include "memoryblock.h"
#include <vector>

#define BUCKET_INF_DIST 0

class Bucket {
public:
  Bucket() : aCount{0}, aCount_excl{0} {}
  unsigned long aCount;
  unsigned long aCount_excl;
  StackIterator marker;

  static std::vector<unsigned> mins;

  inline void add_sub(const Bucket &add, const Bucket &sub) {
    aCount += add.aCount - sub.aCount;
    aCount_excl += add.aCount_excl - sub.aCount_excl;
  }
};
