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

#ifndef ANDROID_AUDIO_HARDWARE_BASE_H
#define ANDROID_AUDIO_HARDWARE_BASE_H

#include "hardware/AudioHardwareInterface.h"


namespace android {

// ----------------------------------------------------------------------------

/** 
 * AudioHardwareBase is a convenient base class used for implementing the 
 * AudioHardwareInterface interface.
 */
class AudioHardwareBase : public AudioHardwareInterface
{
public:
                        AudioHardwareBase();
    virtual             ~AudioHardwareBase() { }

    /**  
     * Audio routing methods. Routes defined in include/hardware/AudioSystem.h. 
     * Audio routes can be (ROUTE_EARPIECE | ROUTE_SPEAKER | ROUTE_BLUETOOTH 
     *                    | ROUTE_HEADSET)
     * 
     * setRouting sets the routes for a mode. This is called at startup. It is
     * also called when a new device is connected, such as a wired headset is 
     * plugged in or a Bluetooth headset is paired.
     */
    virtual status_t    setRouting(int mode, uint32_t routes);
    
    virtual status_t    getRouting(int mode, uint32_t* routes);
    
    /**
     * setMode is called when the audio mode changes. NORMAL mode is for
     * standard audio playback, RINGTONE when a ringtone is playing, and IN_CALL
     * when a call is in progress.
     */
    virtual status_t    setMode(int mode);
    virtual status_t    getMode(int* mode);

    // Temporary interface, do not use
    // TODO: Replace with a more generic key:value get/set mechanism
    virtual status_t    setParameter(const char* key, const char* value);
    
    /**This method dumps the state of the audio hardware */
    virtual status_t dumpState(int fd, const Vector<String16>& args);

protected:
    int             mMode;
    uint32_t        mRoutes[AudioSystem::NUM_MODES];
};

}; // namespace android

#endif // ANDROID_AUDIO_HARDWARE_BASE_H
