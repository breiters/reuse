#include "region.h"
#include "bucket.h"
#include "cachesim.h"
#include "datastructs.h"
#include <cassert>
#include <cstring>
#include <cxxabi.h>
#include <iostream>

std::unordered_map<char *, Region *> g_regions;

Region::Region(char *region) : region_{strdup(region)}, global_buckets_{std::vector<Bucket>{Bucket::mins.size()}} {
  // add region buckets for every CacheSim
  // init with zero
  size_t num_cachesims = g_cachesims.size() + g_cachesims_combined.size();

  buckets_.reserve(num_cachesims);
  buckets_entry_.reserve(num_cachesims);

  for (size_t i = 0; i < num_cachesims; i++) {
    buckets_.push_back(std::vector<Bucket>{Bucket::mins.size()});
    buckets_entry_.push_back(std::vector<Bucket>{Bucket::mins.size()});
  }
}

Region::~Region() {
  // std::cout << "free region: " << region_ << "\n";
  free(region_);
  region_ = nullptr;
}

void Region::on_region_entry() {
  // std::cout << "entry region: " << region_ << "\n";
  // make snapshot of current buckets access counts
  global_buckets_entry_ = g_cachesim.buckets();

  size_t cs_num = 0;
  for (auto &cs : g_cachesims) {
    buckets_entry_[cs_num] = cs.buckets();
    cs_num++;
  }
  for (auto &cs : g_cachesims_combined) {
    buckets_entry_[cs_num] = cs.buckets();
    cs_num++;
  }
}

void Region::on_region_exit() {
  // std::cout << "exit region: " << region_ << "\n";
  size_t b = 0;
  for (auto &bucket : g_cachesim.buckets()) {
    global_buckets_[b].add_sub(bucket, global_buckets_entry_[b]);
    b++;
  }

  size_t cs_num = 0;
  for (auto &cs : g_cachesims) {
    b = 0;
    for (auto &bucket : cs.buckets()) {
      buckets_[cs_num][b].add_sub(bucket, buckets_entry_[cs_num][b]);
      b++;
    }
    cs_num++;
  }
  for (auto &cs : g_cachesims_combined) {
    b = 0;
    for (auto &bucket : cs.buckets()) {
      buckets_[cs_num][b].add_sub(bucket, buckets_entry_[cs_num][b]);
      b++;
    }
    cs_num++;
  }
}

void Region::register_datastruct() {
  buckets_.push_back(std::vector<Bucket>{Bucket::mins.size()});
  buckets_entry_.push_back(std::vector<Bucket>{Bucket::mins.size()});
}

void Region::demangle_name() {
  int status = -1;
  char *demangled_name = abi::__cxa_demangle(region_, NULL, NULL, &status);
  // printf("Demangled: %s\n", demangled_name);

  // set region name to the demangled name without arguments if it was a mangled function
  if (nullptr != demangled_name) {
    free(region_);
    region_ = demangled_name;
    strtok(region_, "(");
  }
  // std::cout << "new region: " << region_ << "\n";
}

void Region::print_csv(FILE *csv_out) {
  demangle_name();
  g_cachesim.print_csv(csv_out, region_, global_buckets_);
  size_t cs_num = 0;
  for (auto &cs : g_cachesims) {
    cs.print_csv(csv_out, region_, buckets_[cs_num]);
    cs_num++;
  }
  for (auto &cs : g_cachesims_combined) {
    cs.print_csv(csv_out, region_, buckets_[cs_num]);
    cs_num++;
  }
}

#if 0
Region::Region(char *region) : region_{strdup(region)} {
  // add region buckets for every CacheSim
  // init with zero
  global_buckets_ = g_cachesim.buckets();
  for (auto &b : global_buckets_) {
    b.aCount = 0;
    b.aCount_excl = 0;
  }

  buckets_.reserve(g_cachesims.size() + g_cachesims_combined.size());
  buckets_entry_.reserve(g_cachesims.size() + g_cachesims_combined.size());

  for (auto &cs : g_cachesims) {
    buckets_.push_back(new Bucket[cs.buckets().size()]()); // zero
  }
  for (auto &cs : g_cachesims_combined) {
    buckets_.push_back(new Bucket[cs.buckets().size()]()); // zero
  }
}

void Region::demangle_name() {
  int status = -1;
  char *demangled_name = abi::__cxa_demangle(region_, NULL, NULL, &status);
  // printf("Demangled: %s\n", demangled_name);

  // set region name to the demangled name without arguments if it was a mangled function
  if (nullptr != demangled_name) {
    free(region_);
    region_ = demangled_name;
    strtok(region_, "(");
  }
  // std::cout << "new region: " << region_ << "\n";
}

Region::~Region() {
  // std::cout << "free region: " << region_ << "\n";
  free(region_);
  region_ = nullptr;
  for (auto rb : buckets_) {
    delete[] rb;
  }
  for (auto rb : buckets_entry_) {
    delete[] rb;
  }
}

void Region::on_region_entry() {
  // std::cout << "entry region: " << region_ << "\n";
  // make snapshot of current buckets access counts
  global_buckets_entry_ = g_cachesim.buckets();

  size_t cs_num = 0;
  for (auto &cs : g_cachesims) {
    buckets_entry_.push_back(new Bucket[cs.buckets().size()]()); // zero
    size_t b = 0;
    for (auto &buck : cs.buckets()) {
      buckets_entry_[cs_num][b].aCount = buck.aCount;
      buckets_entry_[cs_num][b].aCount_excl = buck.aCount_excl;
      b++;
    }
    cs_num++;
  }
  for (auto &cs : g_cachesims_combined) {
    buckets_entry_.push_back(new Bucket[cs.buckets().size()]()); // zero
    size_t b = 0;
    for (auto &buck : cs.buckets()) {
      buckets_entry_[cs_num][b].aCount = buck.aCount;
      buckets_entry_[cs_num][b].aCount_excl = buck.aCount_excl;
      b++;
    }
    cs_num++;
  }
}

void Region::on_region_exit() {
  // std::cout << "exit region: " << region_ << "\n";
  size_t cs_num = 0;

  size_t b = 0;
  for (auto buck : global_buckets_) {
    buck.aCount += g_cachesim.buckets()[b].aCount - global_buckets_entry_[b].aCount;
    b++;
  }

  for (auto &cs : g_cachesims) {
    size_t b = 0;
    for (auto &buck : cs.buckets()) {
      buckets_[cs_num][b].aCount += buck.aCount - buckets_entry_[cs_num][b].aCount;
      buckets_[cs_num][b].aCount_excl += buck.aCount_excl - buckets_entry_[cs_num][b].aCount_excl;
      b++;
    }
    cs_num++;
  }
  for (auto &cs : g_cachesims_combined) {
    size_t b = 0;
    for (auto &buck : cs.buckets()) {
      buckets_[cs_num][b].aCount += buck.aCount - buckets_entry_[cs_num][b].aCount;
      buckets_[cs_num][b].aCount_excl += buck.aCount_excl - buckets_entry_[cs_num][b].aCount_excl;
      b++;
    }
    cs_num++;
  }
}

void Region::print_csv(FILE *csv_out) {
  demangle_name();
  g_cachesim.print_csv(csv_out, region_, global_buckets_);
  size_t cs_num = 0;
  for (auto &cs : g_cachesims) {
    cs.print_csv(csv_out, region_, buckets_[cs_num], cs.buckets().size());
    cs_num++;
  }
  for (auto &cs : g_cachesims_combined) {
    cs.print_csv(csv_out, region_, buckets_[cs_num], cs.buckets().size());
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
#endif