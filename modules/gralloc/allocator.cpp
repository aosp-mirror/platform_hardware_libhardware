/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cutils/log.h>

#include "allocator.h"


// align all the memory blocks on a cache-line boundary
const int SimpleBestFitAllocator::kMemoryAlign = 32;

SimpleBestFitAllocator::SimpleBestFitAllocator()
    : mHeapSize(0)
{
}

SimpleBestFitAllocator::SimpleBestFitAllocator(size_t size)
    : mHeapSize(0)
{
    setSize(size);
}

SimpleBestFitAllocator::~SimpleBestFitAllocator()
{
    while(!mList.isEmpty()) {
        delete mList.remove(mList.head());
    }
}

ssize_t SimpleBestFitAllocator::setSize(size_t size)
{
    Locker::Autolock _l(mLock);
    if (mHeapSize != 0) return -EINVAL;
    size_t pagesize = getpagesize();
    mHeapSize = ((size + pagesize-1) & ~(pagesize-1));
    chunk_t* node = new chunk_t(0, mHeapSize / kMemoryAlign);
    mList.insertHead(node);
    return size;
}
    
    
size_t SimpleBestFitAllocator::size() const
{
    return mHeapSize;
}

ssize_t SimpleBestFitAllocator::allocate(size_t size, uint32_t flags)
{
    Locker::Autolock _l(mLock);
    if (mHeapSize == 0) return -EINVAL;
    ssize_t offset = alloc(size, flags);
    return offset;
}

ssize_t SimpleBestFitAllocator::deallocate(size_t offset)
{
    Locker::Autolock _l(mLock);
    if (mHeapSize == 0) return -EINVAL;
    chunk_t const * const freed = dealloc(offset);
    if (freed) {
        return 0;
    }
    return -ENOENT;
}

ssize_t SimpleBestFitAllocator::alloc(size_t size, uint32_t flags)
{
    if (size == 0) {
        return 0;
    }
    size = (size + kMemoryAlign-1) / kMemoryAlign;
    chunk_t* free_chunk = 0;
    chunk_t* cur = mList.head();

    size_t pagesize = getpagesize();
    while (cur) {
        int extra = ( -cur->start & ((pagesize/kMemoryAlign)-1) ) ;

        // best fit
        if (cur->free && (cur->size >= (size+extra))) {
            if ((!free_chunk) || (cur->size < free_chunk->size)) {
                free_chunk = cur;
            }
            if (cur->size == size) {
                break;
            }
        }
        cur = cur->next;
    }

    if (free_chunk) {
        const size_t free_size = free_chunk->size;
        free_chunk->free = 0;
        free_chunk->size = size;
        if (free_size > size) {
            int extra = ( -free_chunk->start & ((pagesize/kMemoryAlign)-1) ) ;
            if (extra) {
                chunk_t* split = new chunk_t(free_chunk->start, extra);
                free_chunk->start += extra;
                mList.insertBefore(free_chunk, split);
            }

            LOGE_IF(((free_chunk->start*kMemoryAlign)&(pagesize-1)),
                    "page is not aligned!!!");

            const ssize_t tail_free = free_size - (size+extra);
            if (tail_free > 0) {
                chunk_t* split = new chunk_t(
                        free_chunk->start + free_chunk->size, tail_free);
                mList.insertAfter(free_chunk, split);
            }
        }
        return (free_chunk->start)*kMemoryAlign;
    }
    return -ENOMEM;
}

SimpleBestFitAllocator::chunk_t* SimpleBestFitAllocator::dealloc(size_t start)
{
    start = start / kMemoryAlign;
    chunk_t* cur = mList.head();
    while (cur) {
        if (cur->start == start) {
            LOG_FATAL_IF(cur->free,
                "block at offset 0x%08lX of size 0x%08lX already freed",
                cur->start*kMemoryAlign, cur->size*kMemoryAlign);

            // merge freed blocks together
            chunk_t* freed = cur;
            cur->free = 1;
            do {
                chunk_t* const p = cur->prev;
                chunk_t* const n = cur->next;
                if (p && (p->free || !cur->size)) {
                    freed = p;
                    p->size += cur->size;
                    mList.remove(cur);
                    delete cur;
                }
                cur = n;
            } while (cur && cur->free);

            #ifndef NDEBUG
                if (!freed->free) {
                    dump_l("dealloc (!freed->free)");
                }
            #endif
            LOG_FATAL_IF(!freed->free,
                "freed block at offset 0x%08lX of size 0x%08lX is not free!",
                freed->start * kMemoryAlign, freed->size * kMemoryAlign);

            return freed;
        }
        cur = cur->next;
    }
    return 0;
}
