#include "cachesim.h"
#include "bucket.h"
#include "datastructs.h"
#include "region.h"
#include <algorithm>
#include <cassert>

std::vector<CacheSim> g_cachesims;
std::vector<CacheSim> g_cachesims_combined;

CacheSim::CacheSim(int ds_num)
    : next_bucket_{1}, ds_num_{ds_num}, buckets_{g_buckets} {
  for (auto &b : buckets_) {
    b.aCount = 0;
    b.aCount_excl = 0;
    b.marker = stack_.end(); // TODO: can be removed
    // printf("min: %u\n", b.min);
  }
}

void CacheSim::on_next_bucket_gets_active() {
  // set new buckets marker to end of stack first then set marker to last stack
  // element
  buckets_[next_bucket_].marker = stack_.end();
  --(buckets_[next_bucket_].marker);

  assert(buckets_[next_bucket_].marker != stack_.begin());
  assert(buckets_[next_bucket_].marker != stack_.end());

  // last stack element is now in the next higher bucket
  (buckets_[next_bucket_].marker)->bucket++;

  assert((buckets_[next_bucket_].marker)->bucket == next_bucket_);

  next_bucket_++;
}

/**
 * @brief Adds new memory block to top of stack. Moves active bucket markers.
 * Adds next bucket if necessary.
 *
 * @param mb The to memory block.
 * @return list<MemoryBlock>::iterator The stack begin iterator.
 */
const Marker CacheSim::on_new_block(MemoryBlock mb) {
  stack_.push_front(mb);

  // move markers upwards after inserting new block on stack
  move_markers(next_bucket_ - 1);

  // does another bucket get active?
  bool last_bucket_reached = buckets_[next_bucket_].min == 0;
  bool next_bucket_active = stack_.size() > buckets_[next_bucket_].min;

  if (!last_bucket_reached && next_bucket_active) {
    on_next_bucket_gets_active();
  }
  return stack_.begin();
}

/**
 * @brief
 *
 * @param blockIt
 */
void CacheSim::on_block_seen(const Marker &blockIt) {
  // if already on top do nothing
  if (blockIt == stack_.begin()) {
    return;
  }

  // move all markers below current memory blocks bucket
  int bucket = blockIt->bucket;
  move_markers(bucket);

  // put current memory block on top and set its buckets to zero
  stack_.splice(stack_.begin(), stack_, blockIt);

  blockIt->bucket = 0;
}

void CacheSim::move_markers(int topBucket) {
  for (int b = 1; b <= topBucket; b++) {
    assert(buckets_[next_bucket_].marker != stack_.begin());

    // decrement marker so it stays always on same distance to stack begin
    --buckets_[b].marker;

#if 0
// sanity check, but takes too long
    unsigned int distance = 0;
    for(auto it = stack_.begin(); it != buckets_[b].marker; it++) {
      distance++;
    }
    // printf("distance: %d\n", distance);
    assert(distance == buckets_[b].min);
#endif

    // increment bucket of memory block where current marker points to
    (buckets_[b].marker)->bucket++;
  }
}

void CacheSim::add_datastruct(int ds_num) { ds_nums_.push_back(ds_num); }

bool CacheSim::contains(int ds_num) const {
  return std::find(ds_nums_.begin(), ds_nums_.end(), ds_num) != ds_nums_.end();
}

#define CSV_FORMAT "%s,%d,%p,%zu,%d,%lu,%s,%u,%lu,%lu,%lu\n"
#define CSV_FORMAT2 "%s,%s,%p,%zu,%d,%lu,%s,%u,%lu,%lu,%lu\n"

void CacheSim::print_csv(FILE *csv_out, const char *region) {
  print_csv(csv_out, "main", buckets_);
}

void CacheSim::print_csv(FILE *csv_out, const char *region,
                         std::vector<Bucket> &buckets) {
  // original code has min = 0 to define infinite distance
  // change to max limit to distinguish buckets zero from inf
  buckets.back().min = std::numeric_limits<decltype(buckets.back().min)>::max();
  // print_csv(csv_out, region, buckets_.data());
  print_csv(csv_out, region, &buckets_[0], buckets_.size());
}

void CacheSim::print_csv(FILE *csv_out, const char *region, Bucket *buckets,
                         size_t num_buckets) {
  // single datastruct
  if (ds_nums_.size() == 0) {
    auto &ds = g_datastructs[ds_num_];
    for (size_t b = 0; b < num_buckets; b++) {
      fprintf(csv_out, CSV_FORMAT, region, ds_num_, ds.address, ds.nbytes,
              ds.line, ds.access_count, ds.file_name.c_str(), buckets[b].min,
              buckets[b].aCount, buckets[b].aCount, buckets[b].aCount_excl);
    }
  }
  // combined datastruct
  else {
    size_t MAX_LEN = 512;
    char str[MAX_LEN];
    str[0] = '[';
    str[1] = ' ';
    size_t offset = 2;
    size_t nbytes = 0;
    for (auto num : ds_nums_) {
      offset += snprintf(str + offset, MAX_LEN - offset, "%d | ", num);
      nbytes += g_datastructs[num].nbytes;
    }
    str[offset - 2] = ']';
    str[offset - 1] = '\0';
    for (size_t b = 0; b < num_buckets; b++) {
      fprintf(csv_out, CSV_FORMAT2, region, str, (void *)0x0, nbytes, 0, 0UL,
              " ", buckets[b].min, buckets[b].aCount, buckets[b].aCount,
              buckets[b].aCount_excl);
    }
  }
}
