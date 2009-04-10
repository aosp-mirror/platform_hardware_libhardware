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

#include <limits.h>
#include <errno.h>
#include <pthread.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include "gralloc_priv.h"

/*****************************************************************************/

struct mapped_buffer_t {
    intptr_t key;
    int size;
    int base;
    int refCount;

    int init(private_handle_t* hnd) {
        size = hnd->size;
        base = 0;
        refCount = 0;
        struct stat buf;
        int result = 0;
        key = intptr_t(hnd);
        return result;
    }
};

static int compare(const mapped_buffer_t& a, const mapped_buffer_t& b) {
    if (a.key < b.key) {
        return -1;
    }
    if (a.key > b.key) {
        return 1;
    }
    return 0;
}

struct mapped_buffers_t {
    pthread_mutex_t mutex;
    growable_sorted_array_t<mapped_buffer_t> records;

    int map(gralloc_module_t const* module,
            buffer_handle_t handle,
            void** vaddr)
    {
        private_handle_t* hnd = (private_handle_t*)(handle);
        mapped_buffer_t key;
        int result = key.init(hnd);
        //printRecord(ANDROID_LOG_DEBUG, "map", &key);
        if (result) {
            return result;
        }
        mapped_buffer_t* record = 0;
        pthread_mutex_lock(&mutex);
        // From here to the end of the function we return by jumping to "exit"
        // so that we always unlock the mutex.

        int index = -1;
        if (!records.find(key, index)) {
            //LOGD("New item at %d", index);
            void* mappedAddress = mmap(0, hnd->size,
                    PROT_READ|PROT_WRITE, MAP_SHARED, hnd->fd, 0);
            if (mappedAddress == MAP_FAILED) {
                result = -errno;
                //LOGE("map failed %d", result);
                goto exit;
            }
            key.base = intptr_t(mappedAddress);
            records.insert(index, key);
            record = records.at(index);
        } else {
            //LOGD("Found existing mapping at index %d", index);
            record = records.at(index);
            if (record->size != key.size) {
                LOGE("Requested a new mapping at the same offset"
                     "but with a different size");
                printRecord(ANDROID_LOG_ERROR, "old", record);
                printRecord(ANDROID_LOG_ERROR, "new", &key);
                result = -EINVAL;
                goto exit;
            }
        }
        record->refCount += 1;
        hnd->base = record->base;
        *vaddr = (void*) record->base;
    exit:
        //print();
        pthread_mutex_unlock(&mutex);
        return result;
    }

    int unmap(gralloc_module_t const* module,
            buffer_handle_t handle)
    {
        mapped_buffer_t key;
        key.init((private_handle_t*) handle);
        //printRecord(ANDROID_LOG_DEBUG, "unmap", &key);
        int index = -1;

        int result = 0;
        mapped_buffer_t* record = 0;
        pthread_mutex_lock(&mutex);
        // From here to the end of the function we return by jumping to "exit"
        // so that we always unlock the mutex.

        if (!records.find(key, index)) {
            // This handle is not currently locked.
            //LOGE("Could not find existing mapping near %d", index);
            result = -ENOENT;
            goto exit;
        }
        record = records.at(index);
        //printRecord(ANDROID_LOG_DEBUG, "record", record);
        record->refCount -= 1;
        if (record->refCount == 0) {
            //LOGD("Unmapping...");
            if (munmap((void*)record->base, record->size) < 0) {
                result = -errno;
                //LOGE("Could not unmap %d", result);
            }
            records.remove(index);
            ((private_handle_t*)handle)->base = 0;
        }

    exit:
        //print();
        pthread_mutex_unlock(&mutex);
        return result;
    }

    void print() {
        char prefix[16];
        LOGD("Dumping records: count=%d size=%d", records.count, records.size);
        for(int i=0; i<records.count ; i++) {
            sprintf(prefix, "%3d", i);
            printRecord(ANDROID_LOG_DEBUG, prefix, records.at(i));
        }
    }

    void printRecord(int level, const char* what, mapped_buffer_t* record) {
        LOG_PRI(level, LOG_TAG,
          "%s: key=0x%08x, size=0x%08x, base=0x%08x refCount=%d",
           what, int(record->key), record->size, 
           record->base, record->refCount);
    }
};

static mapped_buffers_t sMappedBuffers  = {
    mutex: PTHREAD_MUTEX_INITIALIZER
};

/*****************************************************************************/

int gralloc_map(gralloc_module_t const* module,
        buffer_handle_t handle,
        void** vaddr)
{
    if (private_handle_t::validate(handle) < 0)
        return -EINVAL;

    private_handle_t* hnd = (private_handle_t*)(handle);
    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
        const private_module_t* m = 
            reinterpret_cast<const private_module_t*>(module);
        handle = m->framebuffer;
    }
    
    int err = sMappedBuffers.map(module, handle, vaddr);
    
    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
        *vaddr = (void*)hnd->base;
    }
    
    return err;
}

int gralloc_unmap(gralloc_module_t const* module,
        buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0)
        return -EINVAL;

    private_handle_t* hnd = (private_handle_t*)(handle);
    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
        const private_module_t* m = 
            reinterpret_cast<const private_module_t*>(module);
        handle = m->framebuffer;
    }

    return sMappedBuffers.unmap(module, handle);
}


int gralloc_lock(gralloc_module_t const* module,
        buffer_handle_t handle, int usage,
        int l, int t, int w, int h)
{
    // FIXME: gralloc_lock() needs implementation
    return 0;
}

int gralloc_unlock(gralloc_module_t const* module, 
        buffer_handle_t handle)
{
    // FIXME: gralloc_unlock() needs implementation
    return 0;
}
