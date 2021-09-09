#include "memoryblock.h"
#include "ds.h"

#if MIN_BLOCKSTRUCT
// generated on first access
MemoryBlock::MemoryBlock(Addr a)
    : bucket{0}, ds_bucket{0}, ds_num{DATASTRUCT_UNKNOWN} {}
MemoryBlock::MemoryBlock(Addr a, int num)
    : bucket{0}, ds_bucket{0}, ds_num{num}, a{a} {}
#else
// TODO
#endif
