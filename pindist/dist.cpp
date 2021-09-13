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

#define DEBUG(x) printf(#x " : %d\n", x)

using namespace std;

extern std::string g_application_name;

// TODO: do not check more than one time if an address is in a CacheSim
extern std::vector<std::vector<int>> ds_in_cs;

//-------------------------------------------------------------------------
// Stack structure with MemoryBlock as element
typedef list<MemoryBlock>::iterator Marker;

// list<MemoryBlock> g_stack;
vector<Bucket> g_buckets;
int g_nextBucket; // when stack is growing, we may enter this bucket
CacheSim g_cachesim;
#define RD_NO_DATASTRUCT -1

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

struct MarkerContainer {
  MarkerContainer(Marker, Marker, std::vector<Marker>);
  MarkerContainer();
  Marker global_marker;
  Marker ds_marker;
  std::vector<Marker> combine_markers;
};

MarkerContainer::MarkerContainer() {}
MarkerContainer::MarkerContainer(Marker g, Marker ds, std::vector<Marker> cds)
    : global_marker{g}, ds_marker{ds}, combine_markers{cds} {};

using addr_map = std::unordered_map<Addr, MarkerContainer, std::hash<S>>;
addr_map g_addrMap;

static void on_accessed_before() {
  //puts(__func__);
  // for all caches where ds_num is included:
  // increment bucket 0 access count
  // and bucket 0 exclusive access count
  //
  int ds_num = g_cachesim.stack().begin()->ds_num;

  // global cache
  g_cachesim.incr_access(0);

  if (ds_num != RD_NO_DATASTRUCT) {
    // single datastruct
    g_cachesims[ds_num].incr_access(0);
    g_cachesims[ds_num].incr_access_excl(0);

    // combined datastructs
    for (int csc : ds_in_cs[ds_num]) {
      g_cachesims_combined[csc].incr_access(0);
      g_cachesims_combined[csc].incr_access_excl(0);
    }
  }
}

static void on_block_new(Addr addr) {
  //puts(__func__);
  // for all caches where ds_num is included:
  // increment bucket inf access count (done in CacheSim::on_block_new)
  // and bucket inf exclusive access count (done in CacheSim::on_block_new)
  //
  // add block to stack and move markers and increment bucket of memory block
  // that move to next bucket
  //
  //puts("1");
  int ds_num = datastruct_num(addr);

#if USE_POINTERS
  // PIN stdlib port does not support shared_ptr...
  // ... so we use raw pointers here and don't care for best practices
#endif
  MemoryBlock mb = MemoryBlock{addr, ds_num};

  // Marker it;
  // std::vector<Marker> stack_begins;

  // new memory block
  // returns begin of stack
  MarkerContainer mc{};
  //puts("2");
  mc.global_marker = g_cachesim.on_block_new(mb);
  //puts("3");
  g_cachesim.incr_access_inf();
  //puts("4");

  if (ds_num != RD_NO_DATASTRUCT) {
    mc.ds_marker = g_cachesims[ds_num].on_block_new(mb);
    g_cachesims[ds_num].incr_access_inf();
    g_cachesims[ds_num].incr_access_excl_inf();

    // account for access to combined dastructs
    for (int csc : ds_in_cs[ds_num]) {
      assert(g_cachesims_combined[csc].contains(ds_num));
      mc.combine_markers.push_back(g_cachesims_combined[csc].on_block_new(mb));
      g_cachesims_combined[csc].incr_access_inf();
      g_cachesims_combined[csc].incr_access_excl_inf();
    }
  }

  // add new marker container to map
  g_addrMap[addr] = mc;

#if USE_OLD_CODE
  if (RD_VERBOSE > 1)
    fprintf(stderr, " NEW block %p, now %lu blocks seen\n", addr,
            g_stack.size());

  if (RD_VERBOSE > 0)
    fprintf(stderr, " ACTIVATE bucket %d (next bucket minimum depth %d)\n",
            g_nextBucket, g_buckets[g_nextBucket + 1].min);
#endif
}

#if USE_OLD_CLODE
bucket = blockIt->bucket;
ds_num = blockIt->ds_num;

// if not already on top of stack, move to top
if (blockIt != g_stack.begin()) {
  moveBlockToTop(addr, blockIt, bucket);
} else {
  blockIt->incACount();
  if (RD_VERBOSE > 1)
    fprintf(stderr, " TOP %p accessed, bucket %d, aCount %lu\n", addr, bucket,
            blockIt->getACount());
}
#endif

