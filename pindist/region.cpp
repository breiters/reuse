#include "region.h"
#include "bucket.h"
#include "cachesim.h"
#include "datastructs.h"
#include <cassert>
#include <cstring>
#include <cxxabi.h>
#include <iostream>

/** TODO: COUNT FOR G_BUCKETS TOO **/

std::unordered_map<char *, Region *> g_regions;

Region::Region(char *region) : region_{strdup(region)} {
#if 0
	int status = -1;
	char *demangled_name = abi::__cxa_demangle(region, NULL, NULL, &status);
	// printf("Demangled: %s\n", demangled_name);

    // set region name to the demangled name without arguments if it was a mangled function
    if(nullptr != demangled_name) {
        region_ = demangled_name;
        strtok(region_, "(");
    } else {
        region_ = strdup(region);
    }
    std::cout << "new region: " << region_ << "\n";
#endif

  // add region buckets for every CacheSim
  // init with zero
  region_buckets_.reserve(g_cachesims.size() + g_cachesims_combined.size());
  region_buckets_on_entry_.reserve(g_cachesims.size() +
                                   g_cachesims_combined.size());

  for (auto &cs : g_cachesims) {
    region_buckets_.push_back(new Bucket[cs.buckets().size()]()); // zero
  }
  for (auto &cs : g_cachesims_combined) {
    region_buckets_.push_back(new Bucket[cs.buckets().size()]()); // zero
  }
}

Region::~Region() {
  // std::cout << "free region: " << region_ << "\n";
  free(region_);
  region_ = nullptr;
  for (auto rb : region_buckets_) {
    delete[] rb;
  }
  for (auto rb : region_buckets_on_entry_) {
    delete[] rb;
  }
}

void Region::on_region_entry() {
  std::cout << "entry region: " << region_ << "\n";
  // make snapshot of current buckets access counts
  size_t cs_num = 0;
  for (auto &cs : g_cachesims) {
    region_buckets_on_entry_.push_back(
        new Bucket[cs.buckets().size()]()); // zero
    size_t b = 0;
    for (auto &buck : cs.buckets()) {
      region_buckets_on_entry_[cs_num][b].aCount = buck.aCount;
      region_buckets_on_entry_[cs_num][b].aCount_excl = buck.aCount_excl;
      b++;
    }
    cs_num++;
  }
  for (auto &cs : g_cachesims_combined) {
    region_buckets_on_entry_.push_back(
        new Bucket[cs.buckets().size()]()); // zero
    size_t b = 0;
    for (auto &buck : cs.buckets()) {
      region_buckets_on_entry_[cs_num][b].aCount = buck.aCount;
      region_buckets_on_entry_[cs_num][b].aCount_excl = buck.aCount_excl;
      b++;
    }
    cs_num++;
  }
}

void Region::on_region_exit() {
  std::cout << "exit region: " << region_ << "\n";
  size_t cs_num = 0;
  for (auto &cs : g_cachesims) {
    size_t b = 0;
    for (auto &buck : cs.buckets()) {
      region_buckets_[cs_num][b].aCount +=
          buck.aCount - region_buckets_on_entry_[cs_num][b].aCount;
      region_buckets_[cs_num][b].aCount_excl +=
          buck.aCount_excl - region_buckets_on_entry_[cs_num][b].aCount_excl;
      region_buckets_[cs_num][b].min = buck.min;
      b++;
    }
    cs_num++;
  }
  for (auto &cs : g_cachesims_combined) {
    size_t b = 0;
    for (auto &buck : cs.buckets()) {
      region_buckets_[cs_num][b].aCount +=
          buck.aCount - region_buckets_on_entry_[cs_num][b].aCount;
      region_buckets_[cs_num][b].aCount_excl +=
          buck.aCount_excl - region_buckets_on_entry_[cs_num][b].aCount_excl;
      region_buckets_[cs_num][b].min = buck.min;
      b++;
    }
    cs_num++;
  }
}

void Region::print_csv(FILE *csv_out) {

  size_t cs_num = 0;
  for (auto &cs : g_cachesims) {
    cs.print_csv(csv_out, region_, region_buckets_[cs_num],
                 cs.buckets().size());
    cs_num++;
  }
  for (auto &cs : g_cachesims_combined) {
    cs.print_csv(csv_out, region_, region_buckets_[cs_num],
                 cs.buckets().size());
    cs_num++;
  }
}

#if USE_MAPS
void Region::on_region_entry() {
  std::cout << "entry region: " << region_ << "\n";
  // make snapshot of current buckets access counts
  for (auto &cs : g_cachesims) {
    region_buckets_on_entry_[cs] = cs.buckets();
    assert(region_buckets_.find(cs) != region_buckets_.end());
  }
  for (auto &cs : g_cachesims_combined) {
    region_buckets_on_entry_[cs] = cs.buckets();
    assert(region_buckets_.find(cs) != region_buckets_.end());
  }
}

void Region::on_region_exit() {
  std::cout << "exit region: " << region_ << "\n";
  for (auto &cs : g_cachesims) {
    assert(region_buckets_.find(cs) != region_buckets_.end());
    size_t bucket = 0;
    for (auto &rb : region_buckets_[cs]) {
      rb.add_sub(g_buckets[bucket], region_buckets_on_entry_[cs][bucket]);
      bucket++;
    }
    assert(region_buckets_.find(cs) != region_buckets_.end());
    for (auto &cs : g_cachesims_combined) {
      assert(region_buckets_.find(cs) != region_buckets_.end());
      size_t bucket = 0;
      for (auto &rb : region_buckets_[cs]) {
        rb.add_sub(g_buckets[bucket], region_buckets_on_entry_[cs][bucket]);
        bucket++;
      }
    }
  }
#if 0
  size_t b = 0;
  for (Bucket &rb : region_buckets_) {
    rb.add_sub(g_buckets[b], region_buckets_on_entry_[b]);
    b++;
  }
#endif
}

void Region::print_csv(FILE *csv_out) {
  for (auto &cs : g_cachesims) {
    assert(region_buckets_.find(cs) != region_buckets_.end());
    cs.print_csv(csv_out, region_, region_buckets_[cs]);
  }
  for (auto &cs : g_cachesims_combined) {
    assert(region_buckets_.find(cs) != region_buckets_.end());
    cs.print_csv(csv_out, region_, region_buckets_[cs]);
  }
}

#endif // USE_MAPS