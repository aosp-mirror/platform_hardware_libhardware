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

#ifndef V4L2_CAMERA_HAL_METADATA_MENU_CONTROL_OPTIONS_H_
#define V4L2_CAMERA_HAL_METADATA_MENU_CONTROL_OPTIONS_H_

#include <cerrno>

#include "common.h"
#include "control_options_interface.h"
#include "default_option_delegate.h"

namespace v4l2_camera_hal {

// MenuControlOptions offer a fixed list of acceptable values.
template <typename T>
class MenuControlOptions : public ControlOptionsInterface<T> {
 public:
  // |options| must be non-empty.
  MenuControlOptions(std::vector<T> options,
                     std::shared_ptr<DefaultOptionDelegate<T>> defaults)
      : options_(options), defaults_(defaults){};
  MenuControlOptions(std::vector<T> options, std::map<int, T> defaults)
      : options_(options),
        defaults_(std::make_shared<DefaultOptionDelegate<T>>(defaults)){};

  virtual std::vector<T> MetadataRepresentation() override { return options_; };
  virtual bool IsSupported(const T& option) override {
    return (std::find(options_.begin(), options_.end(), option) !=
            options_.end());
  };
  virtual int DefaultValueForTemplate(int template_type,
                                      T* default_value) override {
    // Default to the first option.
    if (options_.empty()) {
      HAL_LOGE("Can't get default value, options are empty.");
      return -ENODEV;
    }

    // Try to get it from the defaults delegate.
    if (defaults_->DefaultValueForTemplate(template_type, default_value) &&
        IsSupported(*default_value)) {
      return 0;
    }

    // Fall back to the first available.
    *default_value = options_[0];
    return 0;
  };

 private:
  std::vector<T> options_;
  std::shared_ptr<DefaultOptionDelegate<T>> defaults_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_MENU_CONTROL_OPTIONS_H_
