/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef GRALLOC_PRIV_H_
#define GRALLOC_PRIV_H_

#include <stdint.h>
#include <asm/page.h>
#include <limits.h>
#include <sys/cdefs.h>
#include <hardware/gralloc.h>
#include <pthread.h>

#include <cutils/native_handle.h>

#if HAVE_ANDROID_OS
#include <linux/fb.h>
#endif

/*****************************************************************************/

inline size_t roundUpToPageSize(size_t x) {
    return (x + (PAGESIZE-1)) & ~(PAGESIZE-1);
}

int gralloc_map(gralloc_module_t const* module,
        buffer_handle_t handle, void** vaddr);

int gralloc_unmap(gralloc_module_t const* module, 
        buffer_handle_t handle);


int mapFrameBufferLocked(struct private_module_t* module);

/*****************************************************************************/

struct private_handle_t;

struct private_module_t {
    gralloc_module_t base;

    private_handle_t* framebuffer;
    uint32_t flags;
    uint32_t numBuffers;
    uint32_t bufferMask;
    pthread_mutex_t lock;
    buffer_handle_t currentBuffer;
    struct fb_var_screeninfo info;
    struct fb_fix_screeninfo finfo;
    float xdpi;
    float ydpi;
    float fps;
    
    enum {
        // flag to indicate we'll post this buffer
        PRIV_USAGE_LOCKED_FOR_POST = 0x80000000
    };
};

/*****************************************************************************/

struct private_handle_t : public native_handle
{
    enum {
        PRIV_FLAGS_FRAMEBUFFER = 0x00000001,
        PRIV_FLAGS_USES_PMEM   = 0x00000002,
        PRIV_FLAGS_MAPPED      = 0x00000004,    // FIXME: should be out-of-line
    };

    int     fd;
    int     magic;
    int     flags;
    int     size;
    // FIXME: the attributes below should be out-of-line
    int     base;
    int     lockState;
    int     writeOwner;

    static const int sNumInts = 6;
    static const int sNumFds = 1;
    static const int sMagic = 0x3141592;

    private_handle_t(int fd, int size, int flags) :
        fd(fd), magic(sMagic), flags(flags), size(size), base(0),
        lockState(0), writeOwner(0) 
    {
        version = sizeof(native_handle);
        numInts = sNumInts;
        numFds = sNumFds;
    }

    ~private_handle_t() {
        magic = 0;
    }

    bool usesPhysicallyContiguousMemory() {
        return (flags & PRIV_FLAGS_USES_PMEM) != 0;
    }

    static int validate(const native_handle* h) {
        if (!h || h->version != sizeof(native_handle) ||
                h->numInts!=sNumInts || h->numFds!=sNumFds) {
            return -EINVAL;
        }
        const private_handle_t* hnd = (const private_handle_t*)h;
        if (hnd->magic != sMagic)
            return -EINVAL;
        return 0;
    }

    static private_handle_t* dynamicCast(const native_handle* in) {
        if (validate(in) == 0) {
            return (private_handle_t*) in;
        }
        return NULL;
    }
};

/*****************************************************************************/

template<typename T>
struct growable_sorted_array_t {
    int size;
    int count;
    T* data;

    growable_sorted_array_t() : size(0), count(0), data(0) {
    }

    growable_sorted_array_t(int initialSize)
        : size(initialSize), count(0), data(0)
    {
        data = new T[initialSize];
    }

    ~growable_sorted_array_t() {
        delete[] data;
    }

    /** Returns true if we found an exact match.
     * Argument index is set to the the first index >= key in place.
     * Index will be in range 0..count inclusive.
     *
     */
    bool find(const T& key, int& index) {
        return binarySearch(0, count-1, key, index);
    }

    T* at(int index){
        if (index >= 0  && index < count) {
            return data + index;
        }
        return 0;
    }

    void insert(int index, const T& item) {
        if (index >= 0 && index <= count) {
            if (count + 1 > size) {
                int newSize = size * 2;
                if (newSize < count + 1) {
                    newSize = count + 1;
                }
                T* newData = new T[newSize];
                if (size > 0) {
                    memcpy(newData, data, sizeof(T) * count);
                }
                data = newData;
                size = newSize;
            }
            int toMove = count - index;
            if (toMove > 0) {
                memmove(data + index + 1, data + index, sizeof(T) * toMove);
            }
            count++;
            data[index] = item;
        }
    }

    void remove(int index) {
        if (index >= 0 && index < count) {
            int toMove = (count - 1) - index;
            if (toMove > 0) {
                memmove(data + index, data + index + 1, sizeof(T) * toMove);
            }
            count--;
        }
    }

    /** Return the first index >= key. May be in range first..last+1. */
    int binarySearch(int first, int last, const T& key, int& index)
    {
        while (first <= last) {
            int mid = (first + last) / 2;
            int cmp = compare(key, data[mid]);
            if (cmp > 0) {
                first = mid + 1;
            } else if (cmp < 0) {
                last = mid - 1;
            } else {
                index = mid;
                return true;
            }
        }
        index = first;
        return false;
    }
};


#endif /* GRALLOC_PRIV_H_ */
