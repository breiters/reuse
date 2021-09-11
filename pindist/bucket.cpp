#include "bucket.h"
#include "cachesim.h"
#include "datastructs.h"
#include "memoryblock.h"
#include "region.h"

#include <cassert>
#include <limits>

extern std::list<MemoryBlock *> g_stack;

Bucket::Bucket(int m) {
  aCount = 0;
  min = m;
  marker = g_stack.end();
}

void Bucket::add_sub(const Bucket &add, const Bucket &sub) {
  aCount += add.aCount - sub.aCount;
  for (size_t ds = 0; ds < accounting.size(); ds++) {
    accounting[ds].access_count =
        add.accounting[ds].access_count - sub.accounting[ds].access_count;
    accounting[ds].access_count_excl = add.accounting[ds].access_count_excl -
                                       sub.accounting[ds].access_count_excl;
  }
}

// TODO: duplicated code!
void Bucket::register_combined_datastruct() {
  accounting_combined.push_back((AccessStats){
      .access_count = 0,
      .access_count_excl = 0,
      .marker = g_cachesims_combined[g_cachesims_combined.size() - 1].stack().end()});
}

void Bucket::register_datastruct() {
  accounting.push_back((AccessStats){
      .access_count = 0,
      .access_count_excl = 0,
      .marker = g_cachesims[g_datastructs.size() - 1].stack().end()});
  // ds_aCount.push_back(0);
  // ds_aCount_excl.push_back(0);

  assert(g_datastructs.size() > 0);

  // push back a dummy marker
  // ds_markers.push_back(g_cachesims[g_datastructs.size() - 1].stack().end());
}

#define CSV_FORMAT "%s,%p,%zu,%d,%lu,%s,%u,%lu,%lu,%lu\n"

void Bucket::print_csv(const char *region, FILE *csv_out) {
  // original code has min = 0 to define infinite distance
  // change to max limit to distinguish buckets zero from inf
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
            accounting[ds_num].access_count,
            accounting[ds_num].access_count_excl);
    ds_num++;
  }

  fflush(csv_out);
}
