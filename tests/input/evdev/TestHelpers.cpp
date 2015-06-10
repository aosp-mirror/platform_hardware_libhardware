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

#define LOG_TAG "TestHelpers"
#define LOG_NDEBUG 0

#include "TestHelpers.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <utils/Log.h>

namespace android {

static const char kTmpDirTemplate[] = "/data/local/tmp/XXXXXX";

TempFile::TempFile(const char* path) {
    bool needTrailingSlash = path[strlen(path) - 1] != '/';
    // name = path + XXXXXX + \0
    size_t nameLen = strlen(path) + 6 + 1;
    if (needTrailingSlash) nameLen++;

    mName = new char[nameLen];
    strcpy(mName, path);
    if (needTrailingSlash) {
        strcat(mName, "/");
    }
    strcat(mName, "XXXXXX");
    mName = mktemp(mName);
    LOG_FATAL_IF(mName == nullptr, "could not create temp filename %s. errno=%d", mName, errno);

    int result = TEMP_FAILURE_RETRY(mkfifo(mName, S_IRWXU));
    LOG_FATAL_IF(result < 0, "could not create fifo %s. errno=%d", mName, errno);

    mFd = TEMP_FAILURE_RETRY(open(mName, O_RDWR | O_NONBLOCK));
    LOG_FATAL_IF(mFd < 0, "could not open fifo %s. errno=%d", mName, errno);
}

TempFile::~TempFile() {
    if (unlink(mName) < 0) {
        ALOGE("could not unlink %s. errno=%d", mName, errno);
    }
    if (close(mFd) < 0) {
        ALOGE("could not close %d. errno=%d", mFd, errno);
    }
    delete[] mName;
}

TempDir::TempDir() {
    mName = new char[sizeof(kTmpDirTemplate)];
    strcpy(mName, kTmpDirTemplate);
    mName = mkdtemp(mName);
    LOG_FATAL_IF(mName == nullptr, "could not allocate tempdir %s", mName);
}

TempDir::~TempDir() {
    auto tmpDir = opendir(mName);
    while (auto entry = readdir(tmpDir)) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        ALOGD("stale file %s, removing", entry->d_name);
        unlink(entry->d_name);
    }
    closedir(tmpDir);
    rmdir(mName);
    delete mName;
}

TempFile* TempDir::newTempFile() {
    return new TempFile(mName);
}

}  // namespace android
