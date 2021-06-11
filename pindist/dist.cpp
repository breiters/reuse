
/* Calculation of a Stack Reuse Distance Histogram
 *
 * (C) 2015, Josef Weidendorfer / LRR-TUM
 * GPLv2+ (see COPYING)
 */


#include "dist.h"
#include "ds.hpp"

#include <algorithm>
// #include <bits/charconv.h>
#include <cassert>
#include <iostream>
#include <list>
#include <stdio.h>
#include <unordered_map>
#include <vector>

using namespace std;

//-------------------------------------------------------------------------
// Stack structure with MemoryBlock as element

list<MemoryBlock> stack;

typedef list<MemoryBlock>::iterator Marker;

vector<datastruct_info> datastructs;

vector<Bucket> buckets;
int nextBucket; // when stack is growing, we may enter this bucket

unordered_map<Addr,list<MemoryBlock>::iterator> addrMap;

#if MIN_BLOCKSTRUCT
MemoryBlock::MemoryBlock(Addr a) : bucket{0}, ds_num{0} {}// generated on first access
MemoryBlock::MemoryBlock(Addr a, int num) : bucket{0}, ds_num{num} {}
#else

#endif

Bucket::Bucket(int m)  {
    aCount = 0;
    min = m;
    marker = stack.end();
}

void Bucket::register_datastruct() {
    ds_aCount.push_back(0);
    ds_aCount_excl.push_back(0);
    ds_markers_excl.push_back(marker); // TODO
}

void Bucket::on_datastruct_get_active(int ds_num, list<MemoryBlock>::iterator stack_pos) { }

void register_datastruct(datastruct_info &info) {
  datastructs.push_back(info);
  for(auto &bucket : buckets) {
    bucket.register_datastruct();
    // assert(bucket.ds_aCount.size() == datastructs.size());
    // assert(bucket.ds_markers_excl.size() == datastructs.size());
  }
}

int get_ds_num(Addr addr)
   {
     int i = 0;
     for(auto &ds : datastructs) {
       if((ADDRINT)addr >= (ADDRINT)ds.address && (ADDRINT)addr < (ADDRINT)ds.address + (ADDRINT)ds.nbytes - 1) {
        //  printf("datastructure accessed:\n");
        //  ds.print();
         return i;
       }
       i++;
     }
     return -1;
   }

/*
   void add_bucket(uint min) {
     assert(buckets.size() > 2);
     assert(buckets[buckets.size()-2].min < min);
     buckets.insert( buckets.end()-1, Bucket(min) );
   }

   void move_markers(int topBucket) {
    //  for(auto bucket : buckets.)
    for(int b=1; b<topBucket; b++) {
      --buckets.at(b).marker;
      (buckets.at(b).marker)->bucket++;
      if (RD_DEBUG)
        assert( (buckets[b].marker)->bucket == b );
    }
   }

   bool last_bucket_reached() {
     return buckets[nextBucket].min == 0;
   }

   uint next_bucket_capacity() {
     return buckets[nextBucket].min;
   }

   void activate_next_bucket() {

   if (RD_VERBOSE >0)
    fprintf(stderr," ACTIVATE bucket %d (next bucket minimum depth %d)\n",
	    nextBucket, buckets[nextBucket+1].min);

  --buckets[nextBucket].marker; // set marker to last entry
  (buckets[nextBucket].marker)->bucket++; 
  if (RD_DEBUG)
    assert( (buckets[nextBucket].marker)->bucket == nextBucket );

  nextBucket++;

   }
   */

  //  int nextBucket() { return nextBucket; }

// TODO: refactor ...
#include "ds.cpp"

#if FUTURE_IMPROVED_HASH
//-------------------------------------------------------------------------
// Specialization of unordered_map to use masking for bucket calculation
struct _Mod_myrange_hashing
{
    typedef std::size_t first_argument_type;
    typedef std::size_t second_argument_type;
    typedef std::size_t result_type;

    static std::size_t mask;

    _Mod_myrange_hashing() {}
    // ??
    // _Mod_myrange_hashing(const __detail::_Mod_range_hashing &temp) {} 

