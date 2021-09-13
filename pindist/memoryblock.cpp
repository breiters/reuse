#include "memoryblock.h"
#include "datastructs.h"

#if MIN_BLOCKSTRUCT
// generated on first access
MemoryBlock::MemoryBlock(Addr a, int num) : bucket{0}, ds_num{num}, a{a} {}
#else
// TODO

#endif
