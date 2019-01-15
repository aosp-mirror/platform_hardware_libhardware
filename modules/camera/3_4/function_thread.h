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

#ifndef V4L2_CAMERA_HAL_FUNCTION_THREAD_H_
#define V4L2_CAMERA_HAL_FUNCTION_THREAD_H_

#include <functional>

#include <utils/Thread.h>

namespace v4l2_camera_hal {

class FunctionThread : public android::Thread {
 public:
  FunctionThread(std::function<bool()> function) : function_(function){};

 private:
  bool threadLoop() override {
    bool result = function_();
    return result;
  };

  std::function<bool()> function_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_FUNCTION_THREAD_H_
