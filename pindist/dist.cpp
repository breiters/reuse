/* Calculation of a Stack Reuse Distance Histogram
 *
 * (C) 2015, Josef Weidendorfer / LRR-TUM
 * GPLv2+ (see COPYING)
 */

#include <algorithm>
// #include <bits/charconv.h>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <list>
#include <unordered_map>
#include <vector>

#include "bucket.h"
#include "cachesim.h"
#include "datastructs.h"
#include "dist.h"
#include "memoryblock.h"
#include "pin.H"
#include "region.h"
// #include "khash.h"

using namespace std;

//-------------------------------------------------------------------------
// Stack structure with MemoryBlock as element
typedef list<MemoryBlock *>::iterator Marker;
list<MemoryBlock *> g_stack;
vector<Bucket> g_buckets;
int nextBucket; // when stack is growing, we may enter this bucket

// CacheSim g_cache{-1};

// make sure that memblocks are powers of two
static constexpr bool is_pow2(int a) { return !(a & (a - 1)); }
static_assert(is_pow2(MEMBLOCKLEN));

// find first bit set
static constexpr int ffs_constexpr(int x) {
  int n = 0;
  while ((x & 1) == 0) {
    x >>= 1;
    n++;
  }
  return n;
}
static_assert(ffs_constexpr(256) == 8); // sanity check
static constexpr int first_set = ffs_constexpr(MEMBLOCKLEN);

struct S {}; // dummy class to provide custom hash object

namespace std {
template <> struct hash<S> {
  std::size_t operator()(Addr const &s) const noexcept {
    // last n bits of Addr s are all zero
    // => avoid collisions having keys that are all multiples of MEMBLOCKLEN
    return std::hash<Addr>{}((Addr)(((uint64_t)s) >> first_set));
  }
};
} // namespace std

// unordered_map<Addr, std::pair<Marker, Marker>> addrMap;
// unordered_map<Addr, std::pair<Marker, Marker>, std::hash<S>> g_addrMap;
// unordered_map<Addr, std::pair<Marker, MarkerContainer>, std::hash<S>> g_addrMap;
unordered_map<Addr, MarkerContainer, std::hash<S>> g_addrMap;

MarkerContainer::MarkerContainer() {}
MarkerContainer::MarkerContainer(Marker g, Marker ds)
    : global_marker{g}, ds_marker{ds} {};


void moveMarkers(int topBucket) {
  for (int b = 1; b <= topBucket; b++) {
    --g_buckets.at(b).marker;
    (*(g_buckets.at(b).marker))->bucket++;
    if (RD_DEBUG)
      assert((*(g_buckets[b].marker))->bucket == b);
  }
}

int handleNewBlock(Addr a) {
  int ds_num = datstruct_num(a);

  // PIN stdlib port does not support shared_ptr...
  // ... so we use raw pointers here and don't care for best practices
  MemoryBlock *mb = new MemoryBlock{a, ds_num};
  Marker it;

  // new memory block
  g_stack.push_front(mb);
  if (ds_num != DATASTRUCT_UNKNOWN) {
    it = g_cachesims[ds_num].on_new_block(mb); // returns begin of stack
  }

  // g_addrMap[a] = std::make_pair(g_stack.begin(), it);
  MarkerContainer mc{g_stack.begin(), it};
  // g_addrMap[a] = std::make_pair(g_stack.begin(), mc);
  g_addrMap[a] = mc;
  // g_addrMap[a] = new MarkerContainer(g_stack.begin(), it);

  if (RD_VERBOSE > 1)
    fprintf(stderr, " NEW block %p, now %lu blocks seen\n", a, g_stack.size());

  // move all markers of active buckets
  moveMarkers(nextBucket - 1);

  bool next_bucket_active = g_addrMap.size() > g_buckets[nextBucket].min;
  bool last_bucket_reached = g_buckets[nextBucket].min == 0;

#if USE_OLD_CODE
  // does another bucket get active?
  if (addrMap.size() <= g_buckets[nextBucket].min)
    return;
  if (g_buckets[nextBucket].min == 0)
    return; // last bucket reached

  if (RD_VERBOSE > 0)
    fprintf(stderr, " ACTIVATE bucket %d (next bucket minimum depth %d)\n",
            nextBucket, g_buckets[nextBucket + 1].min);
#endif

  if (!last_bucket_reached && next_bucket_active) {
    --g_buckets[nextBucket].marker; // set marker to last entry
    (*(g_buckets[nextBucket].marker))->bucket++;
    // if (RD_DEBUG)
    assert((*(g_buckets[nextBucket].marker))->bucket == nextBucket);
    nextBucket++;
  }

  return ds_num;
}

