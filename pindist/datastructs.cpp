
#include "datastructs.h"
#include "bucket.h"
#include "cachesim.h"
#include "region.h"

#include <cassert>
#include <vector>

std::vector<std::vector<int>> ds_in_cs;
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
    printf("combining: %d, %d in csnum: %d\n", ds1, ds_num, (int)g_cachesims_combined.size());
    ds_in_cs[ds_num].push_back((int)g_cachesims_combined.size());
    ds_in_cs[ds1].push_back((int)g_cachesims_combined.size());
    g_cachesims_combined.push_back(
      // CacheSim{static_cast<int>(g_cachesims_combined.size())}); // ???
        CacheSim{static_cast<int>(g_cachesims_combined.size() - 1)});

    // g_cachesims_combined.back().add_datastruct(ds1);
    // g_cachesims_combined.back().add_datastruct(ds_num);

    g_cachesims_combined[g_cachesims_combined.size() - 1].add_datastruct(ds1);
    g_cachesims_combined[g_cachesims_combined.size() - 1].add_datastruct(ds_num);
    ds1++;
  }

}

void register_datastruct(DatastructInfo &info) {
  ds_in_cs.push_back(std::vector<int>{});
  combine(static_cast<int>(g_datastructs.size()), 1);
  g_datastructs.push_back(info);
  g_cachesims.push_back(CacheSim{static_cast<int>(g_datastructs.size()) - 1});


#if DEBUG_LEVEL > 0
  int ds_num = 0;
  for(auto ds : ds_in_cs) {
    printf("ds %d in: \n", ds_num);
    for(int cs : ds) {
      printf(" %d ", cs);
    }
    printf("\n");
    ds_num++;
  }
#endif /* DEBUG */
}

// TODO: use better algorithm to get datastruct
int datastruct_num(Addr addr) {
  int i = 0;
  for (auto &ds : g_datastructs) {
    if ((uint64_t)addr >= (uint64_t)ds.address &&
        (uint64_t)addr < (uint64_t)ds.address + (uint64_t)ds.nbytes) {
      return i;
    }
    i++;
  }
  return RD_NO_DATASTRUCT;
}

/*
std::vector<int> datstruct_nums(Addr addr) {
  std::vector<int> v;
  v.push_back(datstruct_num(addr));
  return v; // TODO!
}
*/