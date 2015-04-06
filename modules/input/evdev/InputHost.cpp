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

#include "InputHost.h"

namespace android {

void InputReport::reportEvent(InputDeviceHandle d) {
    mCallbacks.report_event(mHost, d, mReport);
}

void InputReportDefinition::addCollection(InputCollectionId id, int32_t arity) {
    mCallbacks.input_report_definition_add_collection(mHost, mReportDefinition, id, arity);
}

void InputReportDefinition::declareUsage(InputCollectionId id, InputUsage usage,
        int32_t min, int32_t max, float resolution) {
    mCallbacks.input_report_definition_declare_usage_int(mHost, mReportDefinition,
            id, usage, min, max, resolution);
}

void InputReportDefinition::declareUsage(InputCollectionId id, InputUsage* usage,
        size_t usageCount) {
    mCallbacks.input_report_definition_declare_usages_bool(mHost, mReportDefinition,
            id, usage, usageCount);
}

InputReport InputReportDefinition::allocateReport() {
    return InputReport(mHost, mCallbacks,
            mCallbacks.input_allocate_report(mHost, mReportDefinition));
}

void InputDeviceDefinition::addReport(InputReportDefinition r) {
    mCallbacks.input_device_definition_add_report(mHost, mDeviceDefinition, r);
}

InputProperty::~InputProperty() {
    mCallbacks.input_free_device_property(mHost, mProperty);
}

const char* InputProperty::getKey() {
    return mCallbacks.input_get_property_key(mHost, mProperty);
}

const char* InputProperty::getValue() {
    return mCallbacks.input_get_property_value(mHost, mProperty);
}

InputPropertyMap::~InputPropertyMap() {
    mCallbacks.input_free_device_property_map(mHost, mMap);
}

InputProperty InputPropertyMap::getDeviceProperty(const char* key) {
    return InputProperty(mHost, mCallbacks,
            mCallbacks.input_get_device_property(mHost, mMap, key));
}

InputDeviceIdentifier InputHost::createDeviceIdentifier(const char* name, int32_t productId,
        int32_t vendorId, InputBus bus, const char* uniqueId) {
    return mCallbacks.create_device_identifier(mHost, name, productId, vendorId, bus, uniqueId);
}

InputDeviceDefinition InputHost::createDeviceDefinition() {
    return InputDeviceDefinition(mHost, mCallbacks, mCallbacks.create_device_definition(mHost));
}

InputReportDefinition InputHost::createInputReportDefinition() {
    return InputReportDefinition(mHost, mCallbacks,
            mCallbacks.create_input_report_definition(mHost));
}

InputReportDefinition InputHost::createOutputReportDefinition() {
    return InputReportDefinition(mHost, mCallbacks,
            mCallbacks.create_output_report_definition(mHost));
}

InputDeviceHandle InputHost::registerDevice(InputDeviceIdentifier id,
        InputDeviceDefinition d) {
    return mCallbacks.register_device(mHost, id, d);
}

void InputHost::unregisterDevice(InputDeviceHandle handle) {
    return mCallbacks.unregister_device(mHost, handle);
}

InputPropertyMap InputHost::getDevicePropertyMap(InputDeviceIdentifier id) {
    return InputPropertyMap(mHost, mCallbacks,
            mCallbacks.input_get_device_property_map(mHost, id));
}

}  // namespace android