void moveBlockToTop(Addr a, Marker blockIt, int bucket) {
  // move markers of active buckets within range 1 to <bucket>.
  // we need to do this before moving blockIt to top, as
  // there could be a marker on blockIt, which would go wrong
  moveMarkers(bucket);

  // move element pointed to by blockIt to beginning of list
  g_stack.splice(g_stack.begin(), g_stack, blockIt);

  (*blockIt)->incACount();
  (*blockIt)->bucket = 0;
  // blockIt->ds_bucket = 0;

  if (RD_VERBOSE > 1)
    fprintf(stderr, " MOVED %p from bucket %d to top (aCount %lu)\n", a, bucket,
            (*blockIt)->getACount());
}

// run stack simulation
// To use a specific block size, ensure that <a> is aligned
void RD_accessBlock(Addr a) {
  [[maybe_unused]] static Addr a_last;

  int bucket; // bucket of current access
  int ds_bucket = 0;
  int ds_num;

/*
  list<int> a_in_ds;
  list<int> a_not_ds;

  // for all datastructs where a is included : handle a and get iterator in stack
  for(int i : a_in_ds) {
    
  }
  */

  // was memory block accessed before? --> don't need to search in hash map!
  // memory block is on top of stack
  if (a == a_last) {
    // if (0) {
    // increase bucket 0 and ds_bucket 0
    auto blockIt = g_stack.begin();
    ds_num = (*blockIt)->ds_num;
    bucket = 0;
    ds_bucket = 0;

    // sanity check if address of memory block on top of stack is same as a:
    assert((*blockIt)->a == a);
  } else {

    auto it = g_addrMap.find(a);

    if (it == g_addrMap.end()) {
      // memory block not seen yet
      ds_num = handleNewBlock(a);
      bucket = g_buckets.size() - 1; // "infinite" distance, put in last bucket
      ds_bucket = g_buckets.size() - 1;
    } else {
      // memory block already seen
      auto pair = it->second;
      // auto blockIt = pair.first;
      auto blockIt = pair.global_marker;

      bucket = (*blockIt)->bucket;
      ds_num = (*blockIt)->ds_num;

      // if not already on top of stack, move to top
      if (blockIt != g_stack.begin()) {
        moveBlockToTop(a, blockIt, bucket);
      } else {
        (*blockIt)->incACount();
        if (RD_VERBOSE > 1)
          fprintf(stderr, " TOP %p accessed, bucket %d, aCount %lu\n", a,
                  bucket, (*blockIt)->getACount());
      }

      if (ds_num != DATASTRUCT_UNKNOWN) {
        // ds_bucket = (*(pair.second.ds_marker))->ds_bucket;
        ds_bucket = (*(pair.ds_marker))->ds_bucket;

        // sanity check: make sure they refer to same memory blocks
        // assert(*(pair.second) == *(pair.first));

        // g_cachesims[ds_num].on_block_seen(pair.second.ds_marker);
        g_cachesims[ds_num].on_block_seen(pair.ds_marker);
      }
    }
  }

  if (ds_num != DATASTRUCT_UNKNOWN) {
    g_datastructs[ds_num].access_count++;
  }

  a_last = a;

  // global access count on bucket
  g_buckets[bucket].aCount++;

  // access count to datastructure
  if (ds_num != DATASTRUCT_UNKNOWN) {
    assert(ds_bucket > -1);
    assert(ds_bucket <= (int)g_buckets.size() - 1);
    // assert(g_buckets[bucket].ds_markers_excl[ds_num]->ds_num == ds_num);

    g_buckets[bucket].ds_aCount[ds_num]++;
    g_buckets[ds_bucket].ds_aCount_excl[ds_num]++;
  }

  if (RD_DEBUG) {
    // run consistency check every 10 million invocations
    static int checkCount = 0;
    checkCount++;
    if (checkCount > 10000000) {
      RD_checkConsistency();
      checkCount = 0;
    }
  }
}


