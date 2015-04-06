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

#ifndef ANDROID_INPUT_HOST_H_
#define ANDROID_INPUT_HOST_H_

#include <memory>

#include <hardware/input.h>

namespace android {

/**
 * Classes in this file wrap the corresponding interfaces in the Input HAL. They
 * are intended to be lightweight and passed by value. It is still important not
 * to use an object after a HAL-specific method has freed the underlying
 * representation.
 *
 * See hardware/input.h for details about each of these methods.
 */

using InputBus = input_bus_t;
using InputCollectionId = input_collection_id_t;
using InputDeviceHandle = input_device_handle_t*;
using InputDeviceIdentifier = input_device_identifier_t*;
using InputUsage = input_usage_t;

class InputHostBase {
protected:
    InputHostBase(input_host_t* host, input_host_callbacks_t cb) : mHost(host), mCallbacks(cb) {}
    virtual ~InputHostBase() = default;

    input_host_t* mHost;
    input_host_callbacks_t mCallbacks;
};

class InputReport : private InputHostBase {
public:
    virtual ~InputReport() = default;

    InputReport(const InputReport& rhs) = default;
    InputReport& operator=(const InputReport& rhs) = default;
    operator input_report_t*() const { return mReport; }

    void reportEvent(InputDeviceHandle d);

private:
    friend class InputReportDefinition;

    InputReport(input_host_t* host, input_host_callbacks_t cb, input_report_t* r) :
        InputHostBase(host, cb), mReport(r) {}

    input_report_t* mReport;
};

class InputReportDefinition : private InputHostBase {
public:
    virtual ~InputReportDefinition() = default;

    InputReportDefinition(const InputReportDefinition& rhs) = default;
    InputReportDefinition& operator=(const InputReportDefinition& rhs) = default;
    operator input_report_definition_t*() { return mReportDefinition; }

    void addCollection(InputCollectionId id, int32_t arity);
    void declareUsage(InputCollectionId id, InputUsage usage, int32_t min, int32_t max,
            float resolution);
    void declareUsage(InputCollectionId id, InputUsage* usage, size_t usageCount);

    InputReport allocateReport();

private:
    friend class InputHost;

    InputReportDefinition(
            input_host_t* host, input_host_callbacks_t cb, input_report_definition_t* r) :
        InputHostBase(host, cb), mReportDefinition(r) {}

    input_report_definition_t* mReportDefinition;
};

class InputDeviceDefinition : private InputHostBase {
public:
    virtual ~InputDeviceDefinition() = default;

    InputDeviceDefinition(const InputDeviceDefinition& rhs) = default;
    InputDeviceDefinition& operator=(const InputDeviceDefinition& rhs) = default;
    operator input_device_definition_t*() { return mDeviceDefinition; }

    void addReport(InputReportDefinition r);

private:
    friend class InputHost;

    InputDeviceDefinition(
            input_host_t* host, input_host_callbacks_t cb, input_device_definition_t* d) :
        InputHostBase(host, cb), mDeviceDefinition(d) {}

    input_device_definition_t* mDeviceDefinition;
};

class InputProperty : private InputHostBase {
public:
    virtual ~InputProperty();

    operator input_property_t*() { return mProperty; }

    const char* getKey();
    const char* getValue();

    // Default move constructor transfers ownership of the input_property_t
    // pointer.
    InputProperty(InputProperty&& rhs) = default;

    // Prevent copy/assign because of the ownership of the underlying
    // input_property_t pointer.
    InputProperty(const InputProperty& rhs) = delete;
    InputProperty& operator=(const InputProperty& rhs) = delete;

private:
    friend class InputPropertyMap;

    InputProperty(
            input_host_t* host, input_host_callbacks_t cb, input_property_t* p) :
        InputHostBase(host, cb), mProperty(p) {}

    input_property_t* mProperty;
};

class InputPropertyMap : private InputHostBase {
public:
    virtual ~InputPropertyMap();

    operator input_property_map_t*() { return mMap; }

    InputProperty getDeviceProperty(const char* key);

    // Default move constructor transfers ownership of the input_property_map_t
    // pointer.
    InputPropertyMap(InputPropertyMap&& rhs) = default;

    // Prevent copy/assign because of the ownership of the underlying
    // input_property_map_t pointer.
    InputPropertyMap(const InputPropertyMap& rhs) = delete;
    InputPropertyMap& operator=(const InputPropertyMap& rhs) = delete;

private:
    friend class InputHost;

    InputPropertyMap(
            input_host_t* host, input_host_callbacks_t cb, input_property_map_t* m) :
        InputHostBase(host, cb), mMap(m) {}

    input_property_map_t* mMap;
};

class InputHost : private InputHostBase {
public:
    InputHost(input_host_t* host, input_host_callbacks_t cb) : InputHostBase(host, cb) {}
    virtual ~InputHost() = default;

    InputHost(const InputHost& rhs) = default;
    InputHost& operator=(const InputHost& rhs) = default;

    InputDeviceIdentifier createDeviceIdentifier(const char* name, int32_t productId,
            int32_t vendorId, InputBus bus, const char* uniqueId);

    InputDeviceDefinition createDeviceDefinition();
    InputReportDefinition createInputReportDefinition();
    InputReportDefinition createOutputReportDefinition();

    InputDeviceHandle registerDevice(InputDeviceIdentifier id, InputDeviceDefinition d);
    void unregisterDevice(InputDeviceHandle handle);

    InputPropertyMap getDevicePropertyMap(InputDeviceIdentifier id);
};

}  // namespace android

#endif  // ANDROID_INPUT_HOST_H_
