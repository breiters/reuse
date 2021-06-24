#include "cachesim.h"

CacheSim::CacheSim(int datastruct_num) : 
    stack_size_{0},
    next_bucket_{1},
    datastruct_num_{datastruct_num}
    // buckets_{buckets}
{
    // stack_.clear();
}

CacheSim::~CacheSim()
{
}

list<MemoryBlock>::iterator CacheSim::on_new_block(MemoryBlock &mb)
{
    stack_size_++;
    stack_.push_front(mb);

    assert(stack_size_ == stack_.size());

    move_markers(next_bucket_ - 1);

    // printf(__FILE__ " in %s"  " line: %d\n", __func__, __LINE__);

    // does another bucket get active?
    bool last_bucket_reached = buckets[next_bucket_].min == 0;
    bool next_bucket_active = stack_.size() > buckets[next_bucket_].min;

    if(!last_bucket_reached && next_bucket_active) {
        // set new buckets marker to stack end first then set marker to last stack element
        buckets[next_bucket_].ds_markers[datastruct_num_] = stack_.end();
        --(buckets[next_bucket_].ds_markers[datastruct_num_]);

        // printf("bef: %d, %d\n", buckets[next_bucket_].ds_markers[datastruct_num_]->ds_bucket, next_bucket_);
        
        assert(buckets[next_bucket_].ds_markers[datastruct_num_] != stack_.begin());
        assert(buckets[next_bucket_].ds_markers[datastruct_num_] != stack_.end());

        // last stack element is now in the next higher bucket
        buckets[next_bucket_].ds_markers[datastruct_num_]->ds_bucket++;

        // buckets[next_bucket_ + 1].ds_markers[datastruct_num_] = stack_.end();
        // assert(buckets[next_bucket_ + 1].ds_markers[datastruct_num_] == stack_.end());

        // if (RD_DEBUG)
        // printf("aft: %d, %d\n", buckets[next_bucket_].ds_markers[datastruct_num_]->ds_bucket, next_bucket_);
        assert(buckets[next_bucket_].ds_markers[datastruct_num_]->ds_bucket == next_bucket_ );
        next_bucket_++;
    }

    return stack_.begin();
}

void CacheSim::on_block_seen(list<MemoryBlock>::iterator &blockIt)
{
    // if already on top do nothing
    if(blockIt == stack_.begin()) {
        return;
    }

    // move all markers below current memory blocks bucket
    int bucket = blockIt->ds_bucket;
    move_markers(bucket);

    // put current memory block on top and set its buckets to zero
    stack_.splice(stack_.begin(), stack_, blockIt);

    assert(blockIt->bucket == 0);
    blockIt->ds_bucket = 0;
}

void CacheSim::move_markers(int topBucket)
{
    // printf(__FILE__ " in %s"  " line: %d\n", __func__, __LINE__);
    for(int b=1; b<=topBucket; b++) {
        assert(buckets[b].ds_markers[datastruct_num_] != stack_.begin());
        assert(buckets[b].ds_markers[datastruct_num_] != stack_.end());

        --(buckets[b].ds_markers[datastruct_num_]);

        assert((int)buckets[b].ds_markers.size() > datastruct_num_);
        assert(!stack_.empty());
        assert(buckets[b].ds_markers[datastruct_num_] != stack_.begin());
        assert(buckets[b].ds_markers[datastruct_num_] != stack_.end());

        buckets[b].ds_markers[datastruct_num_]->ds_bucket++;

        // if (RD_DEBUG)
            // assert( (buckets[b].marker)->bucket == b );
    }
}