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

#ifndef ANDROID_INPUT_MOCKS_H_
#define ANDROID_INPUT_MOCKS_H_

#include <map>
#include <set>
#include <string>

#include <linux/input.h>

#include "InputHub.h"

namespace android {

class MockInputDeviceNode : public InputDeviceNode {
public:
    MockInputDeviceNode() = default;
    virtual ~MockInputDeviceNode() = default;

    virtual const std::string& getPath() const override { return mPath; }
    virtual const std::string& getName() const override { return mName; }
    virtual const std::string& getLocation() const override { return mLocation; }
    virtual const std::string& getUniqueId() const override { return mUniqueId; }

    void setPath(const std::string& path) { mPath = path; }
    void setName(const std::string& name) { mName = name; }
    void setLocation(const std::string& location) { mLocation = location; }
    void setUniqueId(const std::string& uniqueId) { mUniqueId = uniqueId; }

    virtual uint16_t getBusType() const override { return mBusType; }
    virtual uint16_t getVendorId() const override { return mVendorId; }
    virtual uint16_t getProductId() const override { return mProductId; }
    virtual uint16_t getVersion() const override { return mVersion; }

    void setBusType(uint16_t busType) { mBusType = busType; }
    void setVendorId(uint16_t vendorId) { mVendorId = vendorId; }
    void setProductId(uint16_t productId) { mProductId = productId; }
    void setVersion(uint16_t version) { mVersion = version; }

    virtual bool hasKey(int32_t key) const override { return mKeys.count(key); }
    virtual bool hasKeyInRange(int32_t startKey, int32_t endKey) const override;
    virtual bool hasRelativeAxis(int axis) const override { return mRelAxes.count(axis); }
    virtual bool hasAbsoluteAxis(int32_t axis) const override { return mAbsAxes.count(axis); }
    virtual bool hasSwitch(int32_t sw) const override { return mSwitches.count(sw); }
    virtual bool hasForceFeedback(int32_t ff) const override { return mForceFeedbacks.count(ff); }
    virtual bool hasInputProperty(int32_t property) const override {
        return mInputProperties.count(property);
    }

    // base case
    void addKeys() {}
    // inductive case
    template<typename I, typename... Is>
    void addKeys(I key, Is... keys) {
        // Add the first key
        mKeys.insert(key);
        // Recursively add the remaining keys
        addKeys(keys...);
    }

    void addRelAxis(int32_t axis) { mRelAxes.insert(axis); }
    void addAbsAxis(int32_t axis, AbsoluteAxisInfo* info) { mAbsAxes[axis] = info; }
    void addSwitch(int32_t sw) { mSwitches.insert(sw); }
    void addForceFeedback(int32_t ff) { mForceFeedbacks.insert(ff); }
    void addInputProperty(int32_t property) { mInputProperties.insert(property); }

    virtual int32_t getKeyState(int32_t key) const override { return 0; }
    virtual int32_t getSwitchState(int32_t sw) const override { return 0; }
    virtual const AbsoluteAxisInfo* getAbsoluteAxisInfo(int32_t axis) const override {
        auto iter = mAbsAxes.find(axis);
        if (iter != mAbsAxes.end()) {
            return iter->second;
        }
        return nullptr;
    }
    virtual status_t getAbsoluteAxisValue(int32_t axis, int32_t* outValue) const override {
        // TODO
        return 0;
    }

    virtual void vibrate(nsecs_t duration) override {}
    virtual void cancelVibrate() override {}

    virtual void disableDriverKeyRepeat() override { mKeyRepeatDisabled = true; }

    bool isDriverKeyRepeatEnabled() { return mKeyRepeatDisabled; }

private:
    std::string mPath = "/test";
    std::string mName = "Test Device";
    std::string mLocation = "test/0";
    std::string mUniqueId = "test-id";

    uint16_t mBusType = 0;
    uint16_t mVendorId = 0;
    uint16_t mProductId = 0;
    uint16_t mVersion = 0;

    std::set<int32_t> mKeys;
    std::set<int32_t> mRelAxes;
    std::map<int32_t, AbsoluteAxisInfo*> mAbsAxes;
    std::set<int32_t> mSwitches;
    std::set<int32_t> mForceFeedbacks;
    std::set<int32_t> mInputProperties;

    bool mKeyRepeatDisabled = false;
};

namespace MockNexus7v2 {
MockInputDeviceNode* getElanTouchscreen();
MockInputDeviceNode* getLidInput();
MockInputDeviceNode* getButtonJack();
MockInputDeviceNode* getHeadsetJack();
MockInputDeviceNode* getH2wButton();
MockInputDeviceNode* getGpioKeys();
}  // namespace MockNexus7v2

namespace MockNexusPlayer {
MockInputDeviceNode* getGpioKeys();
MockInputDeviceNode* getMidPowerBtn();
MockInputDeviceNode* getNexusRemote();
MockInputDeviceNode* getAsusGamepad();
}  // namespace MockNexusPlayer

}  // namespace android

#endif  // ANDROID_INPUT_MOCKS_H_
