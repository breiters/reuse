
#include "datastructs.h"
#include "bucket.h"
#include "cachesim.h"
#include "region.h"

#include <cassert>
#include <vector>

std::vector<std::vector<int>> Datastruct::indices_of;
std::vector<Datastruct> Datastruct::datastructs;

/** combine 1 level (pairs) TODO: recursion to combine N levels? do not use small datastructs **/
[[maybe_unused]] void Datastruct::combine(int ds_idx, [[maybe_unused]] int level) {

  if (datastructs[ds_idx].nbytes < RD_COMBINE_THRESHOLD) {
    return;
  }

  for (int ds_idx_other1 = 0; ds_idx_other1 < (int)Datastruct::datastructs.size() - 1; ds_idx_other1++) {
    if (Datastruct::datastructs[ds_idx_other1].nbytes < RD_COMBINE_THRESHOLD) {
      continue;
    }

    g_cachesims.push_back(new CacheSim{});

    int cs_idx = static_cast<int>(g_cachesims.size() - 1);

    indices_of[ds_idx].push_back(cs_idx);
    indices_of[ds_idx_other1].push_back(cs_idx);

    g_cachesims.back()->add_datastruct(ds_idx);
    g_cachesims.back()->add_datastruct(ds_idx_other1);

    for (auto &key_value : g_regions) {
      key_value.second->register_datastruct();
    }

    for (int ds_idx_other2 = ds_idx_other1 + 1; ds_idx_other2 < (int)Datastruct::datastructs.size() - 1;
         ds_idx_other2++) {
           
      if (Datastruct::datastructs[ds_idx_other2].nbytes < RD_COMBINE_THRESHOLD) {
        continue;
      }

      g_cachesims.push_back(new CacheSim{});

      int cs_idx = static_cast<int>(g_cachesims.size() - 1);

      indices_of[ds_idx].push_back(cs_idx);
      indices_of[ds_idx_other1].push_back(cs_idx);
      indices_of[ds_idx_other2].push_back(cs_idx);

      g_cachesims.back()->add_datastruct(ds_idx);
      g_cachesims.back()->add_datastruct(ds_idx_other1);
      g_cachesims.back()->add_datastruct(ds_idx_other2);

      for (auto &key_value : g_regions) {
        key_value.second->register_datastruct();
      }
    }
  }
}

void Datastruct::register_datastruct(Datastruct &info) {
#if RD_DATASTRUCTS
  if (info.nbytes < RD_DATASTRUCT_THRESHOLD) {
    printf("datastruct too small\n");
    return;
  }

  Datastruct::datastructs.push_back(info);
  int ds_idx = static_cast<int>(Datastruct::datastructs.size() - 1);

  g_cachesims.push_back(new CacheSim{});
  g_cachesims.back()->add_datastruct(ds_idx);
  int cs_idx = static_cast<int>(g_cachesims.size() - 1);
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

/** TODO: maybe use better algorithm to get datastruct (sort datastructs by address) **/
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
