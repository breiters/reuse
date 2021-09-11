#include "cachesim.h"
#include "bucket.h"
#include "region.h"
#include <algorithm>
#include <cassert>

std::vector<CacheSim> g_cachesims;
std::vector<CacheSim> g_cachesims_combined;

// buckets_{buckets_}

CacheSim::CacheSim(int ds_num)
    : next_bucket_{1}, ds_num_{ds_num}, buckets_{g_buckets} {
  for (auto &b : buckets_) {
    b.aCount = 0;
    b.aCount_excl = 0;
    // b.marker = stack_.end();
    printf("min: %u\n", b.min);
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
  (*(buckets_[next_bucket_].marker))->ds_bucket++;

  assert((*(buckets_[next_bucket_].marker))->ds_bucket == next_bucket_);

  next_bucket_++;
}

/**
 * @brief Adds new memory block to top of stack. Moves active bucket markers.
 * Adds next bucket if necessary.
 *
 * @param mb The pointer to memory block.
 * @return list<MemoryBlock *>::iterator The stack begin iterator.
 */
const Marker CacheSim::on_new_block(MemoryBlock *mb) {
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
  int bucket = (*blockIt)->ds_bucket;
  move_markers(bucket);

  // put current memory block on top and set its buckets to zero
  stack_.splice(stack_.begin(), stack_, blockIt);

  assert((*blockIt)->bucket == 0);
  (*blockIt)->ds_bucket = 0;
}

void CacheSim::move_markers(int topBucket) {
  for (int b = 1; b <= topBucket; b++) {
    assert(buckets_[next_bucket_].marker != stack_.begin());
    // decrement marker so it stays always on same distance to stack begin
    --(buckets_[b].marker);
    // increment bucket of memory block where current marker points to
    (*buckets_[b].marker)->ds_bucket++;
  }
}

void CacheSim::add_datastruct(int ds_num) { ds_nums_.push_back(ds_num); }

bool CacheSim::contains(int ds_num) const {
  return std::find(ds_nums_.begin(), ds_nums_.end(), ds_num) != ds_nums_.end();
}

void CacheSim::print_csv(const char *region, FILE *csv_out) {
  for(auto &b : buckets_) {
    b.print_csv(region, csv_out);
  }
}
