#include "bucket.h"
#include "cachesim.h"
#include "datastructs.h"
#include "memoryblock.h"
#include "region.h"

#include <cassert>
#include <limits>

extern std::list<MemoryBlock *> g_stack;

Bucket::Bucket(int m) {
  aCount_excl = 0;
  aCount = 0;
  min = m;
  marker = g_stack.end();
}

void Bucket::add_sub(const Bucket &add, const Bucket &sub) {
  aCount += add.aCount - sub.aCount;
  aCount_excl += add.aCount_excl - sub.aCount_excl;
}

#define CSV_FORMAT "%s,%p,%zu,%d,%lu,%s,%u,%lu,%lu,%lu\n"

void Bucket::print_csv(const char *region, FILE *csv_out) {
  // original code has min = 0 to define infinite distance
  // change to max limit to distinguish buckets zero from inf
#if 0
  if (this == &g_buckets[g_buckets.size() - 1]) {
    min = std::numeric_limits<decltype(min)>::max();
  }

  for (auto &pair : g_regions) {
    if (pair.second->region_ == region &&
        this ==
            &pair.second
                 ->region_buckets_[pair.second->region_buckets_.size() - 1]) {
      min = std::numeric_limits<decltype(min)>::max();
    }
  }

  fprintf(csv_out, CSV_FORMAT, region, (void *)0x0, (size_t)0, 0, 0UL, "main",
          min, aCount, 0UL, 0UL);

  int ds_num = 0;
  for (auto &ds : g_datastructs) {
    fprintf(csv_out, CSV_FORMAT, region, ds.address, ds.nbytes, ds.line,
            ds.access_count, ds.file_name.c_str(), min, aCount,
            g_cachesims[ds_num].access_count, data[ds_num].access_count_excl);
    ds_num++;
  }
#endif

#if 0

  unsigned long i = 0;
  for ([[maybe_unused]] auto &cs : g_cachesims_combined) {
    fprintf(csv_out, CSV_FORMAT, region, (void *)i, (size_t)0, 0, 0UL, "combined", min, aCount,
            accounting_combined[i].access_count,
            accounting_combined[i].access_count_excl);
    i++;
  }
#endif

  fflush(csv_out);
}
