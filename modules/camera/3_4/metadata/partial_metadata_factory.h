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

#ifndef V4L2_CAMERA_HAL_METADATA_CONTROL_FACTORY_H_
#define V4L2_CAMERA_HAL_METADATA_CONTROL_FACTORY_H_

#include "common.h"
#include "control.h"
#include "menu_control_options.h"
#include "no_effect_control_delegate.h"
#include "ranged_converter.h"
#include "slider_control_options.h"
#include "state.h"
#include "tagged_control_delegate.h"
#include "tagged_control_options.h"
#include "v4l2_control_delegate.h"

namespace v4l2_camera_hal {

enum class ControlType { kMenu, kSlider };

// Static functions to create partial metadata. Nullptr is returned on failures.

// FixedState: A state that doesn't change.
template <typename T>
static std::unique_ptr<State<T>> FixedState(int32_t tag, T value);

// NoEffectOptionlessControl: A control that accepts any value,
// and has no effect. A default value is given.
template <typename T>
static std::unique_ptr<Control<T>> NoEffectOptionlessControl(
    int32_t delegate_tag, T default_value);

// NoEffectMenuControl: Some menu options, but they have no effect.
template <typename T>
static std::unique_ptr<Control<T>> NoEffectMenuControl(
    int32_t delegate_tag,
    int32_t options_tag,
    const std::vector<T>& options,
    std::map<int, T> default_values = {});

// NoEffectSliderControl: A slider of options, but they have no effect.
template <typename T>
static std::unique_ptr<Control<T>> NoEffectSliderControl(
    int32_t delegate_tag,
    int32_t options_tag,
    T min,
    T max,
    std::map<int, T> default_values = {});

// NoEffectControl: A control with no effect and only a single allowable
// value. Chooses an appropriate ControlOptionsInterface depending on type.
template <typename T>
static std::unique_ptr<Control<T>> NoEffectControl(
    ControlType type,
    int32_t delegate_tag,
    int32_t options_tag,
    T value,
    std::map<int, T> default_values = {});

// V4L2Control: A control corresponding to a V4L2 control.
template <typename T>
static std::unique_ptr<Control<T>> V4L2Control(
    ControlType type,
    int32_t delegate_tag,
    int32_t options_tag,
    std::shared_ptr<V4L2Wrapper> device,
    int control_id,
    std::shared_ptr<ConverterInterface<T, int32_t>> converter,
    std::map<int, T> default_values = {});

// V4L2ControlOrDefault: Like V4L2Control, but if the V4L2Control fails to
// initialize for some reason, this method will fall back to NoEffectControl
// with an initial value defined by |fallback_default|.
template <typename T>
static std::unique_ptr<Control<T>> V4L2ControlOrDefault(
    ControlType type,
    int32_t delegate_tag,
    int32_t options_tag,
    std::shared_ptr<V4L2Wrapper> device,
    int control_id,
    std::shared_ptr<ConverterInterface<T, int32_t>> converter,
    T fallback_default,
    std::map<int, T> default_values = {});

// -----------------------------------------------------------------------------

template <typename T>
std::unique_ptr<State<T>> FixedState(int32_t tag, T value) {
  HAL_LOG_ENTER();

  // Take advantage of ControlDelegate inheriting from StateDelegate;
  // This will only expose GetValue, not SetValue, so the default will
  // always be returned.
  return std::make_unique<State<T>>(
      tag, std::make_unique<NoEffectControlDelegate<T>>(value));
}

template <typename T>
std::unique_ptr<Control<T>> NoEffectOptionlessControl(int32_t delegate_tag,
                                                      T default_value) {
  HAL_LOG_ENTER();

  return std::make_unique<Control<T>>(
      std::make_unique<TaggedControlDelegate<T>>(
          delegate_tag,
          std::make_unique<NoEffectControlDelegate<T>>(default_value)),
      nullptr);
}

template <typename T>
std::unique_ptr<Control<T>> NoEffectMenuControl(
    int32_t delegate_tag,
    int32_t options_tag,
    const std::vector<T>& options,
    std::map<int, T> default_values) {
  HAL_LOG_ENTER();

  if (options.empty()) {
    HAL_LOGE("At least one option must be provided.");
    return nullptr;
  }

  return std::make_unique<Control<T>>(
      std::make_unique<TaggedControlDelegate<T>>(
          delegate_tag,
          std::make_unique<NoEffectControlDelegate<T>>(options[0])),
      std::make_unique<TaggedControlOptions<T>>(
          options_tag,
          std::make_unique<MenuControlOptions<T>>(options, default_values)));
}

template <typename T>
std::unique_ptr<Control<T>> NoEffectSliderControl(
    int32_t delegate_tag,
    int32_t options_tag,
    T min,
    T max,
    std::map<int, T> default_values) {
  HAL_LOG_ENTER();

  return std::make_unique<Control<T>>(
      std::make_unique<TaggedControlDelegate<T>>(
          delegate_tag, std::make_unique<NoEffectControlDelegate<T>>(min)),
      std::make_unique<TaggedControlOptions<T>>(
          options_tag,
          std::make_unique<SliderControlOptions<T>>(min, max, default_values)));
}

template <typename T>
std::unique_ptr<Control<T>> NoEffectControl(ControlType type,
                                            int32_t delegate_tag,
                                            int32_t options_tag,
                                            T value,
                                            std::map<int, T> default_values) {
  HAL_LOG_ENTER();

  switch (type) {
    case ControlType::kMenu:
      return NoEffectMenuControl<T>(
          delegate_tag, options_tag, {value}, default_values);
    case ControlType::kSlider:
      return NoEffectSliderControl(
          delegate_tag, options_tag, value, value, default_values);
  }
}

template <typename T>
std::unique_ptr<Control<T>> V4L2Control(
    ControlType type,
    int32_t delegate_tag,
    int32_t options_tag,
    std::shared_ptr<V4L2Wrapper> device,
    int control_id,
    std::shared_ptr<ConverterInterface<T, int32_t>> converter,
    std::map<int, T> default_values) {
  HAL_LOG_ENTER();

  // Query the device.
  v4l2_query_ext_ctrl control_query;
  int res = device->QueryControl(control_id, &control_query);
  if (res) {
    HAL_LOGE("Failed to query control %d.", control_id);
    return nullptr;
  }

  int32_t control_min = static_cast<int32_t>(control_query.minimum);
  int32_t control_max = static_cast<int32_t>(control_query.maximum);
  int32_t control_step = static_cast<int32_t>(control_query.step);
  if (control_min > control_max) {
    HAL_LOGE("No acceptable values (min %d is greater than max %d).",
             control_min,
             control_max);
    return nullptr;
  }

  // Variables needed by the various switch statements.
  std::vector<T> options;
  T metadata_val;
  T metadata_min;
  T metadata_max;
  // Set up the result converter and result options based on type.
  std::shared_ptr<ConverterInterface<T, int32_t>> result_converter(converter);
  std::unique_ptr<ControlOptionsInterface<T>> result_options;
  switch (control_query.type) {
    case V4L2_CTRL_TYPE_BOOLEAN:
      if (type != ControlType::kMenu) {
        HAL_LOGE(
            "V4L2 control %d is of type %d, which isn't compatible with "
            "desired metadata control type %d",
            control_id,
            control_query.type,
            type);
        return nullptr;
      }

      // Convert each available option,
      // ignoring ones without a known conversion.
      for (int32_t i = control_min; i <= control_max; i += control_step) {
        res = converter->V4L2ToMetadata(i, &metadata_val);
        if (res == -EINVAL) {
          HAL_LOGV("V4L2 value %d for control %d has no metadata equivalent.",
                   i,
                   control_id);
          continue;
        } else if (res) {
          HAL_LOGE("Error converting value %d for control %d.", i, control_id);
          return nullptr;
        }
        options.push_back(metadata_val);
      }
      // Check to make sure there's at least one option.
      if (options.empty()) {
        HAL_LOGE("No valid options for control %d.", control_id);
        return nullptr;
      }

      result_options.reset(new MenuControlOptions<T>(options, default_values));
      // No converter changes necessary.
      break;
    case V4L2_CTRL_TYPE_INTEGER:
      if (type != ControlType::kSlider) {
        HAL_LOGE(
            "V4L2 control %d is of type %d, which isn't compatible with "
            "desired metadata control type %d",
            control_id,
            control_query.type,
            type);
        return nullptr;
      }

      // Upgrade to a range/step-clamping converter.
      result_converter.reset(new RangedConverter<T, int32_t>(
          converter, control_min, control_max, control_step));

      // Convert the min and max.
      res = result_converter->V4L2ToMetadata(control_min, &metadata_min);
      if (res) {
        HAL_LOGE(
            "Failed to convert V4L2 min value %d for control %d to metadata.",
            control_min,
            control_id);
        return nullptr;
      }
      res = result_converter->V4L2ToMetadata(control_max, &metadata_max);
      if (res) {
        HAL_LOGE(
            "Failed to convert V4L2 max value %d for control %d to metadata.",
            control_max,
            control_id);
        return nullptr;
      }
      result_options.reset(new SliderControlOptions<T>(
          metadata_min, metadata_max, default_values));
      break;
    default:
      HAL_LOGE("Control %d (%s) is of unsupported type %d",
               control_id,
               control_query.name,
               control_query.type);
      return nullptr;
  }

  // Construct the control.
  return std::make_unique<Control<T>>(
      std::make_unique<TaggedControlDelegate<T>>(
          delegate_tag,
          std::make_unique<V4L2ControlDelegate<T>>(
              device, control_id, result_converter)),
      std::make_unique<TaggedControlOptions<T>>(options_tag,
                                                std::move(result_options)));
}

template <typename T>
std::unique_ptr<Control<T>> V4L2ControlOrDefault(
    ControlType type,
    int32_t delegate_tag,
    int32_t options_tag,
    std::shared_ptr<V4L2Wrapper> device,
    int control_id,
    std::shared_ptr<ConverterInterface<T, int32_t>> converter,
    T fallback_default,
    std::map<int, T> default_values) {
  HAL_LOG_ENTER();

  std::unique_ptr<Control<T>> result = V4L2Control(type,
                                                   delegate_tag,
                                                   options_tag,
                                                   device,
                                                   control_id,
                                                   converter,
                                                   default_values);
  if (!result) {
    result = NoEffectControl(
        type, delegate_tag, options_tag, fallback_default, default_values);
  }
  return result;
}

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_CONTROL_FACTORY_H_
