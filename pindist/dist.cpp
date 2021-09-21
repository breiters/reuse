/* Calculation of a Stack Reuse Distance Histogram
 *
 * (C) 2015, Josef Weidendorfer / LRR-TUM
 * GPLv2+ (see COPYING)
 */

#include <algorithm>
// #include <bits/charconv.h>
#include <cassert>
#include <cstdio>
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

using std::list;
using std::string;
using std::unordered_map;
using std::vector;

extern unsigned g_max_threads;
extern string g_application_name;

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
  size_t operator()(Addr const &s) const noexcept {
    // last n bits of Addr s are all zero
    // => avoid collisions having keys that are all multiples of MEMBLOCKLEN
    return hash<Addr>{}((Addr)(((uintptr_t)s) >> first_set));
  }
};
} // namespace std

// each memory block can be contained in multiple stacks
// so we need to store the iterators to the memory block in each stack
//
// use hash map with custom hash function:
using AddrMap = std::unordered_map<Addr, vector<StackIterator>, std::hash<S>>;
AddrMap g_addrMap;

[[maybe_unused]] static void on_accessed_before(Addr addr) {
  // for all caches where ds_num is included:
  // increment bucket 0 access count
  // and bucket 0 exclusive access count

  int ds_num = Datastruct::datastruct_num(addr);

#if RD_DATASTRUCTS
  if (ds_num != RD_NO_DATASTRUCT) {
    for (int idx : Datastruct::indices_of[ds_num]) {
      assert(idx != 0);
      assert(g_cachesims[idx]->contains(ds_num));
      g_cachesims[idx]->incr_access(0);
      g_cachesims[idx]->incr_access_excl(0);
    }
  }
#endif /* RD_DATASTRUCTS */
}

// new memory block
static void on_block_new(Addr addr) {
  eprintf("%s\n", __func__);
  // memory block not seen yet
  //
  // for all caches where ds_num is included:
  // increment bucket inf access count (done in CacheSim::on_block_new)
  // and bucket inf exclusive access count (done in CacheSim::on_block_new)
  //
  // add block to stack and move markers and increment bucket of memory block
  // that move to next bucket
  //
  int ds_num = Datastruct::datastruct_num(addr);
  MemoryBlock mb = MemoryBlock{addr, ds_num};

  vector<StackIterator> iterators;

  g_cachesims[0]->on_block_new(mb);

  StackIterator it = g_cachesims[0]->stack_begin();
  iterators.push_back(it);
  g_cachesims[0]->incr_access_inf();

  eprintf("new block: ");
  mb.print();

#if RD_DATASTRUCTS
  if (ds_num != RD_NO_DATASTRUCT) {
    // account for access to combined dastructs
    for (int idx : Datastruct::indices_of[ds_num]) {
      assert(idx != 0);
      assert(g_cachesims[idx]->contains(ds_num));
      iterators.push_back(g_cachesims[idx]->on_block_new(mb));
      g_cachesims[idx]->incr_access_inf();
      g_cachesims[idx]->incr_access_excl_inf();
    }
  }
#endif /* RD_DATASTRUCTS */

  g_addrMap[addr] = iterators;

#if USE_OLD_CODE
  if (RD_VERBOSE > 1)
    fprintf(stderr, " NEW block %p, now %lu blocks seen\n", addr, g_stack.size());

  if (RD_VERBOSE > 0)
    fprintf(stderr, " ACTIVATE bucket %d (next bucket minimum depth %d)\n", g_nextBucket,
            buckets[g_nextBucket + 1].min);
#endif
}

static void on_block_seen(AddrMap::iterator &it) {
  eprintf("%s\n", __func__);
  // memory block already seen - get iterators in stacks

  auto iterators = it->second;

  eprintf("restored iterator: ");
  iterators[0]->print();

  int global_bucket = g_cachesims[0]->on_block_seen(iterators[0]);
  g_cachesims[0]->incr_access(global_bucket);
  int ds_num = iterators[0]->ds_num;

#if RD_DATASTRUCTS
  if (ds_num != RD_NO_DATASTRUCT) {
    int it_idx = 1; // iterator 0 is for global stack so start with 1
    for (int idx : Datastruct::indices_of[ds_num]) {
      assert(idx != 0);
      assert(g_cachesims[idx]->contains(ds_num));
      assert(iterators.size() == Datastruct::indices_of[ds_num].size() + 1);

      int csc_bucket = g_cachesims[idx]->on_block_seen(iterators[it_idx]);
      g_cachesims[idx]->incr_access(global_bucket);
      g_cachesims[idx]->incr_access_excl(csc_bucket);
      it_idx++;
    }
  }
#endif /* RD_DATASTRUCTS */
}

unsigned long abefore_cnt = 0;

void RD_accessBlock(Addr addr) {
  eprintf("%s\n", __func__);
  // [[maybe_unused]] static Addr addr_last;

  static Addr addr_last_arr[MAX_THREADS] = {0};
  bool accessed_before = (addr == addr_last_arr[PIN_ThreadId()]);

  if (accessed_before) {
    abefore_cnt++;
    // assert(g_cachesims[0]->stack_begin() == g_addrMap.find(addr)->second[0]);
    // on_accessed_before(addr);
  } else {
    auto it = g_addrMap.find(addr);

    if (it == g_addrMap.end()) {
      on_block_new(addr);
    } else {
      on_block_seen(it);
    }
  }

  addr_last_arr[PIN_ThreadId()] = addr;
}

static void free_memory() {
  for (auto &region : g_regions) {
    delete region.second;
    region.second = nullptr;
  }

  for (auto &cs_ptr : g_cachesims) {
    delete cs_ptr;
    cs_ptr = nullptr;
  }
}

