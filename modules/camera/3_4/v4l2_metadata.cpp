/*
 * Copyright 2016 The Android Open Source Project
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

#include "v4l2_metadata.h"

#include <camera/CameraMetadata.h>

#include "common.h"

namespace v4l2_camera_hal {

V4L2Metadata::V4L2Metadata(V4L2Wrapper* device) : device_(device) {
  HAL_LOG_ENTER();

  // TODO(b/30140438): Add all metadata components used by V4L2Camera here.
  // Currently these are all the fixed properties. Will add the other properties
  // as more PartialMetadata subclasses get implemented.
}

V4L2Metadata::~V4L2Metadata() { HAL_LOG_ENTER(); }

}  // namespace v4l2_camera_hal
