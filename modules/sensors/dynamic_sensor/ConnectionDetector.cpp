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
            : ConnectionDetector(d), Thread(false /*callCallJava*/), mPath(path), mRegex(regex),
              mLooper(new Looper(true /*allowNonCallback*/)), mInotifyFd(-1) {
    if (mLooper == nullptr) {
        return;
    }

    mInotifyFd = ::inotify_init1(IN_NONBLOCK);
    if (mInotifyFd < 0) {
        ALOGE("Cannot init inotify");
        return;
    }

    int wd = ::inotify_add_watch(mInotifyFd, path.c_str(), IN_CREATE | IN_DELETE);
    if (wd < 0 || !mLooper->addFd(mInotifyFd, POLL_IDENT, Looper::EVENT_INPUT, nullptr, nullptr)) {
        ::close(mInotifyFd);
        mInotifyFd = -1;
        ALOGE("Cannot setup watch on dir %s", path.c_str());
        return;
    }

    // mLooper != null && mInotifyFd added to looper
    run("ddad_file");
}

FileConnectionDetector::~FileConnectionDetector() {
    if (mInotifyFd > 0) {
        requestExit();
        mLooper->wake();
        join();
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

void FileConnectionDetector::handleInotifyData(ssize_t len, const char *data) {
    const char *dataEnd = data + len;
    const struct inotify_event *ev;

    // inotify adds paddings to guarantee the next read is aligned
    for (; data < dataEnd; data += sizeof(struct inotify_event) + ev->len) {
        ev = reinterpret_cast<const struct inotify_event*>(data);
        if (ev->mask & IN_ISDIR) {
            continue;
        }

        const std::string name(ev->name);
        if (matches(name)) {
            if (ev->mask & IN_CREATE) {
                mDaemon->onConnectionChange(getFullName(name), true /*connected*/);
            }
            if (ev->mask & IN_DELETE) {
                mDaemon->onConnectionChange(getFullName(name), false /*connected*/);
            }
        }
    }
}

bool FileConnectionDetector::readInotifyData() {
    struct {
        struct inotify_event ev;
        char padding[NAME_MAX + 1];
    } buffer;

    bool ret = true;
    while (true) {
        ssize_t len = ::read(mInotifyFd, &buffer, sizeof(buffer));
        if (len == -1 && errno == EAGAIN) {
            // no more data
            break;
        } else if (len > static_cast<ssize_t>(sizeof(struct inotify_event))) {
            handleInotifyData(len, reinterpret_cast<char*>(&buffer));
        } else if (len < 0) {
            ALOGE("read error: %s", ::strerror(errno));
            ret = false;
            break;
        } else {
            // 0 <= len <= sizeof(struct inotify_event)
            ALOGE("read return %zd, shorter than inotify_event size %zu",
                  len, sizeof(struct inotify_event));
            ret = false;
            break;
        }
    }
    return ret;
}

bool FileConnectionDetector::threadLoop() {
    Looper::setForThread(mLooper);
    processExistingFiles();
    while(!Thread::exitPending()) {
        int ret = mLooper->pollOnce(-1);

        if (ret != Looper::POLL_WAKE && ret != POLL_IDENT) {
            ALOGE("Unexpected value %d from pollOnce, quit", ret);
            requestExit();
            break;
        }

        if (ret == POLL_IDENT) {
            if (!readInotifyData()) {
                requestExit();
            }
        }
    }

    mLooper->removeFd(mInotifyFd);
    ALOGD("FileConnectionDetection thread exited");
    return false;
}

} // namespace SensorHalExt
} // namespace android
