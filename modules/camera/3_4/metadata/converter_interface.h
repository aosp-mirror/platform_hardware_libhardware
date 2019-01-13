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

#ifndef V4L2_CAMERA_HAL_METADATA_CONVERTER_INTERFACE_H_
#define V4L2_CAMERA_HAL_METADATA_CONVERTER_INTERFACE_H_

namespace v4l2_camera_hal {

// A ConverterInterface converts metadata values to V4L2 values vice-versa.
template <typename TMetadata, typename TV4L2>
class ConverterInterface {
 public:
  virtual ~ConverterInterface(){};

  // Convert.
  virtual int MetadataToV4L2(TMetadata value, TV4L2* conversion) = 0;
  virtual int V4L2ToMetadata(TV4L2 value, TMetadata* conversion) = 0;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_CONVERTER_INTERFACE_H_
