#include "cachesim.h"
#include "bucket.h"
#include "datastructs.h"
#include "dist.h"
#include "pin.H"
#include "region.h"
#include <algorithm>
#include <cassert>

/**
 * can NOT use std::vector<CacheSim> here because PIN stl somehow invalidates the list iterators of existing elements
 * when pushing new CacheSim
 */
std::vector<CacheSim *> g_cachesims; // LRU stack objects

// size_t CacheSim::num_cs;

CacheSim::CacheSim()
    : next_bucket_{1}, cs_num_{static_cast<int>(g_cachesims.size())}, buckets_{
                                                                          std::vector<Bucket>{Bucket::mins.size()}} {
  PIN_MutexInit(&mtx_);
  eprintf("add new cache simulator - num : %d\n", cs_num_);
}

void CacheSim::on_next_bucket_gets_active() {
  // eprintf("\n%s\n", __func__);

  // set new buckets marker to end of stack first then set marker to last stack element
  buckets_[next_bucket_].marker = stack_.end();

  --(buckets_[next_bucket_].marker);

  eprintf("marker now on:");
  buckets_[next_bucket_].marker->print();

#if RD_DEBUG
  StackIterator it = stack_.begin();
  for (unsigned i = 0; i < Bucket::mins[next_bucket_]; i++)
    it++;
  assert(it == buckets_[next_bucket_].marker);
#endif

  // last stack element is now in the next higher bucket
  (buckets_[next_bucket_].marker)->bucket++;

  assert((buckets_[next_bucket_].marker)->bucket == next_bucket_);

  next_bucket_++;

  print_stack();
}

/**
 * @brief Adds new memory block to top of stack. Moves active bucket markers.
 * Adds next bucket if necessary.
 *
 * @param mb The to memory block.
 * @return The stack begin iterator
 */
StackIterator CacheSim::on_block_new(const MemoryBlock &mb) {
  // eprintf("\n%s\n", __func__);
  stack_.push_front(mb);

  // print_stack();

  // move markers upwards after inserting new block on stack
  move_markers(next_bucket_ - 1);

  // does another bucket get active?
  if (Bucket::mins[next_bucket_] != BUCKET_INF_DIST && (stack_.size() > Bucket::mins[next_bucket_])) {
    on_next_bucket_gets_active();
  }

  check_consistency();

  return stack_.begin();
}

/**
 * @brief
 *
 * @param blockIt
 */
int CacheSim::on_block_seen(StackIterator &blockIt) {
  // eprintf("\n%s\n", __func__);

  // if already on top of stack: do nothing (bucket is zero anyway)
  if (blockIt == stack_.begin()) {
    return 0;
  }

  eprintf("block was not on top... moving block to top of stack\n");

  blockIt->print();
  // move all markers below current memory blocks bucket
  int bucket = blockIt->bucket;

  move_markers(bucket);

  // put current memory block on top of stack
  stack_.splice(stack_.begin(), stack_, blockIt);

  eprintf("\nstack after splice:");
  print_stack();

  // bucket of blockIt is zero now because it is on top of stack
  blockIt->bucket = 0;

  check_consistency();

  return bucket;
}

/**
 * Sanity check:
 * - every active marker must be found in stack in the right order
 * - distance of bucket marker to stack begin must be equal to the min distance for the bucket
 */
void CacheSim::check_consistency() {
#if RD_DEBUG > 1
  constexpr size_t DO_CHECK = 100000;
  static size_t iter = 0;
  iter++;
  if (iter < DO_CHECK) {
    return;
  }
  auto it_start = stack_.begin();
  for (int b = 1; b < next_bucket_; b++) {
    unsigned distance = 0;
    for (auto it = it_start; it != buckets_[b].marker; it++) {
      assert(it != stack_.end());
      distance++;
    }
    assert(distance == Bucket::mins[b]);
  }
#endif
}

void CacheSim::move_markers(int topBucket) {
  assert(topBucket < next_bucket_ && topBucket >= 0);
  for (int b = 1; b <= topBucket; b++) {
    assert(buckets_[next_bucket_].marker != stack_.begin());

    // decrement marker so it stays always on same distance to stack begin
    --(buckets_[b].marker);

    // increment bucket of memory block where current marker points to
    (buckets_[b].marker)->bucket++;
  }
}

void CacheSim::add_datastruct(int ds_num) { ds_nums_.push_back(ds_num); }

bool CacheSim::contains(int ds_num) const {
  return std::find(ds_nums_.begin(), ds_nums_.end(), ds_num) != ds_nums_.end();
}

/*
 *********************************************
 *     CSV output functions
 *********************************************
 */

#define CSV_TRAIL "%p,%zu,%d,%lu,%s,%u,%lu,%lu,%u\n"
#define CSV_FORMAT "%s,%d," CSV_TRAIL
#define CSV_FORMAT2 "%s,%s," CSV_TRAIL

extern unsigned g_max_threads;

void CacheSim::print_csv(FILE *csv_out, const char *region) const { print_csv(csv_out, region, buckets_); }

void CacheSim::print_csv(FILE *csv_out, const char *region, const std::vector<Bucket> &buckets) const {
  // global address space accesses
  if (ds_nums_.size() == 0) {
    Datastruct ds{};
    for (size_t b = 0; b < buckets.size(); b++) {
      fprintf(csv_out, CSV_FORMAT, region, -1, (void *)0x0, 0UL, 0, 0UL, "main file", Bucket::mins[b],
              buckets[b].aCount, buckets[b].aCount_excl, g_max_threads);
    }
  }
  // single datastruct
  else if (ds_nums_.size() == 1) {
    auto &ds = Datastruct::datastructs[ds_nums_[0]];
    for (size_t b = 0; b < buckets.size(); b++) {
      fprintf(csv_out, CSV_FORMAT, region, ds_nums_[0], ds.address, ds.nbytes, ds.line, ds.access_count,
              ds.file_name.c_str(), Bucket::mins[b], buckets[b].aCount, buckets[b].aCount_excl, g_max_threads);
    }
  }
  // is combined datastruct access
  else {
    size_t MAX_LEN = 512;
    char buf[MAX_LEN];
    buf[0] = '[';
    buf[1] = ' ';
    size_t offset = 2;
    size_t nbytes = 0;
    for (auto num : ds_nums_) {
      offset += snprintf(buf + offset, MAX_LEN - offset, "%d | ", num);
      nbytes += Datastruct::datastructs[num].nbytes;
    }
    buf[offset - 2] = ']';
    buf[offset - 1] = '\0';
    for (size_t b = 0; b < buckets.size(); b++) {
      fprintf(csv_out, CSV_FORMAT2, region, buf, (void *)0x0, nbytes, 0, 0UL, " ", Bucket::mins[b], buckets[b].aCount,
              buckets[b].aCount_excl, g_max_threads);
    }
  }
}
