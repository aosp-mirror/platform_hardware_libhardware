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

// Mock for default option delegates.

#ifndef V4L2_CAMERA_HAL_METADATA_DEFAULT_OPTION_DELEGATE_MOCK_H_
#define V4L2_CAMERA_HAL_METADATA_DEFAULT_OPTION_DELEGATE_MOCK_H_

#include "default_option_delegate.h"

#include <gmock/gmock.h>

namespace v4l2_camera_hal {

template <typename T>
class DefaultOptionDelegateMock : public DefaultOptionDelegate<T> {
 public:
  DefaultOptionDelegateMock() : DefaultOptionDelegate<T>({}){};
  MOCK_METHOD2_T(DefaultValueForTemplate, bool(int, T*));
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_DEFAULT_OPTION_DELEGATE_MOCK_H_
