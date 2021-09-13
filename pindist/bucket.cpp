#include "bucket.h"

Bucket::Bucket() : aCount{0}, aCount_excl{0} {}

void Bucket::add_sub(const Bucket &add, const Bucket &sub) {
  aCount += add.aCount - sub.aCount;
  aCount_excl += add.aCount_excl - sub.aCount_excl;
}

std::vector<unsigned> Bucket::mins;