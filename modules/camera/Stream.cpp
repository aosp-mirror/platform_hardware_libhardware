/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <pthread.h>
#include <hardware/camera3.h>
#include <hardware/gralloc.h>
#include <system/graphics.h>

//#define LOG_NDEBUG 0
#define LOG_TAG "Stream"
#include <cutils/log.h>

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL)
#include <cutils/trace.h>
#include "ScopedTrace.h"

#include "Stream.h"

namespace default_camera_hal {

Stream::Stream(int id, camera3_stream_t *s)
  : mReuse(false),
    mId(id),
    mStream(s),
    mType(s->stream_type),
    mWidth(s->width),
    mHeight(s->height),
    mFormat(s->format),
    mUsage(0),
    mMaxBuffers(0),
    mRegistered(false)
{
    // NULL (default) pthread mutex attributes
    pthread_mutex_init(&mMutex, NULL);
}

Stream::~Stream()
{
    // TODO: unregister buffers from hw
}

void Stream::setUsage(uint32_t usage)
{
    pthread_mutex_lock(&mMutex);
    if (usage != mUsage) {
        mUsage = usage;
        mRegistered = false;
        // TODO: unregister buffers from hw
    }
    pthread_mutex_unlock(&mMutex);
}

void Stream::setMaxBuffers(uint32_t max_buffers)
{
    pthread_mutex_lock(&mMutex);
    if (max_buffers != mMaxBuffers) {
        mMaxBuffers = max_buffers;
        mRegistered = false;
        // TODO: unregister buffers from hw
    }
    pthread_mutex_unlock(&mMutex);
}

int Stream::getType()
{
    return mType;
}

bool Stream::isInputType()
{
    return mType & (CAMERA3_STREAM_INPUT | CAMERA3_STREAM_BIDIRECTIONAL);
}

bool Stream::isOutputType()
{
    return mType & (CAMERA3_STREAM_OUTPUT | CAMERA3_STREAM_BIDIRECTIONAL);
}

bool Stream::isValidReuseStream(int id, camera3_stream_t *s)
{
    if (id != mId) {
        ALOGE("%s:%d: Invalid camera id for reuse. Got %d expect %d",
                __func__, mId, id, mId);
        return false;
    }
    if (s != mStream) {
        ALOGE("%s:%d: Invalid stream handle for reuse. Got %p expect %p",
                __func__, mId, s, mStream);
        return false;
    }
    if (s->stream_type != mType) {
        // TODO: prettyprint type string
        ALOGE("%s:%d: Mismatched type in reused stream. Got %d expect %d",
                __func__, mId, s->stream_type, mType);
        return false;
    }
    if (s->format != mFormat) {
        // TODO: prettyprint format string
        ALOGE("%s:%d: Mismatched format in reused stream. Got %d expect %d",
                __func__, mId, s->format, mFormat);
        return false;
    }
    if (s->width != mWidth) {
        ALOGE("%s:%d: Mismatched width in reused stream. Got %d expect %d",
                __func__, mId, s->width, mWidth);
        return false;
    }
    if (s->height != mHeight) {
        ALOGE("%s:%d: Mismatched height in reused stream. Got %d expect %d",
                __func__, mId, s->height, mHeight);
        return false;
    }
    return true;
}

} // namespace default_camera_hal
