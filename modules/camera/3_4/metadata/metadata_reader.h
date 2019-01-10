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

#ifndef DEFAULT_CAMERA_HAL_METADATA_METADATA_READER_H_
#define DEFAULT_CAMERA_HAL_METADATA_METADATA_READER_H_

#include <memory>
#include <set>
#include <vector>

#include <android-base/macros.h>
#include <camera/CameraMetadata.h>
#include "types.h"

namespace default_camera_hal {

// A MetadataReader reads and converts/validates various metadata entries.
class MetadataReader {
 public:
  MetadataReader(std::unique_ptr<const android::CameraMetadata> metadata);
  virtual ~MetadataReader();

  // Get a pointer to the underlying metadata being read.
  // The pointer is valid only as long as this object is alive.
  // The "locking" here only causes non-const methods to fail,
  // which is not a problem since the CameraMetadata being locked
  // is already const. This could be a problem if the metadata was
  // shared more widely, but |metadata_| is a unique_ptr,
  // guaranteeing the safety of this. Destructing automatically "unlocks".
  virtual const camera_metadata_t* raw_metadata() const {
    return metadata_->getAndLock();
  }

  // All accessor methods must be given a valid pointer. They will return:
  // 0: Success.
  // -ENOENT: The necessary entry is missing.
  // -EINVAL: The entry value is invalid.
  // -ENODEV: Some other error occured.

  // The |facing| returned will be one of the enum values from system/camera.h.
  virtual int Facing(int* facing) const;
  virtual int Orientation(int* orientation) const;
  virtual int MaxInputStreams(int32_t* max_input_streams) const;
  virtual int MaxOutputStreams(int32_t* max_raw_output_streams,
                               int32_t* max_non_stalling_output_streams,
                               int32_t* max_stalling_output_streams) const;
  virtual int RequestCapabilities(std::set<uint8_t>* capabilites) const;
  virtual int StreamConfigurations(
      std::vector<StreamConfiguration>* configs) const;
  virtual int StreamStallDurations(
      std::vector<StreamStallDuration>* stalls) const;
  virtual int ReprocessFormats(ReprocessFormatMap* reprocess_map) const;

 private:
  std::unique_ptr<const android::CameraMetadata> metadata_;

  DISALLOW_COPY_AND_ASSIGN(MetadataReader);
};

}  // namespace default_camera_hal

#endif  // DEFAULT_CAMERA_HAL_METADATA_METADATA_READER_H_
