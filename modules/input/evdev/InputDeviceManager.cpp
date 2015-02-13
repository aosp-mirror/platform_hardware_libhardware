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

#define LOG_TAG "InputDeviceManager"
//#define LOG_NDEBUG 0

#include <utils/Log.h>

#include "InputDevice.h"
#include "InputDeviceManager.h"

namespace android {

void InputDeviceManager::onInputEvent(std::shared_ptr<InputDeviceNode> node, InputEvent& event,
        nsecs_t event_time) {
    if (mDevices[node] == nullptr) {
        ALOGE("got input event for unknown node %s", node->getPath().c_str());
        return;
    }
    mDevices[node]->processInput(event, event_time);
}

void InputDeviceManager::onDeviceAdded(std::shared_ptr<InputDeviceNode> node) {
    mDevices[node] = std::make_shared<EvdevDevice>(node);
}

void InputDeviceManager::onDeviceRemoved(std::shared_ptr<InputDeviceNode> node) {
    if (mDevices[node] == nullptr) {
        ALOGE("could not remove unknown node %s", node->getPath().c_str());
        return;
    }
    // TODO: tell the InputDevice and InputDeviceNode that they are being
    // removed so they can run any cleanup.
    mDevices.erase(node);
}

}  // namespace android
