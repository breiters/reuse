
#include "datastructs.h"
#include "bucket.h"
#include "cachesim.h"
#include "region.h"

#include <cassert>
#include <vector>

std::vector<std::vector<int>> Datastruct::indices_of;
std::vector<Datastruct> Datastruct::datastructs;

/** combine 1 level (pairs) TODO: combine N levels? **/
[[maybe_unused]] void Datastruct::combine(int ds_idx, [[maybe_unused]] int level) {
  static bool first = true;
  if (first) {
    first = false;
    return;
  }

  int ds_idx_other = 0;
  for ([[maybe_unused]] auto &ds : Datastruct::datastructs) {
    g_cachesims.push_back(new CacheSim{});

    int cs_idx = static_cast<int>(g_cachesims.size() - 1);
    // int cs_idx = static_cast<int>(g_cachesims.size());
    indices_of[ds_idx].push_back(cs_idx);
    indices_of[ds_idx_other].push_back(cs_idx);

    g_cachesims.back()->add_datastruct(ds_idx);
    g_cachesims.back()->add_datastruct(ds_idx_other);

    for (auto &key_value : g_regions) {
      key_value.second->register_datastruct();
    }

    ds_idx_other++;
  }
}

void Datastruct::register_datastruct(Datastruct &info) {
  Datastruct::datastructs.push_back(info);
  [[maybe_unused]] int ds_idx = static_cast<int>(Datastruct::datastructs.size() - 1);

#if RD_DATASTRUCTS
  g_cachesims.push_back(new CacheSim{});
  g_cachesims.back()->add_datastruct(ds_idx);
  [[maybe_unused]] int cs_idx = static_cast<int>(g_cachesims.size() - 1);
  assert(cs_idx != 0);

  eprintf("new datastruct\n");

  indices_of.push_back(std::vector<int>{});
  indices_of[ds_idx].push_back(cs_idx);

  for (auto &key_value : g_regions) {
    key_value.second->register_datastruct();
  }

#if RD_COMBINED_DATASTRUCTS
  combine(ds_idx, 1);
#endif /* RD_COMBINED_DATASTRUCTS */

#endif /* RD_DATASTRUCTS */
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
