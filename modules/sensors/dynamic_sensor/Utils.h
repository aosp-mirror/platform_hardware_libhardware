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
#ifndef ANDROID_SENSORHAL_EXT_UTILS_H
#define ANDROID_SENSORHAL_EXT_UTILS_H

// Host build does not have RefBase
#ifdef __ANDROID__
#include <utils/RefBase.h>
#define REF_BASE(a) ::android::RefBase
#define SP(a) sp<a>
#define WP(a) wp<a>
#define SP_THIS this
#define PROMOTE(a) (a).promote()
#else
#include <memory>
#define REF_BASE(a) std::enable_shared_from_this<a>
#define SP(a) std::shared_ptr<a>
#define WP(a) std::weak_ptr<a>
#define SP_THIS shared_from_this()
#define PROMOTE(a) (a).lock()
#endif

#endif // ANDROID_SENSORHAL_EXT_UTILS_H
