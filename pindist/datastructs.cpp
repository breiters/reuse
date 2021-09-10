
#include "bucket.h"
#include "datastructs.h"
#include "region.h"
#include "cachesim.h"

#include <cassert>
#include <vector>

std::vector<DatastructInfo> g_datastructs;

void register_datastruct(DatastructInfo &info) {
  g_datastructs.push_back(info);
  g_cachesims.push_back(CacheSim{static_cast<int>(g_datastructs.size()) - 1});

  for (auto &bucket : buckets) {
    bucket.register_datastruct();
    assert(bucket.ds_aCount.size() == g_datastructs.size());
    assert(bucket.ds_markers.size() == g_datastructs.size());
  }

  for (auto &region : g_regions) {
    region.second->register_datastruct();
  }
}

// TODO: use better algorithm to get datastruct
int datstruct_num(Addr addr) {
  int i = 0;
  for (auto &ds : g_datastructs) {
    if ((uint64_t)addr >= (uint64_t)ds.address &&
        (uint64_t)addr < (uint64_t)ds.address + (uint64_t)ds.nbytes) {
      return i;
    }
    i++;
  }
  return DATASTRUCT_UNKNOWN;
}
