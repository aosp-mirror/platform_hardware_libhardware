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

#ifndef STREAM_H_
#define STREAM_H_

#include <hardware/camera3.h>
#include <hardware/gralloc.h>
#include <system/graphics.h>
#include <utils/Mutex.h>
#include <utils/Vector.h>

namespace usb_camera_hal {
// Stream represents a single input or output stream for a camera device.
class Stream {
    public:
        Stream(int id, camera3_stream_t *s);
        ~Stream();

        // validate that a stream's parameters match this stream's parameters
        bool isValidReuseStream(int id, camera3_stream_t *s);

        void setUsage(uint32_t usage);
        void setMaxBuffers(uint32_t max_buffers);

        int getType();
        bool isInputType();
        bool isOutputType();
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
        // Array of handles to buffers currently in use by the stream
        android::Vector<buffer_handle_t *> mBuffers;
        // Lock protecting the Stream object for modifications
        android::Mutex mLock;
};
} // namespace usb_camera_hal

#endif // STREAM_H_