void RD_init(int min1) {
  g_stack.clear();
  g_addrMap.clear();
  g_datastructs.clear();
  // addrMap.rehash(4000000);

  g_buckets.clear();
  g_buckets.push_back(Bucket(0));    // bucket starting with distance 0
  g_buckets.push_back(Bucket(min1)); // first real bucket of interest
  g_buckets.push_back(Bucket(0));    // for "infinite" distance
  nextBucket = 1;
}

// add distance buckets, starting from smallest (>0)
// only specification of minimal distance required
void RD_addBucket(unsigned int min) {
  // fprintf(stderr, "Add bucket with dist %d (last dist: %d)\n",
  // min, g_buckets[g_buckets.size()-2].min);
  assert(g_buckets.size() > 2);
  assert(g_buckets[g_buckets.size() - 2].min < min);

  g_buckets.insert(g_buckets.end() - 1, Bucket(min));
}

// get statistics
void RD_stat(unsigned long &stack_size, unsigned long &accessCount) {
  if (RD_DEBUG)
    RD_checkConsistency();

  unsigned long aCount = 0;
  for (const Bucket &b : g_buckets)
    aCount += b.aCount;

  stack_size = g_addrMap.size();
  accessCount = aCount;
}

// get resulting histogram
// Repeatly call RD_get_hist, start with bucket 0.
// Returns next bucket or 0 if this was last
int RD_get_hist(unsigned int b, unsigned int &min, unsigned long &accessCount) {
  // if (RD_DEBUG) RD_checkConsistency();

  assert((b >= 0) && (b < g_buckets.size()));
  min = g_buckets[b].min;
  accessCount = g_buckets[b].aCount;
  if (b == g_buckets.size() - 1)
    return 0;
  return b + 1;
}

// histogram for datastructures
int RD_get_hist_ds(int ds_num, unsigned int b, unsigned int &min,
                   unsigned long &accessCount) {
  // if (RD_DEBUG) RD_checkConsistency();

  assert((b >= 0) && (b < g_buckets.size()));
  min = g_buckets[b].min;
  accessCount = g_buckets[b].ds_aCount[ds_num];
  if (b == g_buckets.size() - 1)
    return 0;
  return b + 1;
}

int RD_get_hist_ds_excl(int ds_num, unsigned int b, unsigned int &min,
                        unsigned long &accessCount) {
  // if (RD_DEBUG) RD_checkConsistency();

  assert((b >= 0) && (b < g_buckets.size()));
  min = g_buckets[b].min;
  accessCount = g_buckets[b].ds_aCount_excl[ds_num];
  if (b == g_buckets.size() - 1)
    return 0;
  return b + 1;
}

// print nice ASCII histogram

// helpers
char *formatLong(unsigned long aCount, unsigned long maxCount) {
  static char out[20];

  if (maxCount > 999999999)
    sprintf(out, "%7.3f G", (double)aCount / 1000000000.0);
  else if (maxCount > 999999)
    sprintf(out, "%7.3f M", (double)aCount / 1000000.0);
  else if (aCount > 999)
    sprintf(out, "%3d %03d  ", (int)aCount / 1000, (int)(aCount % 1000));
  else
    sprintf(out, "    %3d  ", (int)aCount);

  return out;
}

char *formatBar(unsigned long aCount, unsigned long maxCount, int len) {
  int i, size;
  static char bar[110];

  if (len > 100)
    len = 100;
  for (i = 0; i < len; i++)
    bar[i] = '#';
  bar[i] = 0;

  size = (int)((double)aCount / maxCount * len);
  if (size > len)
    size = len;
  return bar + len - size;
}

