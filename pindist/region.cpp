#include "bucket.h"
#include "datastructs.h"
#include "region.h"
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
    
    for (auto b : buckets) {
        b.aCount = 0;
        for(auto& a : b.ds_aCount) {
            a = 0;
        }
        for(auto& a : b.ds_aCount_excl) {
            a = 0;
        }
        region_buckets_.push_back(b);
        region_buckets_on_entry_.push_back(b);
    }
}

Region::~Region()
{
    // std::cout << "free region: " << region_ << "\n";
    free(region_);
    region_ = nullptr;
}

void Region::register_datastruct()
{
    for(auto &bucket : region_buckets_) {
        bucket.register_datastruct();
    }
    for(auto &bucket : region_buckets_on_entry_) {
        bucket.register_datastruct();
    }
}

void Region::on_region_entry()
{
    // make snapshot of current buckets access counts
    region_buckets_on_entry_ = buckets;

    // std::cout << "entry region: " << region_ << "\n";
}

void Region::on_region_exit()
{
    // std::cout << "exit region: " << region_ << "\n";

    // accumulate bucket difference (exit - entry) for access counts
    int b = 0;
    for(Bucket &rb : region_buckets_) {
        rb.aCount += buckets[b].aCount - region_buckets_on_entry_[b].aCount;
        for(decltype(rb.ds_aCount.size()) ds = 0; ds < rb.ds_aCount.size(); ds++) {
            rb.ds_aCount[ds] = buckets[b].ds_aCount[ds] - region_buckets_on_entry_[b].ds_aCount[ds];
            rb.ds_aCount_excl[ds] = buckets[b].ds_aCount_excl[ds] - region_buckets_on_entry_[b].ds_aCount_excl[ds];
        }
        b++;
    }
}

void Region::print_csv()
{
    for (auto &b : region_buckets_) {
        b.print_csv(region_);
    }
}
