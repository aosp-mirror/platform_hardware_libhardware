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


#ifndef GRALLOC_ALLOCATOR_H_
#define GRALLOC_ALLOCATOR_H_

#include <stdint.h>
#include <sys/types.h>

#include "gr.h"

// ----------------------------------------------------------------------------

/*
 * A simple templatized doubly linked-list implementation
 */

template <typename NODE>
class LinkedList
{
    NODE*  mFirst;
    NODE*  mLast;

public:
                LinkedList() : mFirst(0), mLast(0) { }
    bool        isEmpty() const { return mFirst == 0; }
    NODE const* head() const { return mFirst; }
    NODE*       head() { return mFirst; }
    NODE const* tail() const { return mLast; }
    NODE*       tail() { return mLast; }

    void insertAfter(NODE* node, NODE* newNode) {
        newNode->prev = node;
        newNode->next = node->next;
        if (node->next == 0) mLast = newNode;
        else                 node->next->prev = newNode;
        node->next = newNode;
    }

    void insertBefore(NODE* node, NODE* newNode) {
         newNode->prev = node->prev;
         newNode->next = node;
         if (node->prev == 0)   mFirst = newNode;
         else                   node->prev->next = newNode;
         node->prev = newNode;
    }

    void insertHead(NODE* newNode) {
        if (mFirst == 0) {
            mFirst = mLast = newNode;
            newNode->prev = newNode->next = 0;
        } else {
            newNode->prev = 0;
            newNode->next = mFirst;
            mFirst->prev = newNode;
            mFirst = newNode;
        }
    }
    
    void insertTail(NODE* newNode) {
        if (mLast == 0) {
            insertHead(newNode);
        } else {
            newNode->prev = mLast;
            newNode->next = 0;
            mLast->next = newNode;
            mLast = newNode;
        }
    }

    NODE* remove(NODE* node) {
        if (node->prev == 0)    mFirst = node->next;
        else                    node->prev->next = node->next;
        if (node->next == 0)    mLast = node->prev;
        else                    node->next->prev = node->prev;
        return node;
    }
};

class SimpleBestFitAllocator
{
public:

    SimpleBestFitAllocator();
    SimpleBestFitAllocator(size_t size);
    ~SimpleBestFitAllocator();

    ssize_t     setSize(size_t size);

    ssize_t     allocate(size_t size, uint32_t flags = 0);
    ssize_t     deallocate(size_t offset);
    size_t      size() const;

private:
    struct chunk_t {
        chunk_t(size_t start, size_t size) 
            : start(start), size(size), free(1), prev(0), next(0) {
        }
        size_t              start;
        size_t              size : 28;
        int                 free : 4;
        mutable chunk_t*    prev;
        mutable chunk_t*    next;
    };

    ssize_t  alloc(size_t size, uint32_t flags);
    chunk_t* dealloc(size_t start);

    static const int    kMemoryAlign;
    mutable Locker      mLock;
    LinkedList<chunk_t> mList;
    size_t              mHeapSize;
};

#endif /* GRALLOC_ALLOCATOR_H_ */
