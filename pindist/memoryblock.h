#pragma once

#include <cstdio>
#include <list>

// use minimal memory block element? Prohibits consistency checks
#define MIN_BLOCKSTRUCT 1

//-------------------------------------------------------------------------
// Stack structure with MemoryBlock as element
typedef void *Addr;

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

using Marker = std::list<MemoryBlock *>::iterator;
