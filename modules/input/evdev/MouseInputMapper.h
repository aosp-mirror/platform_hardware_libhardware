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

#ifndef ANDROID_MOUSE_INPUT_MAPPER_H_
#define ANDROID_MOUSE_INPUT_MAPPER_H_

#include <cstdint>

#include <utils/BitSet.h>
#include <utils/Timers.h>

#include "InputMapper.h"

namespace android {

class MouseInputMapper : public InputMapper {
public:
    virtual ~MouseInputMapper() = default;

    virtual bool configureInputReport(InputDeviceNode* devNode,
            InputReportDefinition* report) override;
    virtual void process(const InputEvent& event) override;

private:
    void processMotion(int32_t code, int32_t value);
    void processButton(int32_t code, int32_t value);
    void sync(nsecs_t when);

    BitSet32 mButtonValues;
    BitSet32 mUpdatedButtonMask;

    int32_t mRelX = 0;
    int32_t mRelY = 0;

    int32_t mRelWheel = 0;
    int32_t mRelHWheel = 0;
};

}  // namespace android

#endif  // ANDROID_MOUSE_INPUT_MAPPER_H_
