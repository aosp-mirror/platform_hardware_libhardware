/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ANDROID_SENSORHAL_EXT_CONNECTION_DETECTOR_H
#define ANDROID_SENSORHAL_EXT_CONNECTION_DETECTOR_H

#include "BaseDynamicSensorDaemon.h"
#include <utils/Thread.h>
#include <utils/Looper.h>

#include <regex>

#include <poll.h>

namespace android {
namespace SensorHalExt {

// Abstraction of connection detector: an entity that calls
// BaseDynamicSensorDaemon::onConnectionChange() when necessary.
class ConnectionDetector : virtual public RefBase {
public:
    ConnectionDetector(BaseDynamicSensorDaemon *d) : mDaemon(d) { }
    virtual ~ConnectionDetector() = default;
protected:
    BaseDynamicSensorDaemon* mDaemon;
};

// Open a socket that listen to localhost:port and notify sensor daemon of connection and
// disconnection event when socket is connected or disconnected, respectively. Only one concurrent
// client is accepted.
class SocketConnectionDetector : public ConnectionDetector, public Thread {
public:
    SocketConnectionDetector(BaseDynamicSensorDaemon *d, int port);
    virtual ~SocketConnectionDetector();
private:
    // implement virtual of Thread
    virtual bool threadLoop();
    int waitForConnection();
    static void waitForDisconnection(int connFd);

    int mListenFd;
    std::string mDevice;
};

// Detect file change under path and notify sensor daemon of connection and disconnection event when
// file is created in or removed from the directory, respectively.
class FileConnectionDetector : public ConnectionDetector, public Thread {
public:
    FileConnectionDetector(
            BaseDynamicSensorDaemon *d, const std::string &path, const std::string &regex);
    virtual ~FileConnectionDetector();
private:
    static constexpr int POLL_IDENT = 1;
    // implement virtual of Thread
    virtual bool threadLoop();

    bool matches(const std::string &name) const;
    void processExistingFiles() const;
    void handleInotifyData(ssize_t len, const char *data);
    bool readInotifyData();
    std::string getFullName(const std::string name) const;

    std::string mPath;
    std::regex mRegex;
    sp<Looper> mLooper;
    int mInotifyFd;
};

} // namespace SensorHalExt
} // namespace android

#endif // ANDROID_SENSORHAL_EXT_DYNAMIC_SENSOR_DAEMON_H
