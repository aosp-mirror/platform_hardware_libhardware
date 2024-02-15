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

#ifndef GR_H_
#define GR_H_

#include <stdint.h>
#include <sys/user.h>
#include <limits.h>
#include <sys/cdefs.h>
#include <hardware/gralloc.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include <cutils/native_handle.h>

/*****************************************************************************/

struct private_module_t;
struct private_handle_t;

static const size_t kPageSize = getpagesize();

inline size_t roundUpToPageSize(size_t x) {
    return (x + (kPageSize-1)) & ~(kPageSize-1);
}

int mapFrameBufferLocked(struct private_module_t* module, int format);
int terminateBuffer(gralloc_module_t const* module, private_handle_t* hnd);
int mapBuffer(gralloc_module_t const* module, private_handle_t* hnd);

/*****************************************************************************/

class Locker {
    pthread_mutex_t mutex;
public:
    class Autolock {
        Locker& locker;
    public:
        inline explicit Autolock(Locker& locker) : locker(locker) {  locker.lock(); }
        inline ~Autolock() { locker.unlock(); }
    };
    inline Locker()        { pthread_mutex_init(&mutex, 0); }
    inline ~Locker()       { pthread_mutex_destroy(&mutex); }
    inline void lock()     { pthread_mutex_lock(&mutex); }
    inline void unlock()   { pthread_mutex_unlock(&mutex); }
};

#endif /* GR_H_ */
