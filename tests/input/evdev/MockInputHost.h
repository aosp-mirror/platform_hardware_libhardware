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

#ifndef ANDROID_MOCK_INPUT_HOST_H_
#define ANDROID_MOCK_INPUT_HOST_H_

#include "InputHost.h"

#include "gmock/gmock.h"


namespace android {
namespace tests {

class MockInputReport : public InputReport {
public:
    MockInputReport() : InputReport(nullptr, {}, nullptr) {}
    MOCK_METHOD4(setIntUsage, void(InputCollectionId id, InputUsage usage, int32_t value,
                int32_t arityIndex));
    MOCK_METHOD4(setBoolUsage, void(InputCollectionId id, InputUsage usage, bool value,
                int32_t arityIndex));
    MOCK_METHOD1(reportEvent, void(InputDeviceHandle* d));
};

class MockInputReportDefinition : public InputReportDefinition {
public:
    MockInputReportDefinition() : InputReportDefinition(nullptr, {}, nullptr) {}
    MOCK_METHOD2(addCollection, void(InputCollectionId id, int32_t arity));
    MOCK_METHOD5(declareUsage, void(InputCollectionId id, InputUsage usage, int32_t min,
                int32_t max, float resolution));
    MOCK_METHOD3(declareUsages, void(InputCollectionId id, InputUsage* usage, size_t usageCount));
    MOCK_METHOD0(allocateReport, InputReport*());
};

class MockInputDeviceDefinition : public InputDeviceDefinition {
public:
    MockInputDeviceDefinition() : InputDeviceDefinition(nullptr, {}, nullptr) {}
    MOCK_METHOD1(addReport, void(InputReportDefinition* r));
};

class MockInputProperty : public InputProperty {
public:
    MockInputProperty() : InputProperty(nullptr, {}, nullptr) {}
    virtual ~MockInputProperty() {}
    MOCK_CONST_METHOD0(getKey, const char*());
    MOCK_CONST_METHOD0(getValue, const char*());
};

class MockInputPropertyMap : public InputPropertyMap {
public:
    MockInputPropertyMap() : InputPropertyMap(nullptr, {}, nullptr) {}
    virtual ~MockInputPropertyMap() {}
    MOCK_CONST_METHOD1(getDeviceProperty, InputProperty*(const char* key));
    MOCK_CONST_METHOD1(freeDeviceProperty, void(InputProperty* property));
};

class MockInputHost : public InputHostInterface {
public:
    MOCK_METHOD5(createDeviceIdentifier, InputDeviceIdentifier*(
                const char* name, int32_t productId, int32_t vendorId, InputBus bus,
                const char* uniqueId));
    MOCK_METHOD0(createDeviceDefinition, InputDeviceDefinition*());
    MOCK_METHOD0(createInputReportDefinition, InputReportDefinition*());
    MOCK_METHOD0(createOutputReportDefinition, InputReportDefinition*());
    MOCK_METHOD1(freeReportDefinition, void(InputReportDefinition* reportDef));
    MOCK_METHOD2(registerDevice, InputDeviceHandle*(InputDeviceIdentifier* id,
                InputDeviceDefinition* d));
    MOCK_METHOD1(unregisterDevice, void(InputDeviceHandle* handle));
    MOCK_METHOD1(getDevicePropertyMap, InputPropertyMap*(InputDeviceIdentifier* id));
    MOCK_METHOD1(freeDevicePropertyMap, void(InputPropertyMap* propertyMap));
};

}  // namespace tests
}  // namespace android

#endif  // ANDROID_MOCK_INPUT_HOST_H_