    result_type
    operator()(first_argument_type __num,
	       second_argument_type __den) const noexcept
    {
	if (mask < __den) {
		std::size_t n = __den-1;
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

// ?
/*
namespace std {
template<typename _Alloc,
	 typename _ExtractKey, typename _Equal,
	 typename _H1, typename _Hash,
	 typename _RehashPolicy, typename _Traits>

class _Hashtable<Addr,  std::pair<const Addr, list<MemoryBlock>::iterator>,
	_Alloc,_ExtractKey, _Equal,
	_H1, __detail::_Mod_range_hashing, _Hash,
	_RehashPolicy,  _Traits>
	: public _Hashtable<Addr,
	std::pair<const Addr, list<MemoryBlock>::iterator>,
	_Alloc, _ExtractKey, _Equal, _H1, _Mod_myrange_hashing,
	_Hash, _RehashPolicy, _Traits>
{
public:
    using myBase = _Hashtable<Addr,
    std::pair<const Addr, list<MemoryBlock>::iterator>,
    _Alloc, _ExtractKey, _Equal, _H1, _Mod_myrange_hashing,
    _Hash, _RehashPolicy, _Traits>;

    using myBase::_Hashtable;
    using mySizeType = typename myBase::size_type;
};
}
*/
//-------------------------------------------------------------------------
#endif

void RD_init(int min1)
{
  stack.clear();
  addrMap.clear();
  datastructs.clear();
  //addrMap.rehash(4000000);

  buckets.clear();
  buckets.push_back( Bucket(0) );    // bucket starting with distance 0
  buckets.push_back( Bucket(min1) ); // first real bucket of interest
  buckets.push_back( Bucket(0) );    // for "infinite" distance
  nextBucket = 1;
}

// add distance buckets, starting from smallest (>0)
// only specification of minimal distance required
void RD_addBucket(unsigned int min)
{
  //fprintf(stderr, "Add bucket with dist %d (last dist: %d)\n",
  //   min, buckets[buckets.size()-2].min);
  assert(buckets.size() > 2);
  assert(buckets[buckets.size()-2].min < min);

  buckets.insert( buckets.end()-1, Bucket(min) );
}


// do an internal consistency check
void RD_checkConsistency()
{
  unsigned int aCount1, aCount2;
  unsigned int d = 0;
  int b = 0;
  aCount1 = 0;
  aCount2 = buckets[0].aCount;

  if (RD_VERBOSE>0) {
    fprintf(stderr,"\nChecking... (stack size: %lu)\n", stack.size());
    fprintf(stderr,"   START Bucket %d (min depth %u): aCount %lu\n",
	    b, buckets[b].min, buckets[b].aCount );
  }

  list<MemoryBlock>::iterator stackIt;
  for(stackIt = stack.begin(); stackIt != stack.end(); ++stackIt, ++d) {
    if (d == buckets[b+1].min) {
      b++;
      aCount2 += buckets[b].aCount;
      if (RD_VERBOSE>0)
	fprintf(stderr,"   START Bucket %d (min depth %u): aCount %lu\n",
		b, buckets[b].min, buckets[b].aCount );
      assert( stackIt == buckets[b].marker );
    }

    if (RD_VERBOSE>1) {
      static char b[100];
      stackIt->print(b);
      fprintf(stderr,"   %5d: %s\n", d, b);
    }
    aCount1 += stackIt->getACount();
    assert( stackIt->bucket == b );
  }
  assert( nextBucket = b+1 );
  b = buckets.size()-1;
  aCount2 += buckets[b].aCount;
  if (RD_VERBOSE>0) {
    fprintf(stderr,"   Last Bucket %d: aCount %lu\n",
	    b, buckets[b].aCount );
    fprintf(stderr,"   Total aCount: %u\n", aCount1);
  }
#if MIN_BLOCKSTRUCT
  assert( buckets[b].aCount == stack.size() );
#else
  assert( buckets[b].aCount == stack.size() );
  assert( aCount1 == aCount2 );
#endif
}


void moveMarkers(int topBucket)
{
  for(int b=1; b<=topBucket; b++) {
    --buckets.at(b).marker;
    (buckets.at(b).marker)->bucket++;
    if (RD_DEBUG)
      assert( (buckets[b].marker)->bucket == b );
  }
}

void handleNewBlock(Addr a)
{
  int ds_num = get_ds_num(a);
  // new memory block
  stack.push_front( MemoryBlock(a, ds_num) );
  addrMap[a] = stack.begin();

  if (RD_VERBOSE >1)
    fprintf(stderr," NEW block %p, now %lu blocks seen\n", a, stack.size());

  // move all markers of active buckets
  moveMarkers(nextBucket-1);

  // does another bucket get active?
  if (addrMap.size() <= buckets[nextBucket].min) return;
  if (buckets[nextBucket].min == 0) return; // last bucket reached

  if (RD_VERBOSE >0)
    fprintf(stderr," ACTIVATE bucket %d (next bucket minimum depth %d)\n",
	    nextBucket, buckets[nextBucket+1].min);

  --buckets[nextBucket].marker; // set marker to last entry
  (buckets[nextBucket].marker)->bucket++;
  if (RD_DEBUG)
    assert( (buckets[nextBucket].marker)->bucket == nextBucket );

  nextBucket++;
}

void moveBlockToTop(Addr a, list<MemoryBlock>::iterator blockIt, int bucket)
{
  // move markers of active buckets within range 1 to <bucket>.
  // we need to do this before moving blockIt to top, as
  // there could be a marker on blockIt, which would go wrong
  moveMarkers(bucket);

  // move element pointed to by blockIt to beginning of list
  stack.splice(stack.begin(), stack, blockIt); // TODO
  // stack.remove(blockIt);
  // stack.insert(blockIt);

  blockIt->incACount();
  blockIt->bucket = 0;

  if (RD_VERBOSE >1)
    fprintf(stderr," MOVED %p from bucket %d to top (aCount %lu)\n",
	    a, bucket, blockIt->getACount());
}


// run stack simulation
// To use a specific block size, ensure that <a> is aligned
void RD_accessBlock(Addr a)
{
  int bucket;
  auto it = addrMap.find(a);
  if (it == addrMap.end()) {
    handleNewBlock(a);
    bucket = buckets.size()-1; // "infinite" distance, put in last bucket
  }
  else {
    // memory block already seen
    auto blockIt = it->second;
    bucket = blockIt->bucket;

    if (blockIt != stack.begin()) {
      moveBlockToTop(a, blockIt, bucket);
    }
    else {
      blockIt->incACount();
      if (RD_VERBOSE >1)
	fprintf(stderr," TOP %p accessed, bucket %d, aCount %lu\n",
		a, bucket, blockIt->getACount());
    }
  }

  // global access count
  buckets[bucket].aCount++;

  // access count to datastructure
  int ds_num = get_ds_num(a);
  if(ds_num > -1) {
    // printf("ds_num: %d %d %d %d\n", ds_num, (int)datastructs.size(), (int)buckets[bucket].ds_aCount.size(), bucket);
    assert((int) datastructs.size() >= ds_num - 1);
    assert(buckets[bucket].ds_aCount.size() == datastructs.size());
    buckets[bucket].ds_aCount[ds_num]++;
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


// get statistics
void RD_stat(unsigned long & stack_size, unsigned long & accessCount) //, vector<unsigned long> )
{
  if (RD_DEBUG)
    RD_checkConsistency();

  unsigned long aCount = 0;
  for(const Bucket& b: buckets)
    aCount += b.aCount;

  stack_size = addrMap.size();
  accessCount = aCount;
}


// get resulting histogram
// Repeatly call RD_get_hist, start with bucket 0.
// Returns next bucket or 0 if this was last
int RD_get_hist(unsigned int b,
		unsigned int & min, unsigned long & accessCount)
{
  //if (RD_DEBUG) RD_checkConsistency();

  assert((b>=0) && (b < buckets.size()));
  min = buckets[b].min;
  accessCount = buckets[b].aCount;
  if (b == buckets.size()-1) return 0;
  return b+1;
}

int RD_get_hist_ds(int ds_num, unsigned int b,
		unsigned int & min, unsigned long & accessCount)
{
  //if (RD_DEBUG) RD_checkConsistency();

  assert((b>=0) && (b < buckets.size()));
  min = buckets[b].min;
  accessCount = buckets[b].ds_aCount[ds_num];
  if (b == buckets.size()-1) return 0;
  return b+1;
}


// print nice ASCII histogram

// helpers
char* formatLong(unsigned long aCount, unsigned long maxCount)
{
  static char out[20];

  if (maxCount > 999999999)
    sprintf(out, "%7.3f G", (double) aCount / 1000000000.0);
  else if (maxCount > 999999)
    sprintf(out, "%7.3f M", (double) aCount / 1000000.0);
  else if (aCount > 999)
    sprintf(out, "%3d %03d  ",
	    (int) aCount / 1000, (int)(aCount % 1000));
  else
    sprintf(out, "    %3d  ", (int) aCount);

  return out;
}

char* formatBar(unsigned long aCount, unsigned long maxCount, int len)
{
  int i, size;
  static char bar[110];

  if (len>100) len=100;
  for(i=0;i<len;i++)
    bar[i] = '#';
  bar[i] = 0;

  size = (int)((double)aCount/maxCount*len);
  if (size > len) size = len;
  return bar + len - size;
}

// print histogram + statistics to <out>
//  <pStr> prefix for every line
void RD_printHistogram(FILE* out, const char* pStr, int blockSize)
{
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
    if (aCount > maxCount) maxCount = aCount;
    if ((min>0) && (aCount > 0)) maxNonEmptyBucket = b;
    if (b>0) maxBucket = b;
    b = bNext;
  } while(b!=0);

  if (maxNonEmptyBucket < maxBucket)
    maxNonEmptyBucket++;

  //  fprintf(out, "%sMaxBucket %d NonEmpty %d\n",
  //          pStr, maxBucket, maxNonEmptyBucket);

  bNext = RD_get_hist(0, min, aCount);
  fprintf(out, "%s[%8.3f MB ..] %s ==>\n",
	  pStr, (double)(min * blockSize)/1000000.0,
	  formatLong(aCount,aCount));
  b = bNext;
  do {
    bNext = RD_get_hist(b, min, aCount);
    if ((b>maxNonEmptyBucket) && (aCount == 0)) {
      b = bNext;
      continue;
    }

    if (min>0)
      sprintf(bStr, "%8.3f MB ..",
	      (double)(min * blockSize) / 1024.0 / 1024.0);
    else
      sprintf(bStr, "   inf/cold   ");

    fprintf(out, "%s[%s] %s %s\n", pStr, bStr,
	    formatLong(aCount, maxCount),
	    formatBar(aCount, maxCount, 60));
    b = bNext;
  } while(b!=0);

int ds_num = 0;
  for (auto ds : datastructs)
{
  ds.print();

  bNext = RD_get_hist_ds(ds_num, 0, min, aCount);
  fprintf(out, "%s[%8.3f MB ..] %s ==>\n",
	  pStr, (double)(min * blockSize)/1000000.0,
	  formatLong(aCount,aCount));
  b = bNext;
  do {
    bNext = RD_get_hist_ds(ds_num, b, min, aCount);
    if ((b>maxNonEmptyBucket) && (aCount == 0)) {
      b = bNext;
      continue;
    }

    if (min>0)
      sprintf(bStr, "%8.3f MB ..",
	      (double)(min * blockSize) / 1024.0 / 1024.0);
    else
      sprintf(bStr, "   inf/cold   ");

    fprintf(out, "%s[%s] %s %s\n", pStr, bStr,
	    formatLong(aCount, maxCount),
	    formatBar(aCount, maxCount, 60));
    b = bNext;
  } while(b!=0);

  ds_num++;
}

  RD_stat(stack_size, aCount);

  fprintf(out,
	  "%sStatistics:\n"
	  "%s  memory blocks accessed: %lu (%3.3f MB)\n"
	  "%s  number of accesses:     %lu\n",
	  pStr, pStr, stack_size,
	  ((double)stack_size * blockSize)/1000000.0,
	  pStr, aCount);
}
