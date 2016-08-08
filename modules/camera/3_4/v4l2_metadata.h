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

#include <map>
#include <memory>

#include <hardware/camera3.h>

#include "common.h"
#include "metadata/control.h"
#include "metadata/metadata.h"
#include "v4l2_wrapper.h"

namespace v4l2_camera_hal {
class V4L2Metadata : public Metadata {
 public:
  V4L2Metadata(std::shared_ptr<V4L2Wrapper> device);
  virtual ~V4L2Metadata();

 private:
  // Attempt to construct and add an enum control. If construction fails,
  // use an IgnoredControl with only the default value instead.
  void AddEnumControlOrDefault(
      int v4l2_control, int32_t control_tag, int32_t options_tag,
      const std::map<int32_t, uint8_t>& v4l2_to_metadata,
      uint8_t default_value);

  // Access to the device.
  std::shared_ptr<V4L2Wrapper> device_;

  DISALLOW_COPY_AND_ASSIGN(V4L2Metadata);
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_V4L2_METADATA_H_
