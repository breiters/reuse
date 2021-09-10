#include <cassert>

#include "bucket.h"
#include "cachesim.h"

std::vector<CacheSim> g_cachesims;
std::vector<CacheSim> g_cachesims_negated;
std::vector<CacheSim> g_cachesims_combined;
std::vector<CacheSim> g_cachesims_combined_negated;

CacheSim::CacheSim(int datastruct_num)
    : next_bucket_{1}, datastruct_num_{datastruct_num} {}
// buckets_{g_buckets}

void CacheSim::on_next_bucket_gets_active() {
  // set new buckets marker to stack end first then set marker to last stack
  // element
  g_buckets[next_bucket_].ds_markers[datastruct_num_] = stack_.end();
  --(g_buckets[next_bucket_].ds_markers[datastruct_num_]);

  assert(g_buckets[next_bucket_].ds_markers[datastruct_num_] != stack_.begin());
  assert(g_buckets[next_bucket_].ds_markers[datastruct_num_] != stack_.end());

  // last stack element is now in the next higher bucket
  (*(g_buckets[next_bucket_].ds_markers[datastruct_num_]))->ds_bucket++;

  assert((*(g_buckets[next_bucket_].ds_markers[datastruct_num_]))->ds_bucket ==
         next_bucket_);

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

  move_markers(next_bucket_ - 1);

  // does another bucket get active?
  bool last_bucket_reached = g_buckets[next_bucket_].min == 0;
  bool next_bucket_active = stack_.size() > g_buckets[next_bucket_].min;

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
    assert(g_buckets[b].ds_markers[datastruct_num_] != stack_.begin());
    assert(g_buckets[b].ds_markers[datastruct_num_] != stack_.end());

    // decrement marker so it stays always on same distance to stack begin
    --(g_buckets[b].ds_markers[datastruct_num_]);

    assert((int)g_buckets[b].ds_markers.size() > datastruct_num_);
    assert(!stack_.empty());
    assert(g_buckets[b].ds_markers[datastruct_num_] != stack_.begin());
    assert(g_buckets[b].ds_markers[datastruct_num_] != stack_.end());

    // increment bucket of memory block where current marker points to
    (*(g_buckets[b].ds_markers[datastruct_num_]))->ds_bucket++;

    // assert((g_buckets[b].marker)->ds_bucket == b); // TODO ??
  }
}
