
#include "datastructs.h"
#include "bucket.h"
#include "cachesim.h"
#include "region.h"

#include <cassert>
#include <vector>

std::vector<std::vector<int>> g_csindices_of_ds;
std::vector<DatastructInfo> g_datastructs;

/** combine 1 level (pairs) TODO: combine N levels **/
[[maybe_unused]] static void combine(int ds_num, [[maybe_unused]] int level) {
  static bool first = true;
  if (first) {
    first = false;
    return;
  }

  int ds1 = 0;
  for ([[maybe_unused]] auto &ds : g_datastructs) {
    // printf("combining: %d, %d in csnum: %d\n", ds1, ds_num, (int)g_cachesims_combined.size());
    int cs_num = static_cast<int>(g_cachesims_combined.size() - 1);

    g_csindices_of_ds[ds_num].push_back(cs_num + 1);
    g_csindices_of_ds[ds1].push_back(cs_num + 1);

    g_cachesims_combined.push_back(CacheSim{cs_num});
    g_cachesims_combined.back().add_datastruct(ds1);
    g_cachesims_combined.back().add_datastruct(ds_num);
    ds1++;
  }
}

void register_datastruct(DatastructInfo &info) {
  g_csindices_of_ds.push_back(std::vector<int>{});
  
  int num_datastructs = static_cast<int>(g_datastructs.size());
  g_cachesims.push_back(CacheSim{num_datastructs});

  combine(num_datastructs, 1);
  
  g_datastructs.push_back(info);

#if DEBUG_LEVEL > 0
  int ds_num = 0;
  for (auto ds : g_csindices_of_ds) {
    printf("ds %d in: \n", ds_num);
    for (int cs : ds) {
      printf(" %d ", cs);
    }
    printf("\n");
    ds_num++;
  }
#endif /* DEBUG_LEVEL */
}

/** TODO: maybe use better algorithm to get datastruct **/
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
