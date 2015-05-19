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

#ifndef ANDROID_INPUT_DEVICE_MANAGER_H_
#define ANDROID_INPUT_DEVICE_MANAGER_H_

#include <memory>
#include <unordered_map>

#include <utils/Timers.h>

#include "InputHub.h"

namespace android {

class InputDeviceInterface;
class InputHostInterface;

/**
 * InputDeviceManager keeps the mapping of InputDeviceNodes to
 * InputDeviceInterfaces and handles the callbacks from the InputHub, delegating
 * them to the appropriate InputDeviceInterface.
 */
class InputDeviceManager : public InputCallbackInterface {
public:
    explicit InputDeviceManager(InputHostInterface* host) :
        mHost(host) {}
    virtual ~InputDeviceManager() override = default;

    virtual void onInputEvent(const std::shared_ptr<InputDeviceNode>& node, InputEvent& event,
            nsecs_t event_time) override;
    virtual void onDeviceAdded(const std::shared_ptr<InputDeviceNode>& node) override;
    virtual void onDeviceRemoved(const std::shared_ptr<InputDeviceNode>& node) override;

private:
    InputHostInterface* mHost;

    template<class T, class U>
    using DeviceMap = std::unordered_map<std::shared_ptr<T>, std::shared_ptr<U>>;

    DeviceMap<InputDeviceNode, InputDeviceInterface> mDevices;
};

}  // namespace android

#endif  // ANDROID_INPUT_DEVICE_MANAGER_H_