// print histogram + statistics to <out>
//  <pStr> prefix for every line
void RD_printHistogram(FILE *out, const char *pStr, int blockSize) {
  int b, bNext;
  int maxBucket, maxNonEmptyBucket;
  unsigned int min;
  unsigned long aCount, maxCount;
  unsigned long stack_size;
  char bStr[20];

  fprintf(out, "%sHistogram:\n", pStr);

  maxNonEmptyBucket = 1; // set to bucket with largest non-empty distance
  maxCount = 0;
  b = 1;
  do {
    bNext = RD_get_hist(b, min, aCount);
    if (aCount > maxCount)
      maxCount = aCount;
    if ((min > 0) && (aCount > 0))
      maxNonEmptyBucket = b;
    if (b > 0)
      maxBucket = b;
    b = bNext;
  } while (b != 0);

  if (maxNonEmptyBucket < maxBucket)
    maxNonEmptyBucket++;

  // TODO: duplicated code !

  //  fprintf(out, "%sMaxBucket %d NonEmpty %d\n",
  //          pStr, maxBucket, maxNonEmptyBucket);

  bNext = RD_get_hist(0, min, aCount);
  fprintf(out, "%s[%8.3f MB ..] %s ==>\n", pStr,
          (double)(min * blockSize) / 1000000.0, formatLong(aCount, aCount));
  b = bNext;
  do {
    bNext = RD_get_hist(b, min, aCount);
    if ((b > maxNonEmptyBucket) && (aCount == 0)) {
      b = bNext;
      continue;
    }

    if (min > 0)
      sprintf(bStr, "%8.3f MB ..", (double)(min * blockSize) / 1024.0 / 1024.0);
    else
      sprintf(bStr, "   inf/cold   ");

    fprintf(out, "%s[%s] %s %s\n", pStr, bStr, formatLong(aCount, maxCount),
            formatBar(aCount, maxCount, 60));
    b = bNext;
  } while (b != 0);

#if PRINT_DATASTRUCT_TO_TERMINAL
  // Datastructure prints

  int ds_num = 0;
  for (auto ds : g_datastructs) {
    fprintf(out, "\n");
    ds.print();
    fprintf(out, "  number of accesses to datastruct:     %lu\n",
            ds.access_count);
    fprintf(out, "\n");

    bNext = RD_get_hist_ds(ds_num, 0, min, aCount);
    fprintf(out, "%s[%8.3f MB ..] %s ==>\n", pStr,
            (double)(min * blockSize) / 1000000.0, formatLong(aCount, aCount));
    b = bNext;
    do {
      bNext = RD_get_hist_ds(ds_num, b, min, aCount);
      if ((b > maxNonEmptyBucket) && (aCount == 0)) {
        b = bNext;
        continue;
      }

      if (min > 0)
        sprintf(bStr, "%8.3f MB ..",
                (double)(min * blockSize) / 1024.0 / 1024.0);
      else
        sprintf(bStr, "   inf/cold   ");

      fprintf(out, "%s[%s] %s %s\n", pStr, bStr, formatLong(aCount, maxCount),
              formatBar(aCount, maxCount, 60));
      b = bNext;
    } while (b != 0);

    fprintf(out, "exclusive:\n");

    bNext = RD_get_hist_ds_excl(ds_num, 0, min, aCount);
    fprintf(out, "%s[%8.3f MB ..] %s ==>\n", pStr,
            (double)(min * blockSize) / 1000000.0, formatLong(aCount, aCount));
    b = bNext;
    do {
      bNext = RD_get_hist_ds_excl(ds_num, b, min, aCount);
      if ((b > maxNonEmptyBucket) && (aCount == 0)) {
        b = bNext;
        continue;
      }

      if (min > 0)
        sprintf(bStr, "%8.3f MB ..",
                (double)(min * blockSize) / 1024.0 / 1024.0);
      else
        sprintf(bStr, "   inf/cold   ");

      fprintf(out, "%s[%s] %s %s\n", pStr, bStr, formatLong(aCount, maxCount),
              formatBar(aCount, maxCount, 60));
      b = bNext;
    } while (b != 0);

    ds_num++;
  }
#endif

  fprintf(out, "\n");

  RD_stat(stack_size, aCount);

  fprintf(out,
          "%sStatistics:\n"
          "%s  memory blocks accessed: %lu (%3.3f MB)\n"
          "%s  number of accesses:     %lu\n",
          pStr, pStr, stack_size, ((double)stack_size * blockSize) / 1000000.0,
          pStr, aCount);

  for (auto &bucket : g_buckets) {
    bucket.print_csv("main");
  }

  // cleanup : should be done elsewhere
  for (const auto &region : g_regions) {
    region.second->print_csv();
    delete region.second;
  }
}



