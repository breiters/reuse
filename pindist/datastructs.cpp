
#include "datastructs.h"
#include "bucket.h"
#include "cachesim.h"
#include "region.h"

#include <cassert>
#include <vector>

std::vector<std::vector<int>> Datastruct::indices_of;
std::vector<Datastruct> Datastruct::datastructs;

/** combine 1 level (pairs) and 2 level (triplet) TODO: recursion to combine N levels? **/
[[maybe_unused]] void Datastruct::combine(int ds_idx, [[maybe_unused]] int level) {

  if (datastructs[ds_idx].nbytes < RD_COMBINE_THRESHOLD) {
    return;
  }

  for (int ds_idx_other1 = 0; ds_idx_other1 < (int)Datastruct::datastructs.size() - 1; ds_idx_other1++) {
    if (Datastruct::datastructs[ds_idx_other1].nbytes < RD_COMBINE_THRESHOLD) {
      continue;
    }

    g_cachesims.push_back(new CacheSim{0});

    int cs_idx = static_cast<int>(g_cachesims.size() - 1);

    indices_of[ds_idx].push_back(cs_idx);
    indices_of[ds_idx_other1].push_back(cs_idx);

    g_cachesims.back()->add_datastruct(ds_idx);
    g_cachesims.back()->add_datastruct(ds_idx_other1);

    for (auto &key_value : g_regions) {
      key_value.second->register_datastruct();
    }

    // add complement of current combined address space as datastruct
    g_cachesims.push_back(new CacheSim{cs_idx});
    for (int i = 0; i < ds_idx; i++) {
      if (i != ds_idx_other1) {
        g_cachesims.back()->add_datastruct(i);
        indices_of[i].push_back(cs_idx + 1);
      }
    }

#if COMBINE_THREE_DATASTRUCTS
    for (int ds_idx_other2 = ds_idx_other1 + 1; ds_idx_other2 < (int)Datastruct::datastructs.size() - 1;
         ds_idx_other2++) {

      if (Datastruct::datastructs[ds_idx_other2].nbytes < RD_COMBINE_THRESHOLD) {
        continue;
      }

      g_cachesims.push_back(new CacheSim{false});

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
#endif
  }
}

void Datastruct::register_datastruct(Datastruct &info) {
  if (info.nbytes < RD_DATASTRUCT_THRESHOLD) {
    // printf("datastruct too small\n");
    // return; // TODO!!
  }

  Datastruct::datastructs.push_back(info);
  int ds_idx = static_cast<int>(Datastruct::datastructs.size() - 1);

  g_cachesims.push_back(new CacheSim{0});
  g_cachesims.back()->add_datastruct(ds_idx);
  int cs_idx = static_cast<int>(g_cachesims.size() - 1);

  eprintf("new datastruct\n");

  indices_of.push_back(std::vector<int>{});
  indices_of[ds_idx].push_back(cs_idx);

  for (auto &key_value : g_regions) {
    key_value.second->register_datastruct();
  }

  if (ds_idx != RD_NO_DATASTRUCT) {
    // register datastruct on all other complements
    int idx = 0;
    for (auto &cs : g_cachesims) {
      if (cs->is_complement()) {
        cs->add_datastruct(ds_idx);
        indices_of[ds_idx].push_back(idx);
      }
      idx++;
    }

    // add complement of current address space as datastruct
    g_cachesims.push_back(new CacheSim{cs_idx});
    for (int i = 0; i < ds_idx; i++) {
      g_cachesims.back()->add_datastruct(i);
      indices_of[i].push_back(cs_idx + 1);
    }
    for (auto &key_value : g_regions) {
      // for complement of address space:
      key_value.second->register_datastruct();
    }

#if RD_COMBINED_DATASTRUCTS
    combine(ds_idx, 1);
#endif /* RD_COMBINED_DATASTRUCTS */
  }

  /*
    for(size_t i = 0; i < datastructs.size(); i++) {
      printf("ds_idx %lu contained in: \n", i);
    for(auto cs : indices_of[i]) {
      printf("%d\n", cs);
    }}*/
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
