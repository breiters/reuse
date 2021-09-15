#include "cachesim.h"
#include "bucket.h"
#include "datastructs.h"
#include "region.h"
#include <algorithm>
#include <cassert>

std::vector<CacheSim> CacheSim::cachesims;

CacheSim::CacheSim()
    : next_bucket_{1}, cs_num_{static_cast<int>(cachesims.size())}, buckets_{std::vector<Bucket>{Bucket::mins.size()}} {
      printf("add cs: %d\n", cs_num_);
}

void CacheSim::on_next_bucket_gets_active() {
  // set new buckets marker to end of stack first then set marker to last stack element
  buckets_[next_bucket_].marker = stack_.end();

  --(buckets_[next_bucket_].marker);

// #if RD_DEBUG
#if 1
  StackIterator it = stack_.begin();
  for (unsigned i = 0; i < Bucket::mins[next_bucket_]; i++)
    it++;
  assert(it == buckets_[next_bucket_].marker);
#endif

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
const StackIterator CacheSim::on_block_new(MemoryBlock mb) {
  stack_.push_front(mb);

  // move markers upwards after inserting new block on stack
  move_markers(next_bucket_ - 1);

  // does another bucket get active?
  if (!Bucket::mins[next_bucket_] == BUCKET_INF_DIST && stack_.size() > Bucket::mins[next_bucket_]) {
    on_next_bucket_gets_active();
  }
  return stack_.begin();
}

/**
 * @brief
 *
 * @param blockIt
 */
int CacheSim::on_block_seen(const StackIterator &blockIt) {
  // if already on top of stack: do nothing (bucket is zero anyway)
  if (blockIt == stack_.begin()) {
    return 0;
  }
  puts("1");
  // move all markers below current memory blocks bucket
  int bucket = blockIt->bucket;
  move_markers(bucket);
puts("2");
  // put current memory block on top of stack
  stack_.splice(stack_.begin(), stack_, blockIt);
puts("3");
  // bucket of blockIt is zero now because it is on top of stack
  blockIt->bucket = 0;

  return bucket;
}

void CacheSim::move_markers(int topBucket) {
  for (int b = 1; b <= topBucket; b++) {
    assert(buckets_[next_bucket_].marker != stack_.begin());

    // decrement marker so it stays always on same distance to stack begin
    --buckets_[b].marker;

// #if RD_DEBUG > 1
#if 1
    // sanity check for bucket distance to stack begin (expensive)
    unsigned distance = 0;
    for (auto it = stack_.begin(); it != buckets_[b].marker; it++) {
      distance++;
    }
    printf("distance: %d\n", distance);
    printf("bucketmins: %d\n", Bucket::mins[b]);
    printf("b: %d\n", b);
    assert(distance == Bucket::mins[b]);
#endif

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
 * CSV output functions
 *********************************************
 */

#define CSV_FORMAT "%s,%d,%p,%zu,%d,%lu,%s,%u,%lu,%lu\n"
#define CSV_FORMAT2 "%s,%s,%p,%zu,%d,%lu,%s,%u,%lu,%lu\n"

void CacheSim::print_csv(FILE *csv_out, const char *region) const { print_csv(csv_out, region, buckets_); }

void CacheSim::print_csv(FILE *csv_out, const char *region, const std::vector<Bucket> &buckets) const {
  // is single datastruct or global address space accesses
  if (ds_nums_.size() == 0)
    if (cs_num_ == RD_NO_DATASTRUCT) {
      for (size_t b = 0; b < buckets.size(); b++) {
        fprintf(csv_out, CSV_FORMAT, region, cs_num_, (void *)0x0, 0UL, 0, 0UL, "main file", Bucket::mins[b],
                buckets[b].aCount, buckets[b].aCount_excl);
      }
    } else {
      auto &ds = Datastruct::datastructs[cs_num_];
      for (size_t b = 0; b < buckets.size(); b++) {
        fprintf(csv_out, CSV_FORMAT, region, cs_num_, ds.address, ds.nbytes, ds.line, ds.access_count,
                ds.file_name.c_str(), Bucket::mins[b], buckets[b].aCount, buckets[b].aCount_excl);
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
              buckets[b].aCount_excl);
    }
  }
}
