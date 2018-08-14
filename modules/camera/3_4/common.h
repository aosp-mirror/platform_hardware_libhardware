/*
 * Copyright 2015 The Android Open Source Project
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

#ifndef V4L2_CAMERA_HAL_COMMON_H_
#define V4L2_CAMERA_HAL_COMMON_H_

#include <cutils/log.h>

// Helpers of logging (showing function name and line number).
#define HAL_LOGE(fmt, args...) do { \
    ALOGE("%s:%d: " fmt, __func__, __LINE__, ##args);   \
  } while(0)

#define HAL_LOGE_IF(cond, fmt, args...) do { \
    ALOGE_IF(cond, "%s:%d: " fmt, __func__, __LINE__, ##args);  \
  } while(0)

#define HAL_LOGW(fmt, args...) do { \
    ALOGW("%s:%d: " fmt, __func__, __LINE__, ##args);   \
  } while(0)

#define HAL_LOGW_IF(cond, fmt, args...) do { \
    ALOGW_IF(cond, "%s:%d: " fmt, __func__, __LINE__, ##args);  \
  } while(0)

#define HAL_LOGI(fmt, args...) do { \
    ALOGI("%s:%d: " fmt, __func__, __LINE__, ##args);   \
  } while(0)

#define HAL_LOGI_IF(cond, fmt, args...) do { \
    ALOGI_IF(cond, "%s:%d: " fmt, __func__, __LINE__, ##args);  \
  } while(0)

#define HAL_LOGD(fmt, args...) do { \
    ALOGD("%s:%d: " fmt, __func__, __LINE__, ##args);   \
  } while(0)

#define HAL_LOGV(fmt, args...) do { \
    ALOGV("%s:%d: " fmt, __func__, __LINE__, ##args);   \
  } while(0)

// Log enter/exit of methods.
#define HAL_LOG_ENTER() HAL_LOGV("enter")
#define HAL_LOG_EXIT() HAL_LOGV("exit")

#endif  // V4L2_CAMERA_HAL_COMMON_H_
