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

#ifndef V4L2_CAMERA_HAL_METADATA_ENUM_CONVERTER_H_
#define V4L2_CAMERA_HAL_METADATA_ENUM_CONVERTER_H_

#include <map>

#include <android-base/macros.h>
#include "converter_interface.h"

namespace v4l2_camera_hal {

// An EnumConverter converts between enum values.
class EnumConverter : public ConverterInterface<uint8_t, int32_t> {
 public:
  EnumConverter(const std::multimap<int32_t, uint8_t>& v4l2_to_metadata);

  virtual int MetadataToV4L2(uint8_t value, int32_t* conversion) override;
  virtual int V4L2ToMetadata(int32_t value, uint8_t* conversion) override;

 private:
  const std::multimap<int32_t, uint8_t> v4l2_to_metadata_;

  DISALLOW_COPY_AND_ASSIGN(EnumConverter);
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_ENUM_CONVERTER_H_
