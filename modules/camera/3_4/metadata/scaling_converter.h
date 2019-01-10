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

#ifndef V4L2_CAMERA_HAL_METADATA_SCALING_CONVERTER_H_
#define V4L2_CAMERA_HAL_METADATA_SCALING_CONVERTER_H_

#include "common.h"
#include "converter_interface.h"

namespace v4l2_camera_hal {

// An ScalingConverter scales values up or down.
template <typename TMetadata, typename TV4L2>
class ScalingConverter : public ConverterInterface<TMetadata, TV4L2> {
 public:
  ScalingConverter(TMetadata v4l2_to_metadata_numerator,
                   TMetadata v4l2_to_metadata_denominator);

  virtual int MetadataToV4L2(TMetadata value, TV4L2* conversion) override;
  virtual int V4L2ToMetadata(TV4L2 value, TMetadata* conversion) override;

 private:
  const TMetadata v4l2_to_metadata_numerator_;
  const TMetadata v4l2_to_metadata_denominator_;

  DISALLOW_COPY_AND_ASSIGN(ScalingConverter);
};

// -----------------------------------------------------------------------------

template <typename TMetadata, typename TV4L2>
ScalingConverter<TMetadata, TV4L2>::ScalingConverter(
    TMetadata v4l2_to_metadata_numerator,
    TMetadata v4l2_to_metadata_denominator)
    : v4l2_to_metadata_numerator_(v4l2_to_metadata_numerator),
      v4l2_to_metadata_denominator_(v4l2_to_metadata_denominator) {
  HAL_LOG_ENTER();
}

template <typename TMetadata, typename TV4L2>
int ScalingConverter<TMetadata, TV4L2>::MetadataToV4L2(TMetadata value,
                                                       TV4L2* conversion) {
  HAL_LOG_ENTER();

  *conversion = static_cast<TV4L2>(value * v4l2_to_metadata_denominator_ /
                                   v4l2_to_metadata_numerator_);
  return 0;
}

template <typename TMetadata, typename TV4L2>
int ScalingConverter<TMetadata, TV4L2>::V4L2ToMetadata(TV4L2 value,
                                                       TMetadata* conversion) {
  HAL_LOG_ENTER();

  *conversion = static_cast<TMetadata>(value) * v4l2_to_metadata_numerator_ /
                v4l2_to_metadata_denominator_;
  return 0;
}

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_SCALING_CONVERTER_H_
