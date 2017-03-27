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

#include "v4l2_gralloc.h"

#include <linux/videodev2.h>

#include <cstdlib>

#include <hardware/camera3.h>
#include <hardware/gralloc.h>
#include <system/graphics.h>

#include "common.h"
#include "stream_format.h"

namespace v4l2_camera_hal {

// Copy |height| lines from |src| to |dest|,
// where |src| and |dest| may have different line lengths.
void copyWithPadding(uint8_t* dest,
                     const uint8_t* src,
                     size_t dest_stride,
                     size_t src_stride,
                     size_t height) {
  size_t copy_stride = dest_stride;
  if (copy_stride > src_stride) {
    // Adding padding, not reducing. 0 out the extra memory.
    memset(dest, 0, src_stride * height);
    copy_stride = src_stride;
  }
  uint8_t* dest_line_start = dest;
  const uint8_t* src_line_start = src;
  for (size_t row = 0; row < height;
       ++row, dest_line_start += dest_stride, src_line_start += src_stride) {
    memcpy(dest_line_start, src_line_start, copy_stride);
  }
}

V4L2Gralloc* V4L2Gralloc::NewV4L2Gralloc() {
  // Initialize and check the gralloc module.
  const hw_module_t* module = nullptr;
  int res = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
  if (res || !module) {
    HAL_LOGE("Couldn't get gralloc module.");
    return nullptr;
  }
  const gralloc_module_t* gralloc =
      reinterpret_cast<const gralloc_module_t*>(module);

  // This class only supports Gralloc v0, not Gralloc V1.
  if (gralloc->common.module_api_version > GRALLOC_MODULE_API_VERSION_0_3) {
    HAL_LOGE(
        "Invalid gralloc version %x. Only 0.3 (%x) "
        "and below are supported by this HAL.",
        gralloc->common.module_api_version,
        GRALLOC_MODULE_API_VERSION_0_3);
    return nullptr;
  }

  return new V4L2Gralloc(gralloc);
}

// Private. As checked by above factory, module will be non-null
// and a supported version.
V4L2Gralloc::V4L2Gralloc(const gralloc_module_t* module) : mModule(module) {}

V4L2Gralloc::~V4L2Gralloc() {
  // Unlock buffers that are still locked.
  unlockAllBuffers();
}

int V4L2Gralloc::lock(const camera3_stream_buffer_t* camera_buffer,
                      uint32_t bytes_per_line,
                      v4l2_buffer* device_buffer) {
  // Lock the camera buffer (varies depending on if the buffer is YUV or not).
  std::unique_ptr<BufferData> buffer_data(
      new BufferData{camera_buffer, nullptr, bytes_per_line});
  buffer_handle_t buffer = *camera_buffer->buffer;
  void* data;
  camera3_stream_t* stream = camera_buffer->stream;
  int ret = 0;
  switch (StreamFormat::HalToV4L2PixelFormat(stream->format)) {
    // TODO(b/30119452): support more YCbCr formats.
    case V4L2_PIX_FMT_YUV420:
      android_ycbcr yuv_data;
      ret = mModule->lock_ycbcr(mModule,
                                buffer,
                                stream->usage,
                                0,
                                0,
                                stream->width,
                                stream->height,
                                &yuv_data);
      if (ret) {
        HAL_LOGE("Failed to lock ycbcr buffer: %d", ret);
        return ret;
      }

      // Check if gralloc format matches v4l2 format
      // (same padding, not interleaved, contiguous).
      if (yuv_data.ystride == bytes_per_line &&
          yuv_data.cstride == bytes_per_line / 2 && yuv_data.chroma_step == 1 &&
          (reinterpret_cast<uint8_t*>(yuv_data.cb) ==
           reinterpret_cast<uint8_t*>(yuv_data.y) +
               (stream->height * yuv_data.ystride)) &&
          (reinterpret_cast<uint8_t*>(yuv_data.cr) ==
           reinterpret_cast<uint8_t*>(yuv_data.cb) +
               (stream->height / 2 * yuv_data.cstride))) {
        // If so, great, point to the beginning.
        data = yuv_data.y;
      } else {
        // If not, allocate a contiguous buffer of appropriate size
        // (to be transformed back upon unlock).
        data = new uint8_t[device_buffer->length];
        // Make a dynamically-allocated copy of yuv_data,
        // since it will be needed at transform time.
        buffer_data->transform_dest.reset(new android_ycbcr(yuv_data));
      }
      break;
    case V4L2_PIX_FMT_JPEG:
      // Jpeg buffers are just contiguous blobs; lock length * 1.
      ret = mModule->lock(mModule,
                          buffer,
                          stream->usage,
                          0,
                          0,
                          device_buffer->length,
                          1,
                          &data);
      if (ret) {
        HAL_LOGE("Failed to lock jpeg buffer: %d", ret);
        return ret;
      }
      break;
    case V4L2_PIX_FMT_BGR32:  // Fall-through.
    case V4L2_PIX_FMT_RGB32:
      // RGB formats have nice agreed upon representation. Unless using android
      // flex formats.
      ret = mModule->lock(mModule,
                          buffer,
                          stream->usage,
                          0,
                          0,
                          stream->width,
                          stream->height,
                          &data);
      if (ret) {
        HAL_LOGE("Failed to lock RGB buffer: %d", ret);
        return ret;
      }
      break;
    default:
      return -EINVAL;
  }

  if (!data) {
    ALOGE("Gralloc lock returned null ptr");
    return -ENODEV;
  }

  // Set up the device buffer.
  static_assert(sizeof(unsigned long) >= sizeof(void*),
                "void* must be able to fit in the v4l2_buffer m.userptr "
                "field (unsigned long) for this code to work");
  device_buffer->m.userptr = reinterpret_cast<unsigned long>(data);

  // Note the mapping of data:buffer info for when unlock is called.
  mBufferMap.emplace(data, buffer_data.release());

  return 0;
}

int V4L2Gralloc::unlock(const v4l2_buffer* device_buffer) {
  // TODO(b/30000211): support multi-planar data (video_capture_mplane).
  if (device_buffer->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    return -EINVAL;
  }

  void* data = reinterpret_cast<void*>(device_buffer->m.userptr);

  // Find and pop the matching entry in the map.
  auto map_entry = mBufferMap.find(data);
  if (map_entry == mBufferMap.end()) {
    HAL_LOGE("No matching buffer for data at %p", data);
    return -EINVAL;
  }
  std::unique_ptr<const BufferData> buffer_data(map_entry->second);
  mBufferMap.erase(map_entry);

  const camera3_stream_buffer_t* camera_buffer = buffer_data->camera_buffer;
  const buffer_handle_t buffer = *camera_buffer->buffer;

  // Check for transform.
  if (buffer_data->transform_dest) {
    HAL_LOGV("Transforming V4L2 YUV to gralloc YUV.");
    // In this case data was allocated by this class, put it in a unique_ptr
    // to ensure it gets cleaned up no matter which way this function exits.
    std::unique_ptr<uint8_t[]> data_cleanup(reinterpret_cast<uint8_t*>(data));

    uint32_t bytes_per_line = buffer_data->v4l2_bytes_per_line;
    android_ycbcr* yuv_data = buffer_data->transform_dest.get();

    // Should only occur in error situations.
    if (device_buffer->bytesused == 0) {
      return -EINVAL;
    }

    // Transform V4L2 to Gralloc, copying each plane to the correct place,
    // adjusting padding, and interleaving if necessary.
    uint32_t height = camera_buffer->stream->height;
    // Y data first.
    size_t y_len = bytes_per_line * height;
    if (yuv_data->ystride == bytes_per_line) {
      // Data should match exactly.
      memcpy(yuv_data->y, data, y_len);
    } else {
      HAL_LOGV("Changing padding on Y plane from %u to %u.",
               bytes_per_line,
               yuv_data->ystride);
      // Wrong padding from V4L2.
      copyWithPadding(reinterpret_cast<uint8_t*>(yuv_data->y),
                      reinterpret_cast<uint8_t*>(data),
                      yuv_data->ystride,
                      bytes_per_line,
                      height);
    }
    // C data.
    // TODO(b/30119452): These calculations assume YCbCr_420_888.
    size_t c_len = y_len / 4;
    uint32_t c_bytes_per_line = bytes_per_line / 2;
    // V4L2 is packed, meaning the data is stored as contiguous {y, cb, cr}.
    uint8_t* cb_device = reinterpret_cast<uint8_t*>(data) + y_len;
    uint8_t* cr_device = cb_device + c_len;
    size_t step = yuv_data->chroma_step;
    if (step == 1) {
      // Still planar.
      if (yuv_data->cstride == c_bytes_per_line) {
        // Data should match exactly.
        memcpy(yuv_data->cb, cb_device, c_len);
        memcpy(yuv_data->cr, cr_device, c_len);
      } else {
        HAL_LOGV("Changing padding on C plane from %u to %u.",
                 c_bytes_per_line,
                 yuv_data->cstride);
        // Wrong padding from V4L2.
        copyWithPadding(reinterpret_cast<uint8_t*>(yuv_data->cb),
                        cb_device,
                        yuv_data->cstride,
                        c_bytes_per_line,
                        height / 2);
        copyWithPadding(reinterpret_cast<uint8_t*>(yuv_data->cr),
                        cr_device,
                        yuv_data->cstride,
                        c_bytes_per_line,
                        height / 2);
      }
    } else {
      // Desire semiplanar (cb and cr interleaved).
      HAL_LOGV("Interleaving cb and cr. Padding going from %u to %u.",
               c_bytes_per_line,
               yuv_data->cstride);
      uint32_t c_height = height / 2;
      uint32_t c_width = camera_buffer->stream->width / 2;
      // Zero out destination
      uint8_t* cb_gralloc = reinterpret_cast<uint8_t*>(yuv_data->cb);
      uint8_t* cr_gralloc = reinterpret_cast<uint8_t*>(yuv_data->cr);
      memset(cb_gralloc, 0, c_width * c_height * step);

      // Interleaving means we need to copy the cb and cr bytes one by one.
      for (size_t line = 0; line < c_height; ++line,
                  cb_gralloc += yuv_data->cstride,
                  cr_gralloc += yuv_data->cstride,
                  cb_device += c_bytes_per_line,
                  cr_device += c_bytes_per_line) {
        for (size_t i = 0; i < c_width; ++i) {
          *(cb_gralloc + (i * step)) = *(cb_device + i);
          *(cr_gralloc + (i * step)) = *(cr_device + i);
        }
      }
    }
  }

  // Unlock.
  int res = mModule->unlock(mModule, buffer);
  if (res) {
    HAL_LOGE("Failed to unlock buffer at %p", buffer);
    return -ENODEV;
  }

  return 0;
}

int V4L2Gralloc::unlockAllBuffers() {
  HAL_LOG_ENTER();

  bool failed = false;
  for (auto const& entry : mBufferMap) {
    int res = mModule->unlock(mModule, *entry.second->camera_buffer->buffer);
    if (res) {
      failed = true;
    }
    // When there is a transform to be made, the buffer returned by lock()
    // is dynamically allocated (to hold the pre-transform data).
    if (entry.second->transform_dest) {
      delete[] reinterpret_cast<uint8_t*>(entry.first);
    }
    // The BufferData entry is always dynamically allocated in lock().
    delete entry.second;
  }
  mBufferMap.clear();

  // If any unlock failed, return error.
  if (failed) {
    return -ENODEV;
  }

  return 0;
}

}  // namespace default_camera_hal
