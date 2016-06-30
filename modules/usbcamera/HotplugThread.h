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
#ifndef HOTPLUG_THREAD_H_
#define HOTPLUG_THREAD_H_

#include <utils/Thread.h>
#include "CameraHAL.h"

namespace usb_camera_hal {
/**
 * Thread for managing usb camera hotplug. It does below:
 * 1. Monitor camera hotplug status, and notify the status changes by calling
 *    module callback methods.
 * 2. When camera is plugged, create camera device instance, initialize the camera
 *    static info. When camera is unplugged, destroy the camera device instance and
 *    static metadata. As an optimization option, the camera device instance (including
 *    the static info) could be cached when the same camera plugged/unplugged multiple
 *    times.
 */

class CameraHAL;

class HotplugThread : public android::Thread {

    public:
        explicit HotplugThread(CameraHAL *hal);
        ~HotplugThread();

        // Override below two methods for proper cleanup.
        virtual bool threadLoop();
        virtual void requestExit();

    private:
        CameraHAL *mModule;
};

} // namespace usb_camera_hal

#endif // HOTPLUG_THREAD_H_
