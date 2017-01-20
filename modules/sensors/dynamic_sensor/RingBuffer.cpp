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

#include "RingBuffer.h"

#include <stdlib.h>
#include <string.h>

namespace android {

RingBuffer::RingBuffer(size_t size)
    : mSize(size),
      mData((sensors_event_t *)malloc(sizeof(sensors_event_t) * mSize)),
      mReadPos(0),
      mWritePos(0) {
}

RingBuffer::~RingBuffer() {
    free(mData);
    mData = NULL;
}

ssize_t RingBuffer::write(const sensors_event_t *ev, size_t size) {
    Mutex::Autolock autoLock(mLock);

    size_t numAvailableToRead = mWritePos - mReadPos;
    size_t numAvailableToWrite = mSize - numAvailableToRead;

    if (size > numAvailableToWrite) {
        size = numAvailableToWrite;
    }

    size_t writePos = (mWritePos % mSize);
    size_t copy = mSize - writePos;

    if (copy > size) {
        copy = size;
    }

    memcpy(&mData[writePos], ev, copy * sizeof(sensors_event_t));

    if (size > copy) {
        memcpy(mData, &ev[copy], (size - copy) * sizeof(sensors_event_t));
    }

    mWritePos += size;

    if (numAvailableToRead == 0 && size > 0) {
        mNotEmptyCondition.broadcast();
    }

    return size;
}

ssize_t RingBuffer::read(sensors_event_t *ev, size_t size) {
    Mutex::Autolock autoLock(mLock);

    size_t numAvailableToRead;
    for (;;) {
        numAvailableToRead = mWritePos - mReadPos;
        if (numAvailableToRead > 0) {
            break;
        }

        mNotEmptyCondition.wait(mLock);
    }

    if (size > numAvailableToRead) {
        size = numAvailableToRead;
    }

    size_t readPos = (mReadPos % mSize);
    size_t copy = mSize - readPos;

    if (copy > size) {
        copy = size;
    }

    memcpy(ev, &mData[readPos], copy * sizeof(sensors_event_t));

    if (size > copy) {
        memcpy(&ev[copy], mData, (size - copy) * sizeof(sensors_event_t));
    }

    mReadPos += size;

    return size;
}

}  // namespace android

