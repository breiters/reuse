#include "region.h"
#include "bucket.h"
#include "cachesim.h"
#include "datastructs.h"
#include <cassert>
#include <cstring>
#include <cxxabi.h>
#include <iostream>

std::unordered_map<char *, Region *> g_regions;

Region::Region(char *region) : region_{strdup(region)} {
  // add region buckets for every CacheSim
  // init with zero
  size_t num_cachesims = CacheSim::cachesims.size();

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
  size_t cs_num = 0;
  for (auto &cs : CacheSim::cachesims) {
    buckets_entry_[cs_num] = cs.buckets();
    cs_num++;
  }
}

void Region::on_region_exit() {
  // std::cout << "exit region: " << region_ << "\n";
  size_t cs_num = 0;
  for (auto &cs : CacheSim::cachesims) {
    size_t b = 0;
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
  size_t cs_num = 0;
  for (auto &cs : CacheSim::cachesims) {
    cs.print_csv(csv_out, region_, buckets_[cs_num]);
    cs_num++;
  }
}
