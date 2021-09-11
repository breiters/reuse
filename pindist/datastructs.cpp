
#include "datastructs.h"
#include "bucket.h"
#include "cachesim.h"
#include "region.h"

#include <cassert>
#include <vector>

std::vector<DatastructInfo> g_datastructs;
// extern std::vector<Bucket> g_buckets;

// combine 1 level
[[maybe_unused]] static void combine(int ds_num, [[maybe_unused]] int level) {
  static bool first = true;
  if (first) {
    first = false;
    return;
  }

  int ds1 = 0;

  for ([[maybe_unused]] auto &ds : g_datastructs) {
    g_cachesims_combined.push_back(
        CacheSim{static_cast<int>(g_cachesims_combined.size())});
    g_cachesims_combined.rbegin()->add_datastruct(ds1);
    g_cachesims_combined.rbegin()->add_datastruct(ds_num);

    printf("combining: %d, %d\n", ds1, ds_num);

    for (auto &bucket : g_buckets) {
      bucket.register_combined_datastruct();
    }

    for (auto &region : g_regions) {
      region.second->register_combined_datastruct();
    }
    ds1++;
  }
}

void register_datastruct(DatastructInfo &info) {
  // combine(static_cast<int>(g_datastructs.size()), 1);

  g_datastructs.push_back(info);
  g_cachesims.push_back(CacheSim{static_cast<int>(g_datastructs.size()) - 1});

  for (auto &bucket : g_buckets) {
    bucket.register_datastruct();
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

/*
std::vector<int> datstruct_nums(Addr addr) {
  std::vector<int> v;
  v.push_back(datstruct_num(addr));
  return v; // TODO!
}
*/