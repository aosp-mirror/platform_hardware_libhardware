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

#include <memory>

#include <linux/input.h>

#include <gtest/gtest.h>

#include "InputMocks.h"
#include "MockInputHost.h"
#include "SwitchInputMapper.h"

using ::testing::_;
using ::testing::Args;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace android {
namespace tests {

class SwitchInputMapperTest : public ::testing::Test {
protected:
     virtual void SetUp() override {
         mMapper = std::make_unique<SwitchInputMapper>();
     }

     MockInputHost mHost;
     std::unique_ptr<SwitchInputMapper> mMapper;
};

TEST_F(SwitchInputMapperTest, testConfigureDevice) {
    MockInputReportDefinition reportDef;
    MockInputDeviceNode deviceNode;
    deviceNode.addSwitch(SW_LID);
    deviceNode.addSwitch(SW_CAMERA_LENS_COVER);

    EXPECT_CALL(reportDef, addCollection(INPUT_COLLECTION_ID_SWITCH, 1));
    EXPECT_CALL(reportDef, declareUsages(INPUT_COLLECTION_ID_SWITCH, _, 2))
        .With(Args<1,2>(UnorderedElementsAre(INPUT_USAGE_SWITCH_LID,
                        INPUT_USAGE_SWITCH_CAMERA_LENS_COVER)));

    mMapper->configureInputReport(&deviceNode, &reportDef);
}

TEST_F(SwitchInputMapperTest, testConfigureDevice_noSwitches) {
    MockInputReportDefinition reportDef;
    MockInputDeviceNode deviceNode;

    EXPECT_CALL(reportDef, addCollection(_, _)).Times(0);
    EXPECT_CALL(reportDef, declareUsages(_, _, _)).Times(0);

    mMapper->configureInputReport(&deviceNode, &reportDef);
}

TEST_F(SwitchInputMapperTest, testProcessInput) {
    MockInputReportDefinition reportDef;
    MockInputDeviceNode deviceNode;
    deviceNode.addSwitch(SW_LID);

    EXPECT_CALL(reportDef, addCollection(_, _));
    EXPECT_CALL(reportDef, declareUsages(_, _, _));

    mMapper->configureInputReport(&deviceNode, &reportDef);

    MockInputReport report;
    EXPECT_CALL(reportDef, allocateReport())
        .WillOnce(Return(&report));

    {
        // Test two switch events in order
        InSequence s;
        EXPECT_CALL(report, setBoolUsage(INPUT_COLLECTION_ID_SWITCH, INPUT_USAGE_SWITCH_LID, 1, 0));
        EXPECT_CALL(report, reportEvent(_));
        EXPECT_CALL(report, setBoolUsage(INPUT_COLLECTION_ID_SWITCH, INPUT_USAGE_SWITCH_LID, 0, 0));
        EXPECT_CALL(report, reportEvent(_));
    }

    InputEvent events[] = {
        {0, EV_SW, SW_LID, 1},
        {1, EV_SYN, SYN_REPORT, 0},
        {2, EV_SW, SW_LID, 0},
        {3, EV_SYN, SYN_REPORT, 0},
    };
    for (auto e : events) {
        mMapper->process(e);
    }
}

}  // namespace tests
}  // namespace android

