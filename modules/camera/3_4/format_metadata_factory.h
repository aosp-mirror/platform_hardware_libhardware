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

#ifndef V4L2_CAMERA_HAL_FORMAT_METADATA_FACTORY_H_
#define V4L2_CAMERA_HAL_FORMAT_METADATA_FACTORY_H_

#include <iterator>
#include <memory>

#include "metadata/metadata_common.h"
#include "v4l2_wrapper.h"

namespace v4l2_camera_hal {

// A factory method to construct all the format-related
// partial metadata for a V4L2 device.
int AddFormatComponents(
    std::shared_ptr<V4L2Wrapper> device,
    std::insert_iterator<PartialMetadataSet> insertion_point);

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_FORMAT_METADATA_FACTORY_H_