static void on_block_seen(addr_map::iterator it) {
  //puts(__func__);
  // memory block already seen
  auto markers = it->second;

  int global_bucket = g_cachesim.on_block_seen(markers.global_marker);
  g_cachesim.incr_access(global_bucket);

  int ds_num = markers.global_marker->ds_num;

  if (ds_num != RD_NO_DATASTRUCT) {
    int bucket = g_cachesims[ds_num].on_block_seen(markers.ds_marker);
    g_cachesims[ds_num].incr_access(global_bucket);
    g_cachesims[ds_num].incr_access_excl(bucket);

    int m = 0;
    for (int csd : ds_in_cs[ds_num]) {
      assert(g_cachesims_combined[csd].contains(ds_num));
      g_cachesims_combined[csd].on_block_seen(markers.combine_markers[m]);
      g_cachesims_combined[csd].incr_access(global_bucket);
      g_cachesims_combined[csd].incr_access_excl(bucket);
      m++;
    }
  }
}

void RD_accessBlock(Addr addr) {
  [[maybe_unused]] static Addr addr_last;

  if (addr == addr_last) {
    on_accessed_before();
  } else {
    auto it = g_addrMap.find(addr);

    if (it == g_addrMap.end()) {
      // memory block not seen yet
      on_block_new(addr);
    } else {
      on_block_seen(it);
    }
  }

  addr_last = addr;

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

void RD_init_finish() {
  g_cachesim = CacheSim{-1};
}

#if OLD_CLODE
void moveMarkers(int topBucket) {
  for (int b = 1; b <= topBucket; b++) {
    --g_buckets.at(b).marker;
    (g_buckets.at(b).marker)->bucket++;
    if (RD_DEBUG)
      assert((g_buckets[b].marker)->bucket == b);
  }
}

void moveBlockToTop(Addr addr, Marker blockIt, int bucket) {
  // move markers of active buckets within range 1 to <bucket>.
  // we need to do this before moving blockIt to top, as
  // there could be a marker on blockIt, which would go wrong
  moveMarkers(bucket);

  // move element pointed to by blockIt to beginning of list
  g_stack.splice(g_stack.begin(), g_stack, blockIt);

  blockIt->incACount();
  blockIt->bucket = 0;

  if (RD_VERBOSE > 1)
    fprintf(stderr, " MOVED %p from bucket %d to top (aCount %lu)\n", addr,
            bucket, blockIt->getACount());
}

// run stack simulation
// To use a specific block size, ensure that <a> is aligned
void RD_accessBlock(Addr addr) {
  [[maybe_unused]] static Addr addr_last;

  int bucket; // bucket of current access
  int ds_bucket = 0;
  int ds_num;

  vector<int> ds_buckets_combined;

  // bool accessed_before = (addr == addr_last);
  // was memory block accessed before? --> don't need to search in hash map!
  // memory block is on top of stack
  // if (addr == addr_last) {
  if (0) { // TODO
    // increase bucket 0 and ds_bucket 0
    ds_num = g_stack.begin()->ds_num;
    bucket = 0;
    ds_bucket = 0;

    // sanity check if address of memory block on top of stack is same as a:
    assert(g_stack.begin()->a == addr);
    if (ds_num != RD_NO_DATASTRUCT) {
      assert(g_cachesims[ds_num].stack().begin()->a == addr);
    }
  } else {
    auto it = g_addrMap.find(addr);

    if (it == g_addrMap.end()) {
      // memory block not seen yet
      ds_num = handleNewBlock(addr);
      bucket = g_buckets.size() - 1; // "infinite" distance, put in last bucket
      ds_bucket = g_buckets.size() - 1;

    } else {
      // memory block already seen
      auto markers = it->second;
      auto blockIt = markers.global_marker;

      bucket = blockIt->bucket;
      ds_num = blockIt->ds_num;

      // if not already on top of stack, move to top
      if (blockIt != g_stack.begin()) {
        moveBlockToTop(addr, blockIt, bucket);
      } else {
        blockIt->incACount();
        if (RD_VERBOSE > 1)
          fprintf(stderr, " TOP %p accessed, bucket %d, aCount %lu\n", addr,
                  bucket, blockIt->getACount());
      }

      if (ds_num != RD_NO_DATASTRUCT) {
        ds_bucket = (markers.ds_marker)->bucket;

        g_cachesims[ds_num].on_block_seen(markers.ds_marker);

        // DEBUG(ds_num);
        int m = 0;
        for (int i : ds_in_cs[ds_num]) {
          assert(g_cachesims_combined[i].contains(ds_num));
          // DEBUG(i);
          ds_buckets_combined.push_back((markers.combine_markers[m])->bucket);
          g_cachesims_combined[i].on_block_seen(markers.combine_markers[m]);
          m++;
        }
// DEBUG(ds_num);
// int cs_num = 0;
#if 0
        int i = 0;
        for (auto &cs : g_cachesims_combined) {
          if (cs.contains(ds_num)) {
            DEBUG(i);
            assert(g_cachesims_combined[i].contains(ds_num));
            // buckets_combined.push_back(std::make_pair(
            // (*markers.combine_markers[i])->ds_bucket, cs_num));
            // DEBUG(i);
            ds_buckets_combined.push_back((markers.combine_markers[i])->bucket);
            cs.on_block_seen(markers.combine_markers[i]);
            i++;
          }
          // cs_num++;
        }
#endif
        /*
         */
        // exit(0);
      }
    }
  }

  // store last accessed address
  addr_last = addr;

  // global access count on bucket
  g_buckets[bucket].aCount++;

  assert(bucket <= (int)g_buckets.size() - 1);

  // access count to datastructure
  if (ds_num != RD_NO_DATASTRUCT) {
    g_datastructs[ds_num].access_count++;
    assert(ds_bucket > -1);
    assert(ds_bucket <= (int)g_buckets.size() - 1);

    g_cachesims[ds_num].buckets()[bucket].aCount++;
    g_cachesims[ds_num].buckets()[ds_bucket].aCount_excl++;

    // TODO: CAREFULL IF VECTOR IS EMPTY WHEN A == A_LAST!!!

    if (ds_buckets_combined.size() == 0) {
      for (auto &cs : g_cachesims_combined) {
        if (cs.contains(ds_num)) {
          // case: infinite distance
          cs.buckets()[g_buckets.size() - 1].aCount++;
          cs.buckets()[g_buckets.size() - 1].aCount_excl++;
        }
      }
    } else {

      int i = 0;
      for (auto &cs : g_cachesims_combined) {
        if (cs.contains(ds_num)) {
          assert(ds_buckets_combined[i] > -1);
          assert(ds_buckets_combined[i] < (int)g_buckets.size() - 1);
          cs.buckets()[bucket].aCount++;
          cs.buckets()[ds_buckets_combined[i]].aCount_excl++;
          i++;
        }
      }
    }
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

#endif

#define CSV_FORMAT "%s,%d,%p,%zu,%d,%lu,%s,%u,%lu,%lu,%lu\n"

void RD_print_csv() {
  const char *csv_header =
      "region,datastruct,addr,nbytes,line,ds_total_access_count,file_name,"
      "min,"
      "access_count,datastruct_access_count,datastruct_access_exclusive\n";

  // generate filename
  constexpr size_t FILENAME_SIZE = 256;
  char csv_filename[FILENAME_SIZE];
  snprintf(csv_filename, FILENAME_SIZE, "pindist.%s.csv",
           g_application_name.c_str());

  FILE *csv_out = fopen(csv_filename, "w");
  fprintf(csv_out, "%s", csv_header);

#if 0
  g_buckets.back().min =
      std::numeric_limits<decltype(g_buckets.back().min)>::max();
  for (auto &b : g_buckets) {
    fprintf(csv_out, CSV_FORMAT, "main", -1, (void *)0x0, (size_t)0, 0, 0UL,
            "main", b.min, b.aCount, 0UL, 0UL);
  }
#endif

  const char *region = "main";

  g_cachesim.print_csv(csv_out, region);

  for (auto &cs : g_cachesims) {
    cs.print_csv(csv_out, region);
  }

  for (auto &cs : g_cachesims_combined) {
    cs.print_csv(csv_out, region);
  }

  for (const auto &region : g_regions) {
    region.second->print_csv(csv_out);
  }

  // cleanup : should be done elsewhere
  for (auto &region : g_regions) {
    delete region.second;
  }

  fclose(csv_out);
}

void RD_init(int min1) {
  g_addrMap.clear();
  g_datastructs.clear();
  // addrMap.rehash(4000000);

  g_buckets.clear();
  g_buckets.push_back(Bucket(0));    // bucket starting with distance 0
  g_buckets.push_back(Bucket(min1)); // first real bucket of interest
  g_buckets.push_back(Bucket(BUCKET_INF_DIST)); // for "infinite" distance
  g_nextBucket = 1;
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

  fprintf(out, "\n");

  RD_stat(stack_size, aCount);

  fprintf(out,
          "%sStatistics:\n"
          "%s  memory blocks accessed: %lu (%3.3f MB)\n"
          "%s  number of accesses:     %lu\n",
          pStr, pStr, stack_size, ((double)stack_size * blockSize) / 1000000.0,
          pStr, aCount);
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

#if 0
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
      stackIt->print(b);
      fprintf(stderr, "   %5d: %s\n", d, b);
    }
    aCount1 += stackIt->getACount();
    assert(stackIt->bucket == b);
  }
  assert(g_nextBucket = b + 1);
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
#endif
