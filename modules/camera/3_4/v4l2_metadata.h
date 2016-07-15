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

#ifndef V4L2_CAMERA_HAL_V4L2_METADATA_H_
#define V4L2_CAMERA_HAL_V4L2_METADATA_H_

#include <hardware/camera3.h>

#include "common.h"
#include "metadata/partial_metadata_interface.h"
#include "v4l2_wrapper.h"

namespace v4l2_camera_hal {
class V4L2Metadata {
 public:
  V4L2Metadata(V4L2Wrapper* device);
  virtual ~V4L2Metadata();

  int FillStaticMetadata(camera_metadata_t** metadata);
  bool IsValidRequest(const camera_metadata_t* metadata);
  int SetRequestSettings(const camera_metadata_t* metadata);
  int FillResultMetadata(camera_metadata_t** metadata);

 protected:
  // Helper for the constructor to fill in metadata components.
  // Virtual/protected so tests can subclass and override populating with mocks.
  virtual void PopulateComponents();

 private:
  // Helper for the constructor to fill in metadata components.
  void AddComponent(std::unique_ptr<PartialMetadataInterface> component);

  // Access to the device.
  V4L2Wrapper* device_;
  // The overall metadata is broken down into several distinct pieces.
  // Note: it is undefined behavior if multiple components share tags.
  std::vector<std::unique_ptr<PartialMetadataInterface>> components_;

  friend class V4L2MetadataTest;

  DISALLOW_COPY_AND_ASSIGN(V4L2Metadata);
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_V4L2_METADATA_H_
