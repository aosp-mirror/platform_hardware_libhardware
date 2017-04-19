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
#ifndef HIDUTIL_HIDLOG_H_
#define HIDUTIL_HIDLOG_H_

#if defined(__ANDROID__) && !defined(LOG_TO_CONSOLE)
#include <android-base/logging.h>
#define LOG_ENDL ""
#define LOG_E LOG(ERROR)
#define LOG_W LOG(WARNING)
#define LOG_I LOG(INFO)
#define LOG_D LOG(DEBUG)
#define LOG_V LOG(VERBOSE)
#else
#include <iostream>
#define LOG_ENDL std::endl
#define LOG_E (std::cerr << "E: ")
#define LOG_W (std::cerr << "W: ")
#define LOG_I (std::cerr << "I: ")
#define LOG_D (std::cerr << "D: ")
#define LOG_V (std::cerr << "V: ")
#endif

#endif // HIDUTIL_HIDLOG_H_
