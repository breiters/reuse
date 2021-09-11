#include "region.h"
#include "bucket.h"
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

  for (auto b : g_buckets) {
    b.aCount = 0;
    /*
    for (auto &a : b.ds_aCount) {
      a = 0;
    }
    for (auto &a : b.ds_aCount_excl) {
      a = 0;
    }
    */
    for (auto &a : b.accounting) {
      a.access_count = 0;
      a.access_count_excl = 0;
    }
    region_buckets_.push_back(b);
    region_buckets_on_entry_.push_back(b);
  }
}

Region::~Region() {
  // std::cout << "free region: " << region_ << "\n";
  free(region_);
  region_ = nullptr;
}

void Region::register_datastruct() {
  for (auto &bucket : region_buckets_) {
    bucket.register_datastruct();
  }
  for (auto &bucket : region_buckets_on_entry_) {
    bucket.register_datastruct();
  }
}

void Region::register_combined_datastruct() {
  for (auto &bucket : region_buckets_) {
    bucket.register_combined_datastruct();
  }
  for (auto &bucket : region_buckets_on_entry_) {
    bucket.register_combined_datastruct();
  }
}

void Region::on_region_entry() {
  // std::cout << "entry region: " << region_ << "\n";

  // make snapshot of current buckets access counts
  region_buckets_on_entry_ = g_buckets;
}

void Region::on_region_exit() {
  // std::cout << "exit region: " << region_ << "\n";
  size_t b = 0;
  for (Bucket &rb : region_buckets_) {
    rb.add_sub(g_buckets[b], region_buckets_on_entry_[b]);
    b++;
  }
}

void Region::print_csv(FILE *csv_out) {
  for (auto &b : region_buckets_) {
    b.print_csv(region_, csv_out);
  }
}
