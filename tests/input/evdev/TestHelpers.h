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

#ifndef ANDROID_TEST_HELPERS_H_
#define ANDROID_TEST_HELPERS_H_

#include <future>
#include <thread>

namespace android {

/**
 * Runs the given function after the specified delay.
 * NOTE: if the std::future returned from std::async is not bound, this function
 * will block until the task completes. This is almost certainly NOT what you
 * want, so save the return value from delay_async into a variable:
 *
 * auto f = delay_async(100ms, []{ ALOGD("Hello world"); });
 */
template<class Function, class Duration>
decltype(auto) delay_async(Duration&& delay, Function&& task)
{
    return std::async(std::launch::async, [=]{ std::this_thread::sleep_for(delay); task(); });
}

/**
 * Creates and opens a temporary file at the given path. The file is unlinked
 * and closed in the destructor.
 */
class TempFile {
public:
    explicit TempFile(const char* path);
    ~TempFile();

    // No copy or assign
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    const char* getName() const { return mName; }
    int getFd() const { return mFd; }

private:
    char* mName;
    int mFd;
};

/**
 * Creates a temporary directory that can create temporary files. The directory
 * is emptied and deleted in the destructor.
 */
class TempDir {
public:
    TempDir();
    ~TempDir();

    // No copy or assign
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const char* getName() const { return mName; }

    TempFile* newTempFile();

private:
    char* mName;
};

}  // namespace android

#endif  // ANDROID_TEST_HELPERS_H_
