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

#ifndef ANDROID_INPUT_HUB_H_
#define ANDROID_INPUT_HUB_H_

#include <memory>
#include <string>
#include <unordered_map>

#include <utils/String8.h>
#include <utils/Timers.h>

namespace android {

/**
 * InputEvent represents an event from the kernel. The fields largely mirror
 * those found in linux/input.h.
 */
struct InputEvent {
    nsecs_t when;

    int32_t type;
    int32_t code;
    int32_t value;
};

/** Describes an absolute axis. */
struct AbsoluteAxisInfo {
    int32_t minValue = 0;   // minimum value
    int32_t maxValue = 0;   // maximum value
    int32_t flat = 0;       // center flat position, e.g. flat == 8 means center is between -8 and 8
    int32_t fuzz = 0;       // error tolerance, e.g. fuzz == 4 means value is +/- 4 due to noise
    int32_t resolution = 0; // resolution in units per mm or radians per mm
};

/**
 * An InputDeviceNode represents a device node in the Linux system. It can be
 * used to interact with the device, setting and getting property values.
 *
 * An InputDeviceNode should only be used on the same thread that is polling for
 * input events.
 */
class InputDeviceNode {
public:
    /** Get the Linux device path for the node. */
    virtual const std::string& getPath() const = 0;

    /** Get the name of the device returned by the driver. */
    virtual const std::string& getName() const = 0;
    /** Get the location of the device returned by the driver. */
    virtual const std::string& getLocation() const = 0;
    /** Get the unique id of the device returned by the driver. */
    virtual const std::string& getUniqueId() const = 0;

    /** Get the bus type of the device returned by the driver. */
    virtual uint16_t getBusType() const = 0;
    /** Get the vendor id of the device returned by the driver. */
    virtual uint16_t getVendorId() const = 0;
    /** Get the product id of the device returned by the driver. */
    virtual uint16_t getProductId() const = 0;
    /** Get the version of the device driver. */
    virtual uint16_t getVersion() const = 0;

    /** Returns true if the device has the key. */
    virtual bool hasKey(int32_t key) const = 0;
    /** Returns true if the device has a key in the range [startKey, endKey). */
    virtual bool hasKeyInRange(int32_t startKey, int32_t endKey) const = 0;
    /** Returns true if the device has the relative axis. */
    virtual bool hasRelativeAxis(int32_t axis) const = 0;
    /** Returns true if the device has the absolute axis. */
    virtual bool hasAbsoluteAxis(int32_t axis) const = 0;
    /** Returns true if the device has the switch. */
    virtual bool hasSwitch(int32_t sw) const = 0;
    /** Returns true if the device has the force feedback method. */
    virtual bool hasForceFeedback(int32_t ff) const = 0;
    /** Returns true if the device has the input property. */
    virtual bool hasInputProperty(int property) const = 0;

    /** Returns the state of the key. */
    virtual int32_t getKeyState(int32_t key) const = 0;
    /** Returns the state of the switch. */
    virtual int32_t getSwitchState(int32_t sw) const = 0;
    /** Returns information about the absolute axis. */
    virtual const AbsoluteAxisInfo* getAbsoluteAxisInfo(int32_t axis) const = 0;
    /** Returns the value of the absolute axis. */
    virtual status_t getAbsoluteAxisValue(int32_t axis, int32_t* outValue) const = 0;

    /** Vibrate the device for duration ns. */
    virtual void vibrate(nsecs_t duration) = 0;
    /** Stop vibration on the device. */
    virtual void cancelVibrate() = 0;

