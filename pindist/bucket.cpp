#include "bucket.h"
#include "cachesim.h"
#include "datastructs.h"
#include "memoryblock.h"
#include "region.h"

#include <cassert>
#include <limits>

extern std::list<MemoryBlock> g_stack;

Bucket::Bucket() {}

Bucket::Bucket(int m) {
  aCount_excl = 0;
  aCount = 0;
  min = m;
  marker = g_stack.end();
}

#if 0
void Bucket::add_sub(const Bucket &add, const Bucket &sub) {
  aCount += add.aCount - sub.aCount;
  aCount_excl += add.aCount_excl - sub.aCount_excl;
}
#endif
