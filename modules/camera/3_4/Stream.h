/*
 * Copyright (C) 2016 The Android Open Source Project
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

// Modified from hardware/libhardware/modules/camera/Stream.h

#ifndef STREAM_H_
#define STREAM_H_

#include <hardware/camera3.h>
#include <hardware/gralloc.h>
#include <system/graphics.h>
#include <utils/Mutex.h>

namespace default_camera_hal {
// Stream represents a single input or output stream for a camera device.
class Stream {
    public:
        Stream(int id, camera3_stream_t *s);
        ~Stream();

        // validate that astream's parameters match this stream's parameters
        bool isValidReuseStream(int id, camera3_stream_t *s);

        void setUsage(uint32_t usage);
        void setMaxBuffers(uint32_t max_buffers);
        void setDataSpace(android_dataspace_t data_space);

        inline int getFormat() const { return mFormat; };
        inline int getType() const { return mType; };
        inline uint32_t getWidth() const { return mWidth; };
        inline uint32_t getHeight() const { return mHeight; };
        inline int getRotation() const { return mRotation; };

        bool isInputType() const;
        bool isOutputType() const;
        const char* typeToString(int type);
        const char* formatToString(int format);
        void dump(int fd);

        // This stream is being reused. Used in stream configuration passes
        bool mReuse;

    private:

        // The camera device id this stream belongs to
        const int mId;
        // Handle to framework's stream, used as a cookie for buffers
        camera3_stream_t *mStream;
        // Stream type: CAMERA3_STREAM_* (see <hardware/camera3.h>)
        const int mType;
        // Width in pixels of the buffers in this stream
        const uint32_t mWidth;
        // Height in pixels of the buffers in this stream
        const uint32_t mHeight;
        // Gralloc format: HAL_PIXEL_FORMAT_* (see <system/graphics.h>)
        const int mFormat;
        // Gralloc usage mask : GRALLOC_USAGE_* (see <hardware/gralloc.h>)
        uint32_t mUsage;
        // Output rotation this stream should undergo
        const int mRotation;
        // Color space of image data.
        android_dataspace_t mDataSpace;
        // Max simultaneous in-flight buffers for this stream
        uint32_t mMaxBuffers;
        // Lock protecting the Stream object for modifications
        android::Mutex mLock;
};
} // namespace default_camera_hal

#endif // STREAM_H_
