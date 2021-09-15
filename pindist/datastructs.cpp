
#include "datastructs.h"
#include "bucket.h"
#include "cachesim.h"
#include "region.h"

#include <cassert>
#include <vector>

std::vector<std::vector<int>> Datastruct::contained_indices;
std::vector<Datastruct> Datastruct::datastructs;

/** combine 1 level (pairs) TODO: combine N levels? **/
[[maybe_unused]] void Datastruct::combine(int ds_num, [[maybe_unused]] int level) {
  static bool first = true;
  if (first) {
    first = false;
    return;
  }

  int ds_other = 0;
  for ([[maybe_unused]] auto &ds : Datastruct::datastructs) {
    // printf("combining: %d, %d in csnum: %d\n", ds1, ds_num, (int)g_cachesims_combined.size());
    int cs_num = static_cast<int>(g_cachesims_combined.size() - 1);

    contained_indices[ds_num].push_back(cs_num + 1);
    contained_indices[ds_other].push_back(cs_num + 1);

    g_cachesims_combined.push_back(CacheSim{cs_num});
    g_cachesims_combined.back().add_datastruct(ds_other);
    g_cachesims_combined.back().add_datastruct(ds_num);

    for (auto &key_value : g_regions) {
      key_value.second->register_datastruct();
    }

    ds_other++;
  }
}

void Datastruct::register_datastruct(Datastruct &info) {
  contained_indices.push_back(std::vector<int>{});

  int num_datastructs = static_cast<int>(Datastruct::datastructs.size());
  g_cachesims.push_back(CacheSim{num_datastructs});

  for (auto &key_value : g_regions) {
    key_value.second->register_datastruct();
  }

  combine(num_datastructs, 1);

  Datastruct::datastructs.push_back(info);
}

/** TODO: maybe use better algorithm to get datastruct **/
int Datastruct::datastruct_num(Addr addr) {
  int i = 0;
  for (auto &ds : Datastruct::datastructs) {
    if ((uint64_t)addr >= (uint64_t)ds.address && (uint64_t)addr < (uint64_t)ds.address + (uint64_t)ds.nbytes) {
      return i;
    }
    i++;
  }
  return RD_NO_DATASTRUCT;
}
