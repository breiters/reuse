#include "bucket.h"

std::vector<unsigned> g_bucket_mins;

Bucket::Bucket() : aCount{0}, aCount_excl{0} {}
// Bucket::Bucket() {}
// Bucket::Bucket(int m) : aCount{0}, aCount_excl{0}, min{static_cast<unsigned>(m)} {}

#if 0
void Bucket::add_sub(const Bucket &add, const Bucket &sub) {
  aCount += add.aCount - sub.aCount;
  aCount_excl += add.aCount_excl - sub.aCount_excl;
}
#endif