    /** Disable key repeat for the device in the driver. */
    virtual void disableDriverKeyRepeat() = 0;

protected:
    InputDeviceNode() = default;
    virtual ~InputDeviceNode() = default;
};

/** Callback interface for receiving input events, including device changes. */
class InputCallbackInterface {
public:
    virtual void onInputEvent(const std::shared_ptr<InputDeviceNode>& node, InputEvent& event,
            nsecs_t event_time) = 0;
    virtual void onDeviceAdded(const std::shared_ptr<InputDeviceNode>& node) = 0;
    virtual void onDeviceRemoved(const std::shared_ptr<InputDeviceNode>& node) = 0;

protected:
    InputCallbackInterface() = default;
    virtual ~InputCallbackInterface() = default;
};

/**
 * InputHubInterface is responsible for monitoring a set of device paths and
 * executing callbacks when events occur. Before calling poll(), you should set
 * the device and input callbacks, and register your device path(s).
 */
class InputHubInterface {
public:
    virtual status_t registerDevicePath(const std::string& path) = 0;
    virtual status_t unregisterDevicePath(const std::string& path) = 0;

    virtual status_t poll() = 0;
    virtual status_t wake() = 0;

    virtual void dump(String8& dump) = 0;

protected:
    InputHubInterface() = default;
    virtual ~InputHubInterface() = default;
};

/**
 * An implementation of InputHubInterface that uses epoll to wait for events.
 *
 * This class is not threadsafe. Any functions called on the InputHub should be
 * called on the same thread that is used to call poll(). The only exception is
 * wake(), which may be used to return from poll() before an input or device
 * event occurs.
 */
class InputHub : public InputHubInterface {
public:
    explicit InputHub(const std::shared_ptr<InputCallbackInterface>& cb);
    virtual ~InputHub() override;

    virtual status_t registerDevicePath(const std::string& path) override;
    virtual status_t unregisterDevicePath(const std::string& path) override;

    virtual status_t poll() override;
    virtual status_t wake() override;

    virtual void dump(String8& dump) override;

private:
    status_t readNotify();
    status_t scanDir(const std::string& path);
    std::shared_ptr<InputDeviceNode> openNode(const std::string& path);
    status_t closeNode(const InputDeviceNode* node);
    status_t closeNodeByFd(int fd);
    std::shared_ptr<InputDeviceNode> findNodeByPath(const std::string& path);

    enum class WakeMechanism {
        /**
         * The kernel supports the EPOLLWAKEUP flag for epoll_ctl.
         *
         * When using this mechanism, epoll_wait will internally acquire a wake
         * lock whenever one of the FDs it is monitoring becomes ready. The wake
         * lock is held automatically by the kernel until the next call to
         * epoll_wait.
         *
         * This mechanism only exists in Linux kernel 3.5+.
         */
        EPOLL_WAKEUP,
        /**
         * The kernel evdev driver supports the EVIOCSSUSPENDBLOCK ioctl.
         *
         * When using this mechanism, the InputHub asks evdev to acquire and
         * hold a wake lock whenever its buffer is non-empty. We must take care
         * to acquire our own userspace wake lock before draining the buffer to
         * prevent actually going back into suspend before we have fully
         * processed all of the events.
         *
         * This mechanism only exists in older Android Linux kernels.
         */
        LEGACY_EVDEV_SUSPENDBLOCK_IOCTL,
        /**
         * The kernel doesn't seem to support any special wake mechanism.
         *
         * We explicitly acquire and release wake locks when processing input
         * events.
         */
        LEGACY_EVDEV_EXPLICIT_WAKE_LOCKS,
    };
    WakeMechanism mWakeupMechanism = WakeMechanism::LEGACY_EVDEV_EXPLICIT_WAKE_LOCKS;
    bool manageWakeLocks() const;
    bool mNeedToCheckSuspendBlockIoctl = true;

    int mEpollFd;
    int mINotifyFd;
    int mWakeEventFd;

    // Callback for input events
    std::shared_ptr<InputCallbackInterface> mInputCallback;

    // Map from watch descriptors to watched paths
    std::unordered_map<int, std::string> mWatchedPaths;
    // Map from file descriptors to InputDeviceNodes
    std::unordered_map<int, std::shared_ptr<InputDeviceNode>> mDeviceNodes;
};

}  // namespace android

#endif  // ANDROID_INPUT_HUB_H_
