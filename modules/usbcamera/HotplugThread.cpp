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

//#define LOG_NDEBUG 0
#define LOG_TAG "HotplugThread"

#include <log/log.h>

#include "HotplugThread.h"

namespace usb_camera_hal {

HotplugThread::HotplugThread(CameraHAL *hal)
    : mModule(hal) {

}

HotplugThread::~HotplugThread() {

}

void HotplugThread::requestExit() {
    // Call parent to set up shutdown
    Thread::requestExit();

    // Cleanup other states?
}

bool HotplugThread::threadLoop() {
    (void)mModule;  // silence warning about unused member.

    /**
     * Check camera connection status change, if connected, do below:
     * 1. Create camera device, add to mCameras.
     * 2. Init static info (mCameras[id]->initStaticInfo())
     * 3. Notify on_status_change callback
     *
     * If unconnected, similarly, do below:
     * 1. Destroy camera device and remove it from mCameras.
     * 2. Notify on_status_change callback
     *
     * DO NOT have a tight polling loop here, to avoid excessive CPU utilization.
     */

    return true;
}

} // namespace usb_camera_hal
