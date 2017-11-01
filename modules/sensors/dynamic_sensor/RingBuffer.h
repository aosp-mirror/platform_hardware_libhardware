/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef RING_BUFFER_H_

#define RING_BUFFER_H_

#include <media/stagefright/foundation/ABase.h>

#include <hardware/sensors.h>
#include <utils/threads.h>

namespace android {

class RingBuffer {
public:
    explicit RingBuffer(size_t size);
    ~RingBuffer();

    ssize_t write(const sensors_event_t *ev, size_t size);
    ssize_t read(sensors_event_t *ev, size_t size);

private:
    Mutex mLock;
    Condition mNotEmptyCondition;

    size_t mSize;
    sensors_event_t *mData;
    size_t mReadPos, mWritePos;

    DISALLOW_EVIL_CONSTRUCTORS(RingBuffer);
};

}  // namespace android

#endif  // RING_BUFFER_H_
