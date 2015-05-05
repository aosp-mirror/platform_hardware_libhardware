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
 * are intended to be lightweight, as they primarily wrap pointers to callbacks.
 * It is still important not to use an object after a HAL-specific method has
 * freed the underlying representation.
 *
 * See hardware/input.h for details about each of these methods.
 */

using InputBus = input_bus_t;
using InputCollectionId = input_collection_id_t;
using InputDeviceHandle = input_device_handle_t;
using InputDeviceIdentifier = input_device_identifier_t;
using InputUsage = input_usage_t;

class InputHostBase {
protected:
    InputHostBase(input_host_t* host, input_host_callbacks_t cb) : mHost(host), mCallbacks(cb) {}
    virtual ~InputHostBase() = default;

    InputHostBase(const InputHostBase& rhs) = delete;
    InputHostBase(InputHostBase&& rhs) = delete;

    input_host_t* mHost;
    input_host_callbacks_t mCallbacks;
};

class InputReport : private InputHostBase {
public:
    InputReport(input_host_t* host, input_host_callbacks_t cb, input_report_t* r) :
        InputHostBase(host, cb), mReport(r) {}
    virtual ~InputReport() = default;

    virtual void setIntUsage(InputCollectionId id, InputUsage usage, int32_t value,
            int32_t arityIndex);
    virtual void setBoolUsage(InputCollectionId id, InputUsage usage, bool value,
            int32_t arityIndex);
    virtual void reportEvent(InputDeviceHandle* d);

    operator input_report_t*() const { return mReport; }

    InputReport(const InputReport& rhs) = delete;
    InputReport& operator=(const InputReport& rhs) = delete;
private:
    input_report_t* mReport;
};

class InputReportDefinition : private InputHostBase {
public:
    InputReportDefinition(input_host_t* host, input_host_callbacks_t cb,
            input_report_definition_t* r) : InputHostBase(host, cb), mReportDefinition(r) {}
    virtual ~InputReportDefinition() = default;

    virtual void addCollection(InputCollectionId id, int32_t arity);
    virtual void declareUsage(InputCollectionId id, InputUsage usage, int32_t min, int32_t max,
            float resolution);
    virtual void declareUsages(InputCollectionId id, InputUsage* usage, size_t usageCount);

    virtual InputReport* allocateReport();

    operator input_report_definition_t*() { return mReportDefinition; }

    InputReportDefinition(const InputReportDefinition& rhs) = delete;
    InputReportDefinition& operator=(const InputReportDefinition& rhs) = delete;
private:
    input_report_definition_t* mReportDefinition;
};

class InputDeviceDefinition : private InputHostBase {
public:
    InputDeviceDefinition(input_host_t* host, input_host_callbacks_t cb,
            input_device_definition_t* d) :
        InputHostBase(host, cb), mDeviceDefinition(d) {}
    virtual ~InputDeviceDefinition() = default;

    virtual void addReport(InputReportDefinition* r);

    operator input_device_definition_t*() { return mDeviceDefinition; }

    InputDeviceDefinition(const InputDeviceDefinition& rhs) = delete;
    InputDeviceDefinition& operator=(const InputDeviceDefinition& rhs) = delete;
private:
    input_device_definition_t* mDeviceDefinition;
};

class InputProperty : private InputHostBase {
public:
    virtual ~InputProperty() = default;

    InputProperty(input_host_t* host, input_host_callbacks_t cb, input_property_t* p) :
        InputHostBase(host, cb), mProperty(p) {}

    virtual const char* getKey() const;
    virtual const char* getValue() const;

    operator input_property_t*() { return mProperty; }

    InputProperty(const InputProperty& rhs) = delete;
    InputProperty& operator=(const InputProperty& rhs) = delete;
private:
    input_property_t* mProperty;
};

class InputPropertyMap : private InputHostBase {
public:
    virtual ~InputPropertyMap() = default;

    InputPropertyMap(input_host_t* host, input_host_callbacks_t cb, input_property_map_t* m) :
        InputHostBase(host, cb), mMap(m) {}

    virtual InputProperty* getDeviceProperty(const char* key) const;
    virtual void freeDeviceProperty(InputProperty* property) const;

    operator input_property_map_t*() { return mMap; }

    InputPropertyMap(const InputPropertyMap& rhs) = delete;
    InputPropertyMap& operator=(const InputPropertyMap& rhs) = delete;
private:
    input_property_map_t* mMap;
};

class InputHostInterface {
public:
    virtual ~InputHostInterface() = default;

    virtual InputDeviceIdentifier* createDeviceIdentifier(const char* name, int32_t productId,
            int32_t vendorId, InputBus bus, const char* uniqueId) = 0;

    virtual InputDeviceDefinition* createDeviceDefinition() = 0;
    virtual InputReportDefinition* createInputReportDefinition() = 0;
    virtual InputReportDefinition* createOutputReportDefinition() = 0;
    virtual void freeReportDefinition(InputReportDefinition* reportDef) = 0;

    virtual InputDeviceHandle* registerDevice(InputDeviceIdentifier* id,
            InputDeviceDefinition* d) = 0;
    virtual void unregisterDevice(InputDeviceHandle* handle) = 0;

    virtual InputPropertyMap* getDevicePropertyMap(InputDeviceIdentifier* id) = 0;
    virtual void freeDevicePropertyMap(InputPropertyMap* propertyMap) = 0;
};

class InputHost : public InputHostInterface, private InputHostBase {
public:
    InputHost(input_host_t* host, input_host_callbacks_t cb) : InputHostBase(host, cb) {}
    virtual ~InputHost() = default;

    InputDeviceIdentifier* createDeviceIdentifier(const char* name, int32_t productId,
            int32_t vendorId, InputBus bus, const char* uniqueId) override;

    InputDeviceDefinition* createDeviceDefinition() override;
    InputReportDefinition* createInputReportDefinition() override;
    InputReportDefinition* createOutputReportDefinition() override;
    virtual void freeReportDefinition(InputReportDefinition* reportDef) override;

    InputDeviceHandle* registerDevice(InputDeviceIdentifier* id, InputDeviceDefinition* d) override;
    void unregisterDevice(InputDeviceHandle* handle) override;

    InputPropertyMap* getDevicePropertyMap(InputDeviceIdentifier* id) override;
    void freeDevicePropertyMap(InputPropertyMap* propertyMap) override;

    InputHost(const InputHost& rhs) = delete;
    InputHost& operator=(const InputHost& rhs) = delete;
};

}  // namespace android

#endif  // ANDROID_INPUT_HOST_H_
