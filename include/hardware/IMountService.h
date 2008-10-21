/*
 * Copyright (C) 2007 The Android Open Source Project
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

//
#ifndef ANDROID_HARDWARE_IMOUNTSERVICE_H
#define ANDROID_HARDWARE_IMOUNTSERVICE_H

#include <utils/IInterface.h>
#include <utils/String16.h>

namespace android {

// ----------------------------------------------------------------------

class IMountService : public IInterface
{
public:
    DECLARE_META_INTERFACE(MountService);

    /**
     * Is mass storage support enabled?
     */
    virtual bool getMassStorageEnabled() = 0;

    /**
     * Enable or disable mass storage support.
     */
    virtual void setMassStorageEnabled(bool enabled) = 0;

    /**
     * Is mass storage connected?
     */
    virtual bool getMassStorageConnected() = 0;
    
    /**
     * Mount external storage at given mount point.
     */
    virtual void mountMedia(String16 mountPoint) = 0;

    /**
     * Safely unmount external storage at given mount point.
     */
    virtual void unmountMedia(String16 mountPoint) = 0;
};

// ----------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_HARDWARE_IMOUNTSERVICE_H
