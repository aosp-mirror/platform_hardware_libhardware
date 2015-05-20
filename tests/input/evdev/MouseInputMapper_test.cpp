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
#include "MouseInputMapper.h"

using ::testing::_;
using ::testing::Args;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace android {
namespace tests {

class MouseInputMapperTest : public ::testing::Test {
protected:
     virtual void SetUp() override {
         mMapper = std::make_unique<MouseInputMapper>();
     }

     MockInputHost mHost;
     std::unique_ptr<MouseInputMapper> mMapper;
};

TEST_F(MouseInputMapperTest, testConfigureDevice) {
    MockInputReportDefinition reportDef;
    MockInputDeviceNode deviceNode;
    deviceNode.addKeys(BTN_LEFT, BTN_RIGHT, BTN_MIDDLE);
    deviceNode.addRelAxis(REL_X);
    deviceNode.addRelAxis(REL_Y);

    const auto id = INPUT_COLLECTION_ID_MOUSE;
    EXPECT_CALL(reportDef, addCollection(id, 1));
    EXPECT_CALL(reportDef, declareUsage(id, INPUT_USAGE_AXIS_X, _, _, _));
    EXPECT_CALL(reportDef, declareUsage(id, INPUT_USAGE_AXIS_Y, _, _, _));
    EXPECT_CALL(reportDef, declareUsages(id, _, 3))
        .With(Args<1,2>(UnorderedElementsAre(
                        INPUT_USAGE_BUTTON_PRIMARY,
                        INPUT_USAGE_BUTTON_SECONDARY,
                        INPUT_USAGE_BUTTON_TERTIARY)));

    EXPECT_TRUE(mMapper->configureInputReport(&deviceNode, &reportDef));
}

TEST_F(MouseInputMapperTest, testConfigureDevice_noXAxis) {
    MockInputReportDefinition reportDef;
    MockInputDeviceNode deviceNode;

    EXPECT_CALL(reportDef, addCollection(INPUT_COLLECTION_ID_MOUSE, 1));
    EXPECT_CALL(reportDef, declareUsage(_, _, _, _, _)).Times(0);
    EXPECT_CALL(reportDef, declareUsages(_, _, _)).Times(0);

    EXPECT_FALSE(mMapper->configureInputReport(&deviceNode, &reportDef));
}

TEST_F(MouseInputMapperTest, testProcessInput) {
    MockInputReportDefinition reportDef;
    MockInputDeviceNode deviceNode;
    deviceNode.addKeys(BTN_LEFT, BTN_RIGHT, BTN_MIDDLE);
    deviceNode.addRelAxis(REL_X);
    deviceNode.addRelAxis(REL_Y);

    EXPECT_CALL(reportDef, addCollection(_, _));
    EXPECT_CALL(reportDef, declareUsage(_, _, _, _, _)).Times(2);
    EXPECT_CALL(reportDef, declareUsages(_, _, 3));

    mMapper->configureInputReport(&deviceNode, &reportDef);

    MockInputReport report;
    EXPECT_CALL(reportDef, allocateReport())
        .WillOnce(Return(&report));

    {
        // Test two switch events in order
        InSequence s;
        const auto id = INPUT_COLLECTION_ID_MOUSE;
        EXPECT_CALL(report, setIntUsage(id, INPUT_USAGE_AXIS_X, 5, 0));
        EXPECT_CALL(report, setIntUsage(id, INPUT_USAGE_AXIS_Y, -3, 0));
        EXPECT_CALL(report, reportEvent(_));
        EXPECT_CALL(report, setBoolUsage(id, INPUT_USAGE_BUTTON_PRIMARY, 1, 0));
        EXPECT_CALL(report, reportEvent(_));
        EXPECT_CALL(report, setBoolUsage(id, INPUT_USAGE_BUTTON_PRIMARY, 0, 0));
        EXPECT_CALL(report, reportEvent(_));
    }

    InputEvent events[] = {
        {0, EV_REL, REL_X, 5},
        {1, EV_REL, REL_Y, -3},
        {2, EV_SYN, SYN_REPORT, 0},
        {0, EV_KEY, BTN_LEFT, 1},
        {1, EV_SYN, SYN_REPORT, 0},
        {2, EV_KEY, BTN_LEFT, 0},
        {3, EV_SYN, SYN_REPORT, 0},
    };
    for (auto e : events) {
        mMapper->process(e);
    }
}

}  // namespace tests
}  // namespace android

