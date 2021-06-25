/* Calculation of a Stack Reuse Distance Histogram
 *
 * (C) 2015, Josef Weidendorfer / LRR-TUM
 * GPLv2+ (see COPYING)
 */

#pragma once

#include <algorithm>
// #include <bits/charconv.h>
#include <cassert>
#include <iostream>
#include <list>
#include <stdio.h>
#include <unordered_map>
#include <vector>

typedef void *Addr;

// Assertions and consistency check?
#define RD_DEBUG 0

// 2: Huge amount of debug output, 1: checks, 0: silent
#define RD_VERBOSE 0

// use minimal memory block element? Prohibits consistency checks
#define MIN_BLOCKSTRUCT 1

//-------------------------------------------------------------------------
// Stack structure with MemoryBlock as element

#if MIN_BLOCKSTRUCT
class MemoryBlock {
public:
  MemoryBlock(Addr a); // generated on first access
  MemoryBlock(Addr a, int num);

  inline void print(char *b) { sprintf(b, "block at bucket %d", bucket); }
  inline void incACount() {}
  inline unsigned long getACount() { return 1; }

  int bucket;    // current bucket
  int ds_bucket; // current bucket if datastructure had exclusive cache
  int ds_num;    // datastructure it belongs to

  Addr a;
};
#else
class MemoryBlock {
public:
  MemoryBlock(Addr a) {
    addr = a;
    bucket = 0;
    aCount = 1;
  } // generated on first access
  void print(char *b) {
    sprintf(b, "block %p, bucket %d, aCount %lu", addr, bucket, aCount);
  }
  void incACount() { aCount++; }
  unsigned long getACount() { return aCount; }

  int bucket;

private:
  Addr addr;
  unsigned long aCount;
};
#endif

class Bucket {
public:
  Bucket(int m);

  using Marker = std::list<MemoryBlock>::iterator;

  unsigned long aCount;
  unsigned int min;
  Marker marker;

  std::vector<unsigned long> ds_aCount;
  std::vector<unsigned long> ds_aCount_excl;
  std::vector<Marker> ds_markers;

  void register_datastruct();
};

// initialize / clear used structs
void RD_init(int min1);

// add distance buckets, starting from smallest (>0)
// only specification of minimal distance required
// use as last bucket min=0 to get "infinite distances" there
void RD_addBucket(unsigned int min);

// run stack simulation
// To use a specific block size, ensure that <a> is aligned
void RD_accessBlock(Addr a);

// do an internal consistency check
void RD_checkConsistency();

// get statistics
void RD_stat(unsigned long &stack_size, unsigned long &accessCount);

// get resulting histogram
// Repeatly call RD_get_hist, start with bucket 0.
// Returns next bucket or 0 if this was last
int RD_get_hist(unsigned int bucket, unsigned int &min,
                unsigned long &accessCount);

// print nice ASCII histogram to <out>
//  <pStr> is prefix for every line, distances scaled by <blockSize>
void RD_printHistogram(FILE *out, const char *pStr, int blockSize);
