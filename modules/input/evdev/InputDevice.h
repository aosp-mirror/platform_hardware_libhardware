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
#include <vector>

#include <utils/Timers.h>

#include "InputMapper.h"

struct input_device_handle;
struct input_device_identifier;

namespace android {

class InputDeviceDefinition;
class InputDeviceNode;
class InputHostInterface;
struct InputEvent;
using InputDeviceHandle = struct input_device_handle;
using InputDeviceIdentifier = struct input_device_identifier;

/**
 * InputDeviceInterface represents an input device in the HAL. It processes
 * input events before passing them to the input host.
 */
class InputDeviceInterface {
public:
    virtual void processInput(InputEvent& event, nsecs_t currentTime) = 0;

    virtual uint32_t getInputClasses() = 0;
protected:
    InputDeviceInterface() = default;
    virtual ~InputDeviceInterface() = default;
};

/**
 * EvdevDevice is an input device backed by a Linux evdev node.
 */
class EvdevDevice : public InputDeviceInterface {
public:
    EvdevDevice(InputHostInterface* host, const std::shared_ptr<InputDeviceNode>& node);
    virtual ~EvdevDevice() override = default;

    virtual void processInput(InputEvent& event, nsecs_t currentTime) override;

    virtual uint32_t getInputClasses() override { return mClasses; }
private:
    void createMappers();
    void configureDevice();

    InputHostInterface* mHost = nullptr;
    std::shared_ptr<InputDeviceNode> mDeviceNode;
    InputDeviceIdentifier* mInputId = nullptr;
    InputDeviceDefinition* mDeviceDefinition = nullptr;
    InputDeviceHandle* mDeviceHandle = nullptr;
    std::vector<std::unique_ptr<InputMapper>> mMappers;
    uint32_t mClasses = 0;

    int32_t mOverrideSec = 0;
    int32_t mOverrideUsec = 0;
};

/* Input device classes. */
enum {
    /* The input device is a keyboard or has buttons. */
    INPUT_DEVICE_CLASS_KEYBOARD      = 0x00000001,

    /* The input device is an alpha-numeric keyboard (not just a dial pad). */
    INPUT_DEVICE_CLASS_ALPHAKEY      = 0x00000002,

    /* The input device is a touchscreen or a touchpad (either single-touch or multi-touch). */
    INPUT_DEVICE_CLASS_TOUCH         = 0x00000004,

    /* The input device is a cursor device such as a trackball or mouse. */
    INPUT_DEVICE_CLASS_CURSOR        = 0x00000008,

    /* The input device is a multi-touch touchscreen. */
    INPUT_DEVICE_CLASS_TOUCH_MT      = 0x00000010,

    /* The input device is a directional pad (implies keyboard, has DPAD keys). */
    INPUT_DEVICE_CLASS_DPAD          = 0x00000020,

    /* The input device is a gamepad (implies keyboard, has BUTTON keys). */
    INPUT_DEVICE_CLASS_GAMEPAD       = 0x00000040,

    /* The input device has switches. */
    INPUT_DEVICE_CLASS_SWITCH        = 0x00000080,

    /* The input device is a joystick (implies gamepad, has joystick absolute axes). */
    INPUT_DEVICE_CLASS_JOYSTICK      = 0x00000100,

    /* The input device has a vibrator (supports FF_RUMBLE). */
    INPUT_DEVICE_CLASS_VIBRATOR      = 0x00000200,

    /* The input device has a microphone. */
    // TODO: remove this and let the host take care of it
    INPUT_DEVICE_CLASS_MIC           = 0x00000400,

    /* The input device is an external stylus (has data we want to fuse with touch data). */
    INPUT_DEVICE_CLASS_EXTERNAL_STYLUS = 0x00000800,

    /* The input device is virtual (not a real device, not part of UI configuration). */
    /* not used - INPUT_DEVICE_CLASS_VIRTUAL       = 0x40000000, */

    /* The input device is external (not built-in). */
    // TODO: remove this and let the host take care of it?
    INPUT_DEVICE_CLASS_EXTERNAL      = 0x80000000,
};

}  // namespace android

#endif  // ANDROID_INPUT_DEVICE_H_
