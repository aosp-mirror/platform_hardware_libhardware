/*
 * Copyright (C) 2015 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "Stream"

#include <stdio.h>

#include <log/log.h>
#include <utils/Mutex.h>

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL)
#include <utils/Trace.h>

#include <hardware/camera3.h>
#include <hardware/gralloc.h>
#include <system/graphics.h>

#include "Stream.h"

namespace usb_camera_hal {

Stream::Stream(int id, camera3_stream_t *s)
  : mReuse(false),
    mId(id),
    mStream(s){
}

Stream::~Stream() {
    for (size_t i = 0; i < mBuffers.size(); i++) {
        delete mBuffers[i];
    }

    mBuffers.clear();
}

void Stream::setUsage(uint32_t usage) {
    android::Mutex::Autolock al(mLock);
    if (usage != mStream->usage) {
        mStream->usage = usage;
    }
}

void Stream::setMaxBuffers(uint32_t max_buffers) {
    android::Mutex::Autolock al(mLock);
    if (max_buffers != mStream->max_buffers) {
        mStream->max_buffers = max_buffers;
    }
}

int Stream::getType() {
    return mStream->stream_type;
}

bool Stream::isInputType() {
    return mStream->stream_type == CAMERA3_STREAM_INPUT ||
            mStream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL;
}

bool Stream::isOutputType() {
    return mStream->stream_type == CAMERA3_STREAM_OUTPUT ||
            mStream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL;
}

const char* Stream::typeToString(int type) {
    switch (type) {
    case CAMERA3_STREAM_INPUT:
        return "CAMERA3_STREAM_INPUT";
    case CAMERA3_STREAM_OUTPUT:
        return "CAMERA3_STREAM_OUTPUT";
    case CAMERA3_STREAM_BIDIRECTIONAL:
        return "CAMERA3_STREAM_BIDIRECTIONAL";
    }
    return "Invalid stream type!";
}

const char* Stream::formatToString(int format) {
    // See <system/graphics.h> for full list
    switch (format) {
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return "BGRA 8888";
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return "RGBA 8888";
    case HAL_PIXEL_FORMAT_RGBX_8888:
        return "RGBX 8888";
    case HAL_PIXEL_FORMAT_RGB_888:
        return "RGB 888";
    case HAL_PIXEL_FORMAT_RGB_565:
        return "RGB 565";
    case HAL_PIXEL_FORMAT_Y8:
        return "Y8";
    case HAL_PIXEL_FORMAT_Y16:
        return "Y16";
    case HAL_PIXEL_FORMAT_YV12:
        return "YV12";
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
        return "NV16";
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        return "NV21";
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        return "YUY2";
    case HAL_PIXEL_FORMAT_RAW10:
        return "RAW10";
    case HAL_PIXEL_FORMAT_RAW16:
        return "RAW16";
    case HAL_PIXEL_FORMAT_BLOB:
        return "BLOB";
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
        return "IMPLEMENTATION DEFINED";
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
        return "FLEXIBLE YCbCr 420 888";
    }
    return "Invalid stream format!";
}

bool Stream::isValidReuseStream(int id, camera3_stream_t *s) {
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
    if (s->stream_type != mStream->stream_type) {
        ALOGE("%s:%d: Mismatched type in reused stream. Got %s(%d) "
                "expect %s(%d)", __func__, mId, typeToString(s->stream_type),
                s->stream_type, typeToString(mStream->stream_type), mStream->stream_type);
        return false;
    }
    if (s->format != mStream->format) {
        ALOGE("%s:%d: Mismatched format in reused stream. Got %s(%d) "
                "expect %s(%d)", __func__, mId, formatToString(s->format),
                s->format, formatToString(mStream->format), mStream->format);
        return false;
    }
    if (s->width != mStream->width) {
        ALOGE("%s:%d: Mismatched width in reused stream. Got %d expect %d",
                __func__, mId, s->width, mStream->width);
        return false;
    }
    if (s->height != mStream->height) {
        ALOGE("%s:%d: Mismatched height in reused stream. Got %d expect %d",
                __func__, mId, s->height, mStream->height);
        return false;
    }
    return true;
}

void Stream::dump(int fd) {
    android::Mutex::Autolock al(mLock);

    dprintf(fd, "Stream ID: %d (%p)\n", mId, mStream);
    dprintf(fd, "Stream Type: %s (%d)\n", typeToString(mStream->stream_type), mStream->stream_type);
    dprintf(fd, "Width: %" PRIu32 " Height: %" PRIu32 "\n", mStream->width, mStream->height);
    dprintf(fd, "Stream Format: %s (%d)", formatToString(mStream->format), mStream->format);
    // ToDo: prettyprint usage mask flags
    dprintf(fd, "Gralloc Usage Mask: %#" PRIx32 "\n", mStream->usage);
    dprintf(fd, "Max Buffer Count: %" PRIu32 "\n", mStream->max_buffers);
    dprintf(fd, "Number of Buffers in use by HAL: %zu\n", mBuffers.size());
    for (size_t i = 0; i < mBuffers.size(); i++) {
        dprintf(fd, "Buffer %zu/%zu: %p\n", i, mBuffers.size(), mBuffers[i]);
    }
}

} // namespace usb_camera_hal