#define FUTURE_IMPROVED_HASH 0
#if FUTURE_IMPROVED_HASH

//-------------------------------------------------------------------------
// Specialization of unordered_map to use masking for bucket calculation
struct _Mod_myrange_hashing {
  typedef std::size_t first_argument_type;
  typedef std::size_t second_argument_type;
  typedef std::size_t result_type;

  static std::size_t mask;

  _Mod_myrange_hashing() {}
  _Mod_myrange_hashing(const __detail::_Mod_range_hashing &temp) {}

  result_type operator()(first_argument_type __num,
                         second_argument_type __den) const noexcept {
    if (mask < __den) {
      std::size_t n = __den - 1;
      n |= n >> 1;
      n |= n >> 2;
      n |= n >> 4;
      n |= n >> 8;
      n |= n >> 16;
      n |= n >> 32;
      mask = n;
    }
    __num >>= 6;
    std::size_t probe = (__num & mask);
    std::size_t b = (probe < __den) ? probe : (__num % __den);

    return b;
  }
};

std::size_t _Mod_myrange_hashing::mask = 1;

namespace std {
template <typename _Alloc, typename _ExtractKey, typename _Equal, typename _H1,
          typename _Hash, typename _RehashPolicy, typename _Traits>

class _Hashtable<Addr, std::pair<const Addr, list<MemoryBlock>::iterator>,
                 _Alloc, _ExtractKey, _Equal, _H1, __detail::_Mod_range_hashing,
                 _Hash, _RehashPolicy, _Traits>
    : public _Hashtable<Addr,
                        std::pair<const Addr, list<MemoryBlock>::iterator>,
                        _Alloc, _ExtractKey, _Equal, _H1, _Mod_myrange_hashing,
                        _Hash, _RehashPolicy, _Traits> {
public:
  using myBase =
      _Hashtable<Addr, std::pair<const Addr, list<MemoryBlock>::iterator>,
                 _Alloc, _ExtractKey, _Equal, _H1, _Mod_myrange_hashing, _Hash,
                 _RehashPolicy, _Traits>;

  using myBase::_Hashtable;
  using mySizeType = typename myBase::size_type;
};
} // namespace std
//-------------------------------------------------------------------------
#endif


// do an internal consistency check
void RD_checkConsistency() {
  unsigned int aCount1, aCount2;
  unsigned int d = 0;
  int b = 0;
  aCount1 = 0;
  aCount2 = g_buckets[0].aCount;

  if (RD_VERBOSE > 0) {
    fprintf(stderr, "\nChecking... (stack size: %lu)\n", g_stack.size());
    fprintf(stderr, "   START Bucket %d (min depth %u): aCount %lu\n", b,
            g_buckets[b].min, g_buckets[b].aCount);
  }

  Marker stackIt;
  for (stackIt = g_stack.begin(); stackIt != g_stack.end(); ++stackIt, ++d) {
    if (d == g_buckets[b + 1].min) {
      b++;
      aCount2 += g_buckets[b].aCount;
      if (RD_VERBOSE > 0)
        fprintf(stderr, "   START Bucket %d (min depth %u): aCount %lu\n", b,
                g_buckets[b].min, g_buckets[b].aCount);
      assert(stackIt == g_buckets[b].marker);
    }

    if (RD_VERBOSE > 1) {
      static char b[100];
      (*stackIt)->print(b);
      fprintf(stderr, "   %5d: %s\n", d, b);
    }
    aCount1 += (*stackIt)->getACount();
    assert((*stackIt)->bucket == b);
  }
  assert(nextBucket = b + 1);
  b = g_buckets.size() - 1;
  aCount2 += g_buckets[b].aCount;
  if (RD_VERBOSE > 0) {
    fprintf(stderr, "   Last Bucket %d: aCount %lu\n", b, g_buckets[b].aCount);
    fprintf(stderr, "   Total aCount: %u\n", aCount1);
  }
#if MIN_BLOCKSTRUCT
  assert(g_buckets[b].aCount == g_stack.size());
#else
  assert(g_buckets[b].aCount == stack.size());
  assert(aCount1 == aCount2);
#endif
}
