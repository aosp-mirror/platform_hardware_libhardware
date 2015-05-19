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

#ifndef ANDROID_INPUT_MAPPER_H_
#define ANDROID_INPUT_MAPPER_H_

struct input_device_handle;

namespace android {

class InputDeviceNode;
class InputReport;
class InputReportDefinition;
struct InputEvent;
using InputDeviceHandle = struct input_device_handle;

/**
 * An InputMapper processes raw evdev input events and combines them into
 * Android input HAL reports. A given InputMapper will focus on a particular
 * type of input, like key presses or touch events. A single InputDevice may
 * have multiple InputMappers, corresponding to the different types of inputs it
 * supports.
 */
class InputMapper {
public:
    InputMapper() = default;
    virtual ~InputMapper() {}

    /**
     * If the mapper supports input events from the InputDevice,
     * configureInputReport will populate the InputReportDefinition and return
     * true. If input is not supported, false is returned, and the InputDevice
     * may free or re-use the InputReportDefinition.
     */
    virtual bool configureInputReport(InputDeviceNode* devNode, InputReportDefinition* report) {
        return false;
    }

    /**
     * If the mapper supports output events from the InputDevice,
     * configureOutputReport will populate the InputReportDefinition and return
     * true. If output is not supported, false is returned, and the InputDevice
     * may free or re-use the InputReportDefinition.
     */
    virtual bool configureOutputReport(InputDeviceNode* devNode, InputReportDefinition* report) {
        return false;
    }

    // Set the InputDeviceHandle after registering the device with the host.
    virtual void setDeviceHandle(InputDeviceHandle* handle) { mDeviceHandle = handle; }
    // Process the InputEvent.
    virtual void process(const InputEvent& event) = 0;

protected:
    virtual void setInputReportDefinition(InputReportDefinition* reportDef) final {
        mInputReportDef = reportDef;
    }
    virtual void setOutputReportDefinition(InputReportDefinition* reportDef) final {
        mOutputReportDef = reportDef;
    }
    virtual InputReportDefinition* getInputReportDefinition() final { return mInputReportDef; }
    virtual InputReportDefinition* getOutputReportDefinition() final { return mOutputReportDef; }
    virtual InputDeviceHandle* getDeviceHandle() final { return mDeviceHandle; }
    virtual InputReport* getInputReport() final;

private:
    InputReportDefinition* mInputReportDef = nullptr;
    InputReportDefinition* mOutputReportDef = nullptr;
    InputDeviceHandle* mDeviceHandle = nullptr;
    InputReport* mReport = nullptr;
};

}  // namespace android

#endif  // ANDROID_INPUT_MAPPER_H_
