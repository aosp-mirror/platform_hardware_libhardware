/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ANDROID_INPUT_DEVICE_H_
#define ANDROID_INPUT_DEVICE_H_

#include <memory>

#include <utils/Timers.h>

#include "InputHub.h"

namespace android {

/**
 * InputDeviceInterface represents an input device in the HAL. It processes
 * input events before passing them to the input host.
 */
class InputDeviceInterface {
public:
    virtual void processInput(InputEvent& event, nsecs_t currentTime) = 0;

protected:
    InputDeviceInterface() = default;
    virtual ~InputDeviceInterface() = default;
};

/**
 * EvdevDevice is an input device backed by a Linux evdev node.
 */
class EvdevDevice : public InputDeviceInterface {
public:
    explicit EvdevDevice(std::shared_ptr<InputDeviceNode> node);
    virtual ~EvdevDevice() override = default;

    virtual void processInput(InputEvent& event, nsecs_t currentTime) override;

private:
    std::shared_ptr<InputDeviceNode> mDeviceNode;

    int32_t mOverrideSec = 0;
    int32_t mOverrideUsec = 0;
};

}  // namespace android

#endif  // ANDROID_INPUT_DEVICE_H_
