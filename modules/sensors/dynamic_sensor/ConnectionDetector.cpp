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

#include "ConnectionDetector.h"

#include <utils/Log.h>

#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/inotify.h>
#include <sys/socket.h>

#include <sstream>

namespace android {
namespace SensorHalExt {

// SocketConnectionDetector functions
SocketConnectionDetector::SocketConnectionDetector(BaseDynamicSensorDaemon *d, int port)
        : ConnectionDetector(d), Thread(false /*canCallJava*/) {
    // initialize socket that accept connection to localhost:port
    mListenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (mListenFd < 0) {
        ALOGE("Cannot open socket");
        return;
    }

    struct sockaddr_in serverAddress = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {
            .s_addr = htonl(INADDR_LOOPBACK)
        }
    };

    ::bind(mListenFd, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (::listen(mListenFd, 0) != NO_ERROR) {
        ALOGE("Cannot listen to port %d", port);
        mListenFd = -1;
        return;
    }

    std::ostringstream s;
    s << "socket:" << port;
    mDevice = s.str();

    run("ddad_socket");
}

SocketConnectionDetector::~SocketConnectionDetector() {
    if (mListenFd >= 0) {
        requestExitAndWait();
    }
}

int SocketConnectionDetector::waitForConnection() {
    return ::accept(mListenFd, nullptr, nullptr);
}

void SocketConnectionDetector::waitForDisconnection(int connFd) {
    char buffer[16];
    while (::read(connFd, buffer, sizeof(buffer)) > 0) {
        // discard data but response something to denote thread alive
        ::write(connFd, ".", 1);
    }
    // read failure means disconnection
    ::close(connFd);
}

bool SocketConnectionDetector::threadLoop() {
    while (!Thread::exitPending()) {
        // block waiting for connection
        int connFd = waitForConnection();

        if (connFd < 0) {
            break;
        }

        ALOGV("Received connection, register dynamic accel sensor");
        mDaemon->onConnectionChange(mDevice, true);

        waitForDisconnection(connFd);
        ALOGV("Connection break, unregister dynamic accel sensor");
        mDaemon->onConnectionChange(mDevice, false);
    }
    mDaemon->onConnectionChange(mDevice, false);
    ALOGD("SocketConnectionDetector thread exited");
    return false;
}

// FileConnectionDetector functions
FileConnectionDetector::FileConnectionDetector (
        BaseDynamicSensorDaemon *d, const std::string &path, const std::string &regex)
            : ConnectionDetector(d), Thread(false /*callCallJava*/), mPath(path), mRegex(regex) {
    mInotifyFd = ::inotify_init1(IN_NONBLOCK);
    if (mInotifyFd < 0) {
        ALOGE("Cannot init inotify");
        return;
    }

    int wd = ::inotify_add_watch(mInotifyFd, path.c_str(), IN_CREATE | IN_DELETE | IN_MOVED_FROM);
    if (wd < 0) {
        ::close(mInotifyFd);
        mInotifyFd = -1;
        ALOGE("Cannot setup watch on dir %s", path.c_str());
        return;
    }

    mPollFd.fd = wd;
    mPollFd.events = POLLIN;

    run("ddad_file");
}

FileConnectionDetector::~FileConnectionDetector() {
    if (mInotifyFd) {
        requestExitAndWait();
        ::close(mInotifyFd);
    }
}

bool FileConnectionDetector::matches(const std::string &name) const {
    return std::regex_match(name, mRegex);
}

std::string FileConnectionDetector::getFullName(const std::string name) const {
    return mPath + name;
}

void FileConnectionDetector::processExistingFiles() const {
    auto dirp = ::opendir(mPath.c_str());
    struct dirent *dp;
    while ((dp = ::readdir(dirp)) != NULL) {
        const std::string name(dp->d_name);
        if (matches(name)) {
            mDaemon->onConnectionChange(getFullName(name), true /*connected*/);
        }
    }
    ::closedir(dirp);
}

bool FileConnectionDetector::threadLoop() {
    struct {
        struct inotify_event e;
        uint8_t padding[NAME_MAX + 1];
    } ev;

    processExistingFiles();

    while (!Thread::exitPending()) {
        int pollNum = ::poll(&mPollFd, 1, -1);
        if (pollNum == -1) {
           if (errno == EINTR)
               continue;
           ALOGE("inotify poll error: %s", ::strerror(errno));
        }

        if (pollNum > 0) {
            if (! (mPollFd.revents & POLLIN)) {
                continue;
            }

            /* Inotify events are available */
            while (true) {
                /* Read some events. */
                ssize_t len = ::read(mInotifyFd, &ev, sizeof ev);
                if (len == -1 && errno != EAGAIN) {
                    ALOGE("read error: %s", ::strerror(errno));
                    requestExit();
                    break;
                }

                /* If the nonblocking read() found no events to read, then
                   it returns -1 with errno set to EAGAIN. In that case,
                   we exit the loop. */
                if (len <= 0) {
                    break;
                }

                if (ev.e.len && !(ev.e.mask & IN_ISDIR)) {
                    const std::string name(ev.e.name);
                    ALOGV("device %s state changed", name.c_str());
                    if (matches(name)) {
                        if (ev.e.mask & IN_CREATE) {
                            mDaemon->onConnectionChange(getFullName(name), true /* connected*/);
                        }

                        if (ev.e.mask & IN_DELETE || ev.e.mask & IN_MOVED_FROM) {
                            mDaemon->onConnectionChange(getFullName(name), false /* connected*/);
                        }
                    }
                }
            }
        }
    }

    ALOGD("FileConnectionDetection thread exited");
    return false;
}
} // namespace SensorHalExt
} // namespace android
