#include "region.h"
#include "bucket.h"
#include "cachesim.h"
#include "datastructs.h"
#include <cstring>
#include <cxxabi.h>

std::unordered_map<void *, Region *> g_regions;

Region::Region(char *region, size_t num_buckets) : region_{strdup(region)} {

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

  // b.print_csv("init");

#if 0
  for (auto b : g_buckets) {
    b.aCount = 0;
    for (auto &a : b.data) {
      a.access_count = 0;
      a.access_count_excl = 0;
    }
    region_buckets_.push_back(b);
    region_buckets_on_entry_.push_back(b);
  }
#endif
}

Region::~Region() {
  // std::cout << "free region: " << region_ << "\n";
#if 0
  free(region_);
  region_ = nullptr;
#endif
}

void Region::register_datastruct() {
  region_buckets_.push_back(std::vector<Bucket>{g_buckets.size()});
  region_buckets_on_entry_.push_back(std::vector<Bucket>{g_buckets.size()});
}

void Region::on_region_entry() {
  // std::cout << "entry region: " << region_ << "\n";
  // make snapshot of current buckets access counts
  size_t i = 0;
  for (auto &cs : g_cachesims) {
    region_buckets_on_entry_[i] = cs.buckets_;
    i++;
  }
}

void Region::on_region_exit() {
  // std::cout << "exit region: " << region_ << "\n";
  size_t i = 0;
  for (auto &cs : g_cachesims) {
    size_t j = 0;
    for (auto &b : cs.buckets_)  {
      region_buckets_[i][j].add_sub();
    }
    i++;
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
  for (auto &b : region_buckets_) {
    b.print_csv(region_, csv_out);
  }
}
