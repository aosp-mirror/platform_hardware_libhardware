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

#ifndef ANDROID_AUDIO_HARDWARE_INTERFACE_H
#define ANDROID_AUDIO_HARDWARE_INTERFACE_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/Vector.h>
#include <utils/String16.h>

#include <media/IAudioFlinger.h>
#include "media/AudioSystem.h"


namespace android {

// ----------------------------------------------------------------------------

/**
 * This is the abstraction interface for the audio output hardware. It provides 
 * information about various properties of the audio output hardware driver.
 */
class AudioStreamOut {
public:
    virtual             ~AudioStreamOut() = 0;
    
    /** return audio sampling rate in hz - eg. 44100 */
    virtual uint32_t    sampleRate() const = 0;
    
    /** returns size of output buffer - eg. 4800 */
    virtual size_t      bufferSize() const = 0;
    
    /** 
     * return number of output audio channels. 
     * Acceptable values are 1 (mono) or 2 (stereo) 
     */
    virtual int         channelCount() const = 0;
    
    /**
     * return audio format in 8bit or 16bit PCM format - 
     * eg. AudioSystem:PCM_16_BIT 
     */
    virtual int         format() const = 0;
    
    /** 
     * This method can be used in situations where audio mixing is done in the
     * hardware. It is a direct interface to the hardware to set the volume as
     * as opposed to controlling this via the framework. It could be multiple 
     * PCM outputs or hardware accelerated codecs such as MP3 or AAC
     */
    virtual status_t    setVolume(float volume) = 0;
    
    /** write audio buffer to driver. Returns number of bytes written */
    virtual ssize_t     write(const void* buffer, size_t bytes) = 0;

    /** dump the state of the audio output device */
    virtual status_t dump(int fd, const Vector<String16>& args) = 0; 
};

/**
 * This is the abstraction interface for the audio input hardware. It defines 
 * the various properties of the audio hardware input driver.  
 */
class AudioStreamIn {
public:
    virtual             ~AudioStreamIn() = 0;
    
    /** return the input buffer size allowed by audio driver */
    virtual size_t      bufferSize() const = 0;

    /** return the number of audio input channels */
    virtual int         channelCount() const = 0;
    
    /** 
     * return audio format in 8bit or 16bit PCM format - 
     * eg. AudioSystem:PCM_16_BIT 
     */
    virtual int         format() const = 0;

    /** set the input gain for the audio driver. This method is for
     *  for future use */
    virtual status_t    setGain(float gain) = 0;
    
    /** read audio buffer in from audio driver */
    virtual ssize_t     read(void* buffer, ssize_t bytes) = 0;

    /** dump the state of the audio input device */
    virtual status_t dump(int fd, const Vector<String16>& args) = 0;
};

/** 
 * This defines the interface to the audio hardware abstraction layer. It 
 * supports setting and getting parameters, selecting audio routing paths and 
 * defining input and output streams.
 * 
 * AudioFlinger initializes the audio hardware and immediately opens an output 
 * stream. Audio routing can be set to output to handset, speaker, bluetooth or 
 * headset. 
 *
 * The audio input stream is initialized when AudioFlinger is called to carry 
 * out a record operation. 
 */
class AudioHardwareInterface
{
public:
                        AudioHardwareInterface();
    virtual             ~AudioHardwareInterface() { }

    /** 
     * check to see if the audio hardware interface has been initialized. 
     * return status based on values defined in include/utils/Errors.h 
     */
    virtual status_t    initCheck() = 0;

    /** 
     * put the audio hardware into standby mode to conserve power. Returns 
     * status based on include/utils/Errors.h 
     */
    virtual status_t    standby() = 0;

    /** set the audio volume of a voice call. Range is between 0.0 and 1.0 */
    virtual status_t    setVoiceVolume(float volume) = 0;
    
    /** 
     * set the audio volume for all audio activities other than voice call. 
     * Range between 0.0 and 1.0. IF any value other than NO_ERROR is returned,
     * the software mixer will emulate this capability.
     */
    virtual status_t    setMasterVolume(float volume) = 0;

    /**  
     * Audio routing methods. Routes defined in include/hardware/AudioSystem.h. 
     * Audio routes can be (ROUTE_EARPIECE | ROUTE_SPEAKER | ROUTE_BLUETOOTH 
     *                    | ROUTE_HEADSET)
     * 
     * setRouting sets the routes for a mode. This is called at startup. It is
     * also called when a new device is connected, such as a wired headset is 
     * plugged in or a Bluetooth headset is paired
     */
    virtual status_t    setRouting(int mode, uint32_t routes);
    
    virtual status_t    getRouting(int mode, uint32_t* routes);
    
    /**
     * setMode is called when the audio mode changes. NORMAL mode is for
     * standard audio playback, RINGTONE when a ringtone is playing and IN_CALL
     * when a call is in progress.
     */
    virtual status_t    setMode(int mode);
    virtual status_t    getMode(int* mode);

    // mic mute
    virtual status_t    setMicMute(bool state) = 0;
    virtual status_t    getMicMute(bool* state) = 0;

    // Temporary interface, do not use
    // TODO: Replace with a more generic key:value get/set mechanism
    virtual status_t    setParameter(const char* key, const char* value);

    /** This method creates and opens the audio hardware output stream */
    virtual AudioStreamOut* openOutputStream(
                                int format=0,
                                int channelCount=0,
                                uint32_t sampleRate=0) = 0;

    /** This method creates and opens the audio hardware input stream */
    virtual AudioStreamIn* openInputStream(
                                int format,
                                int channelCount,
                                uint32_t sampleRate) = 0;
    
    /**This method dumps the state of the audio hardware */
    virtual status_t dumpState(int fd, const Vector<String16>& args);

    static AudioHardwareInterface* create();

protected:
    /**
     * doRouting actually initiates the routing to occur. A call to setRouting 
     * or setMode may result in a routing change. The generic logic will call 
     * doRouting when required. If the device has any special requirements these
     * methods can be overriden.
     */
    virtual status_t    doRouting() = 0;
    
    virtual status_t dump(int fd, const Vector<String16>& args) = 0;

    int             mMode;
    uint32_t        mRoutes[AudioSystem::NUM_MODES];
};

// ----------------------------------------------------------------------------

extern "C" AudioHardwareInterface* createAudioHardware(void);

}; // namespace android

#endif // ANDROID_AUDIO_HARDWARE_INTERFACE_H