void RD_print_csv() {
  const char *csv_header = "region,datastruct,addr,nbytes,line,ds_total_access_count,file_name,min,"
                           "access_count,access_exclusive,threads\n";

  // generate filename
  constexpr size_t FILENAME_SIZE = 256;
  char csv_filename[FILENAME_SIZE];
  snprintf(csv_filename, FILENAME_SIZE, "pindist.%s.%uthreads.csv", g_application_name.c_str(), g_max_threads);

  FILE *csv_out = fopen(csv_filename, "w");
  fprintf(csv_out, "%s", csv_header);

  printf("bla: %lu\n", abefore_cnt);

  for (auto &cs : g_cachesims) {
    cs->print_csv(csv_out, "main");
  }

  for (const auto &region : g_regions) {
    region.second->print_csv(csv_out);
  }

  free_memory();
  fclose(csv_out);
}

void RD_init() {
  g_addrMap.clear();
  // global cache simulation
  g_cachesims.push_back(new CacheSim{});
}

// get statistics
void RD_stat(unsigned long &stack_size, unsigned long &accessCount) {
  // if (RD_DEBUG)
  // RD_checkConsistency();

  unsigned long aCount = 0;
  for (const Bucket &b : g_cachesims[0]->buckets())
    aCount += b.aCount;

  stack_size = g_addrMap.size();
  accessCount = aCount;
}

// get resulting histogram
// Repeatly call RD_get_hist, start with bucket 0.
// Returns next bucket or 0 if this was last
int RD_get_hist(unsigned int b, unsigned int &min, unsigned long &accessCount) {
  // if (RD_DEBUG) RD_checkConsistency();

  assert((b >= 0) && (b < g_cachesims[0]->buckets().size()));
  min = Bucket::mins[b];
  accessCount = g_cachesims[0]->buckets()[b].aCount;
  if (b == g_cachesims[0]->buckets().size() - 1)
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

  bNext = RD_get_hist(0, min, aCount);
  fprintf(out, "%s[%8.3f MB ..] %s ==>\n", pStr, (double)(min * blockSize) / 1000000.0, formatLong(aCount, aCount));
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

    fprintf(out, "%s[%s] %s %s\n", pStr, bStr, formatLong(aCount, maxCount), formatBar(aCount, maxCount, 60));
    b = bNext;
  } while (b != 0);

  fprintf(out, "\n");

  RD_stat(stack_size, aCount);

  fprintf(out,
          "%sStatistics:\n"
          "%s  memory blocks accessed: %lu (%3.3f MB)\n"
          "%s  number of accesses:     %lu\n",
          pStr, pStr, stack_size, ((double)stack_size * blockSize) / 1000000.0, pStr, aCount);
}

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

  result_type operator()(first_argument_type __num, second_argument_type __den) const noexcept {
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
template <typename _Alloc, typename _ExtractKey, typename _Equal, typename _H1, typename _Hash, typename _RehashPolicy,
          typename _Traits>

class _Hashtable<Addr, std::pair<const Addr, list<MemoryBlock>::iterator>, _Alloc, _ExtractKey, _Equal, _H1,
                 __detail::_Mod_range_hashing, _Hash, _RehashPolicy, _Traits>
    : public _Hashtable<Addr, std::pair<const Addr, list<MemoryBlock>::iterator>, _Alloc, _ExtractKey, _Equal, _H1,
                        _Mod_myrange_hashing, _Hash, _RehashPolicy, _Traits> {
public:
  using myBase = _Hashtable<Addr, std::pair<const Addr, list<MemoryBlock>::iterator>, _Alloc, _ExtractKey, _Equal, _H1,
                            _Mod_myrange_hashing, _Hash, _RehashPolicy, _Traits>;

  using myBase::_Hashtable;
  using mySizeType = typename myBase::size_type;
};
} // namespace std
//-------------------------------------------------------------------------
#endif

#if OLD_CODE
/** done in CacheSim::check_consistency() **/

// do an internal consistency check
void RD_checkConsistency() {
  unsigned int aCount1, aCount2;
  unsigned int d = 0;
  int b = 0;
  aCount1 = 0;
  aCount2 = buckets[0].aCount;

  if (RD_VERBOSE > 0) {
    fprintf(stderr, "\nChecking... (stack size: %lu)\n", g_stack.size());
    fprintf(stderr, "   START Bucket %d (min depth %u): aCount %lu\n", b,
            buckets[b].min, buckets[b].aCount);
  }

  StackIterator stackIt;
  for (stackIt = g_stack.begin(); stackIt != g_stack.end(); ++stackIt, ++d) {
    if (d == buckets[b + 1].min) {
      b++;
      aCount2 += buckets[b].aCount;
      if (RD_VERBOSE > 0)
        fprintf(stderr, "   START Bucket %d (min depth %u): aCount %lu\n", b,
                buckets[b].min, buckets[b].aCount);
      assert(stackIt == buckets[b].marker);
    }

    if (RD_VERBOSE > 1) {
      static char b[100];
      stackIt->print(b);
      fprintf(stderr, "   %5d: %s\n", d, b);
    }
    aCount1 += stackIt->getACount();
    assert(stackIt->bucket == b);
  }
  assert(g_nextBucket = b + 1);
  b = buckets.size() - 1;
  aCount2 += buckets[b].aCount;
  if (RD_VERBOSE > 0) {
    fprintf(stderr, "   Last Bucket %d: aCount %lu\n", b, buckets[b].aCount);
    fprintf(stderr, "   Total aCount: %u\n", aCount1);
  }
#if MIN_BLOCKSTRUCT
  assert(buckets[b].aCount == g_stack.size());
#else
  assert(buckets[b].aCount == stack.size());
  assert(aCount1 == aCount2);
#endif
}
#endif
