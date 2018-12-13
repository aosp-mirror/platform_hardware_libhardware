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

#define LOG_TAG "InputHub"
//#define LOG_NDEBUG 0

#include "InputHub.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <vector>

#include <android/input.h>
#include <hardware_legacy/power.h>
#include <linux/input.h>

#include <utils/Log.h>

#include "BitUtils.h"

namespace android {

static const char WAKE_LOCK_ID[] = "KeyEvents";
static const int NO_TIMEOUT = -1;
static const int EPOLL_MAX_EVENTS = 16;
static const int INPUT_MAX_EVENTS = 128;

static constexpr bool testBit(int bit, const uint8_t arr[]) {
    return arr[bit / 8] & (1 << (bit % 8));
}

static constexpr size_t sizeofBitArray(size_t bits) {
    return (bits + 7) / 8;
}

static void getLinuxRelease(int* major, int* minor) {
    struct utsname info;
    if (uname(&info) || sscanf(info.release, "%d.%d", major, minor) <= 0) {
        *major = 0, *minor = 0;
        ALOGE("Could not get linux version: %s", strerror(errno));
    }
}

class EvdevDeviceNode : public InputDeviceNode {
public:
    static EvdevDeviceNode* openDeviceNode(const std::string& path);

    virtual ~EvdevDeviceNode() {
        ALOGV("closing %s (fd=%d)", mPath.c_str(), mFd);
        if (mFd >= 0) {
            ::close(mFd);
        }
    }

    virtual int getFd() const { return mFd; }
    virtual const std::string& getPath() const override { return mPath; }
    virtual const std::string& getName() const override { return mName; }
    virtual const std::string& getLocation() const override { return mLocation; }
    virtual const std::string& getUniqueId() const override { return mUniqueId; }

    virtual uint16_t getBusType() const override { return mBusType; }
    virtual uint16_t getVendorId() const override { return mVendorId; }
    virtual uint16_t getProductId() const override { return mProductId; }
    virtual uint16_t getVersion() const override { return mVersion; }

    virtual bool hasKey(int32_t key) const override;
    virtual bool hasKeyInRange(int32_t start, int32_t end) const override;
    virtual bool hasRelativeAxis(int32_t axis) const override;
    virtual bool hasAbsoluteAxis(int32_t axis) const override;
    virtual bool hasSwitch(int32_t sw) const override;
    virtual bool hasForceFeedback(int32_t ff) const override;
    virtual bool hasInputProperty(int property) const override;

    virtual int32_t getKeyState(int32_t key) const override;
    virtual int32_t getSwitchState(int32_t sw) const override;
    virtual const AbsoluteAxisInfo* getAbsoluteAxisInfo(int32_t axis) const override;
    virtual status_t getAbsoluteAxisValue(int32_t axis, int32_t* outValue) const override;

    virtual void vibrate(nsecs_t duration) override;
    virtual void cancelVibrate() override;

    virtual void disableDriverKeyRepeat() override;

private:
    EvdevDeviceNode(const std::string& path, int fd) :
        mFd(fd), mPath(path) {}

    status_t queryProperties();
    void queryAxisInfo();

    int mFd;
    std::string mPath;

    std::string mName;
    std::string mLocation;
    std::string mUniqueId;

    uint16_t mBusType;
    uint16_t mVendorId;
    uint16_t mProductId;
    uint16_t mVersion;

    uint8_t mKeyBitmask[KEY_CNT / 8];
    uint8_t mAbsBitmask[ABS_CNT / 8];
    uint8_t mRelBitmask[REL_CNT / 8];
    uint8_t mSwBitmask[SW_CNT / 8];
    uint8_t mLedBitmask[LED_CNT / 8];
    uint8_t mFfBitmask[FF_CNT / 8];
    uint8_t mPropBitmask[INPUT_PROP_CNT / 8];

    std::unordered_map<uint32_t, std::unique_ptr<AbsoluteAxisInfo>> mAbsInfo;

    bool mFfEffectPlaying = false;
    int16_t mFfEffectId = -1;
};

EvdevDeviceNode* EvdevDeviceNode::openDeviceNode(const std::string& path) {
    auto fd = TEMP_FAILURE_RETRY(::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC));
    if (fd < 0) {
        ALOGE("could not open evdev device %s. err=%d", path.c_str(), errno);
        return nullptr;
    }

    // Tell the kernel that we want to use the monotonic clock for reporting
    // timestamps associated with input events. This is important because the
    // input system uses the timestamps extensively and assumes they were
    // recorded using the monotonic clock.
    //
    // The EVIOCSCLOCKID ioctl was introduced in Linux 3.4.
    int clockId = CLOCK_MONOTONIC;
    if (TEMP_FAILURE_RETRY(ioctl(fd, EVIOCSCLOCKID, &clockId)) < 0) {
        ALOGW("Could not set input clock id to CLOCK_MONOTONIC. errno=%d", errno);
    }

    auto node = new EvdevDeviceNode(path, fd);
    status_t ret = node->queryProperties();
    if (ret != OK) {
        ALOGE("could not open evdev device %s: failed to read properties. errno=%d",
                path.c_str(), ret);
        delete node;
        return nullptr;
    }
    return node;
}

status_t EvdevDeviceNode::queryProperties() {
    char buffer[80];

    if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGNAME(sizeof(buffer) - 1), buffer)) < 1) {
        ALOGV("could not get device name for %s.", mPath.c_str());
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
        mName = buffer;
    }

    int driverVersion;
    if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGVERSION, &driverVersion))) {
        ALOGE("could not get driver version for %s. err=%d", mPath.c_str(), errno);
        return -errno;
    }

    struct input_id inputId;
    if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGID, &inputId))) {
        ALOGE("could not get device input id for %s. err=%d", mPath.c_str(), errno);
        return -errno;
    }
    mBusType = inputId.bustype;
    mVendorId = inputId.vendor;
    mProductId = inputId.product;
    mVersion = inputId.version;

    if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGPHYS(sizeof(buffer) - 1), buffer)) < 1) {
        ALOGV("could not get location for %s.", mPath.c_str());
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
        mLocation = buffer;
    }

    if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGUNIQ(sizeof(buffer) - 1), buffer)) < 1) {
        ALOGV("could not get unique id for %s.", mPath.c_str());
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
        mUniqueId = buffer;
    }

    ALOGV("add device %s", mPath.c_str());
    ALOGV("  bus:        %04x\n"
          "  vendor:     %04x\n"
          "  product:    %04x\n"
          "  version:    %04x\n",
        mBusType, mVendorId, mProductId, mVersion);
    ALOGV("  name:       \"%s\"\n"
          "  location:   \"%s\"\n"
          "  unique_id:  \"%s\"\n"
          "  descriptor: (TODO)\n"
          "  driver:     v%d.%d.%d",
        mName.c_str(), mLocation.c_str(), mUniqueId.c_str(),
        driverVersion >> 16, (driverVersion >> 8) & 0xff, (driverVersion >> 16) & 0xff);

    TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGBIT(EV_KEY, sizeof(mKeyBitmask)), mKeyBitmask));
    TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGBIT(EV_ABS, sizeof(mAbsBitmask)), mAbsBitmask));
    TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGBIT(EV_REL, sizeof(mRelBitmask)), mRelBitmask));
    TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGBIT(EV_SW,  sizeof(mSwBitmask)),  mSwBitmask));
    TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGBIT(EV_LED, sizeof(mLedBitmask)), mLedBitmask));
    TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGBIT(EV_FF,  sizeof(mFfBitmask)),  mFfBitmask));
    TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGPROP(sizeof(mPropBitmask)), mPropBitmask));

    queryAxisInfo();

    return OK;
}

void EvdevDeviceNode::queryAxisInfo() {
    for (int32_t axis = 0; axis < ABS_MAX; ++axis) {
        if (testBit(axis, mAbsBitmask)) {
            struct input_absinfo info;
            if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGABS(axis), &info))) {
                ALOGW("Error reading absolute controller %d for device %s fd %d, errno=%d",
                        axis, mPath.c_str(), mFd, errno);
                continue;
            }

            mAbsInfo[axis] = std::unique_ptr<AbsoluteAxisInfo>(new AbsoluteAxisInfo{
                    .minValue = info.minimum,
                    .maxValue = info.maximum,
                    .flat = info.flat,
                    .fuzz = info.fuzz,
                    .resolution = info.resolution
                    });
        }
    }
}

bool EvdevDeviceNode::hasKey(int32_t key) const {
    if (key >= 0 && key <= KEY_MAX) {
        return testBit(key, mKeyBitmask);
    }
    return false;
}

bool EvdevDeviceNode::hasKeyInRange(int32_t startKey, int32_t endKey) const {
    return testBitInRange(mKeyBitmask, startKey, endKey);
}

bool EvdevDeviceNode::hasRelativeAxis(int axis) const {
    if (axis >= 0 && axis <= REL_MAX) {
        return testBit(axis, mRelBitmask);
    }
    return false;
}

bool EvdevDeviceNode::hasAbsoluteAxis(int axis) const {
    if (axis >= 0 && axis <= ABS_MAX) {
        return getAbsoluteAxisInfo(axis) != nullptr;
    }
    return false;
}

const AbsoluteAxisInfo* EvdevDeviceNode::getAbsoluteAxisInfo(int32_t axis) const {
    if (axis < 0 || axis > ABS_MAX) {
        return nullptr;
    }

    const auto absInfo = mAbsInfo.find(axis);
    if (absInfo != mAbsInfo.end()) {
        return absInfo->second.get();
    }
    return nullptr;
}

bool EvdevDeviceNode::hasSwitch(int32_t sw) const {
    if (sw >= 0 && sw <= SW_MAX) {
        return testBit(sw, mSwBitmask);
    }
    return false;
}

bool EvdevDeviceNode::hasForceFeedback(int32_t ff) const {
    if (ff >= 0 && ff <= FF_MAX) {
        return testBit(ff, mFfBitmask);
    }
    return false;
}

bool EvdevDeviceNode::hasInputProperty(int property) const {
    if (property >= 0 && property <= INPUT_PROP_MAX) {
        return testBit(property, mPropBitmask);
    }
    return false;
}

int32_t EvdevDeviceNode::getKeyState(int32_t key) const {
    if (key >= 0 && key <= KEY_MAX) {
        if (testBit(key, mKeyBitmask)) {
            uint8_t keyState[sizeofBitArray(KEY_CNT)];
            memset(keyState, 0, sizeof(keyState));
            if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGKEY(sizeof(keyState)), keyState)) >= 0) {
                return testBit(key, keyState) ? AKEY_STATE_DOWN : AKEY_STATE_UP;
            }
        }
    }
    return AKEY_STATE_UNKNOWN;
}

int32_t EvdevDeviceNode::getSwitchState(int32_t sw) const {
    if (sw >= 0 && sw <= SW_MAX) {
        if (testBit(sw, mSwBitmask)) {
            uint8_t swState[sizeofBitArray(SW_CNT)];
            memset(swState, 0, sizeof(swState));
            if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGSW(sizeof(swState)), swState)) >= 0) {
                return testBit(sw, swState) ? AKEY_STATE_DOWN : AKEY_STATE_UP;
            }
        }
    }
    return AKEY_STATE_UNKNOWN;
}

status_t EvdevDeviceNode::getAbsoluteAxisValue(int32_t axis, int32_t* outValue) const {
    *outValue = 0;

    if (axis >= 0 && axis <= ABS_MAX) {
        if (testBit(axis, mAbsBitmask)) {
            struct input_absinfo info;
            if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCGABS(axis), &info))) {
                ALOGW("Error reading absolute controller %d for device %s fd %d, errno=%d",
                        axis, mPath.c_str(), mFd, errno);
                return -errno;
            }

            *outValue = info.value;
            return OK;
        }
    }
    return -1;
}

void EvdevDeviceNode::vibrate(nsecs_t duration) {
    ff_effect effect{};
    effect.type = FF_RUMBLE;
    effect.id = mFfEffectId;
    effect.u.rumble.strong_magnitude = 0xc000;
    effect.u.rumble.weak_magnitude = 0xc000;
    effect.replay.length = (duration + 999'999LL) / 1'000'000LL;
    effect.replay.delay = 0;
    if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCSFF, &effect))) {
        ALOGW("Could not upload force feedback effect to device %s due to error %d.",
                mPath.c_str(), errno);
        return;
    }
    mFfEffectId = effect.id;

    struct input_event ev{};
    ev.type = EV_FF;
    ev.code = mFfEffectId;
    ev.value = 1;
    size_t written = TEMP_FAILURE_RETRY(write(mFd, &ev, sizeof(ev)));
    if (written != sizeof(ev)) {
        ALOGW("Could not start force feedback effect on device %s due to error %d.",
                mPath.c_str(), errno);
        return;
    }
    mFfEffectPlaying = true;
}

void EvdevDeviceNode::cancelVibrate() {
    if (mFfEffectPlaying) {
        mFfEffectPlaying = false;

        struct input_event ev{};
        ev.type = EV_FF;
        ev.code = mFfEffectId;
        ev.value = 0;
        size_t written = TEMP_FAILURE_RETRY(write(mFd, &ev, sizeof(ev)));
        if (written != sizeof(ev)) {
            ALOGW("Could not stop force feedback effect on device %s due to error %d.",
                    mPath.c_str(), errno);
            return;
        }
    }
}

void EvdevDeviceNode::disableDriverKeyRepeat() {
    unsigned int repeatRate[] = {0, 0};
    if (TEMP_FAILURE_RETRY(ioctl(mFd, EVIOCSREP, repeatRate))) {
        ALOGW("Unable to disable kernel key repeat for %s due to error %d.",
                mPath.c_str(), errno);
    }
}

InputHub::InputHub(const std::shared_ptr<InputCallbackInterface>& cb) :
    mInputCallback(cb) {
    // Determine the type of suspend blocking we can do on this device. There
    // are 3 options, in decreasing order of preference:
    //   1) EPOLLWAKEUP: introduced in Linux kernel 3.5, this flag can be set on
    //   an epoll event to indicate that a wake lock should be held from the
    //   time an fd has data until the next epoll_wait (or the epoll fd is
    //   closed).
    //   2) EVIOCSSUSPENDBLOCK: introduced into the Android kernel's evdev
    //   driver, this ioctl blocks suspend while the event queue for the fd is
    //   not empty. This was never accepted into the mainline kernel, and it was
    //   replaced by EPOLLWAKEUP.
    //   3) explicit wake locks: use acquire_wake_lock to manage suspend
    //   blocking explicitly in the InputHub code.
    //
    // (1) can be checked by simply observing the Linux kernel version. (2)
    // requires an fd from an evdev node, which cannot be done in the InputHub
    // constructor. So we assume (3) unless (1) is true, and we can verify
    // whether (2) is true once we have an evdev fd (and we're not in (1)).
    int major, minor;
    getLinuxRelease(&major, &minor);
    if (major > 3 || (major == 3 && minor >= 5)) {
        ALOGI("Using EPOLLWAKEUP to block suspend while processing input events.");
        mWakeupMechanism = WakeMechanism::EPOLL_WAKEUP;
        mNeedToCheckSuspendBlockIoctl = false;
    }
    if (manageWakeLocks()) {
        acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);
    }

    // epoll_create argument is ignored, but it must be > 0.
    mEpollFd = epoll_create(1);
    LOG_ALWAYS_FATAL_IF(mEpollFd < 0, "Could not create epoll instance. errno=%d", errno);

    mINotifyFd = inotify_init();
    LOG_ALWAYS_FATAL_IF(mINotifyFd < 0, "Could not create inotify instance. errno=%d", errno);

    struct epoll_event eventItem;
    memset(&eventItem, 0, sizeof(eventItem));
    eventItem.events = EPOLLIN;
    if (mWakeupMechanism == WakeMechanism::EPOLL_WAKEUP) {
        eventItem.events |= EPOLLWAKEUP;
    }
    eventItem.data.u32 = mINotifyFd;
    int result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mINotifyFd, &eventItem);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not add INotify to epoll instance. errno=%d", errno);

    int wakeFds[2];
    result = pipe(wakeFds);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not create wake pipe. errno=%d", errno);

    mWakeEventFd = eventfd(0, EFD_NONBLOCK);
    LOG_ALWAYS_FATAL_IF(mWakeEventFd == -1, "Could not create wake event fd. errno=%d", errno);

    eventItem.data.u32 = mWakeEventFd;
    result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mWakeEventFd, &eventItem);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not add wake event fd to epoll instance. errno=%d", errno);
}

InputHub::~InputHub() {
    ::close(mEpollFd);
    ::close(mINotifyFd);
    ::close(mWakeEventFd);

    if (manageWakeLocks()) {
        release_wake_lock(WAKE_LOCK_ID);
    }
}

status_t InputHub::registerDevicePath(const std::string& path) {
    ALOGV("registering device path %s", path.c_str());
    int wd = inotify_add_watch(mINotifyFd, path.c_str(), IN_DELETE | IN_CREATE);
    if (wd < 0) {
        ALOGE("Could not add %s to INotify watch. errno=%d", path.c_str(), errno);
        return -errno;
    }
    mWatchedPaths[wd] = path;
    scanDir(path);
    return OK;
}

status_t InputHub::unregisterDevicePath(const std::string& path) {
    int wd = -1;
    for (const auto& pair : mWatchedPaths) {
        if (pair.second == path) {
            wd = pair.first;
            break;
        }
    }

    if (wd == -1) {
        return BAD_VALUE;
    }
    mWatchedPaths.erase(wd);
    if (inotify_rm_watch(mINotifyFd, wd) != 0) {
        return -errno;
    }
    return OK;
}

status_t InputHub::poll() {
    bool deviceChange = false;

    if (manageWakeLocks()) {
        // Mind the wake lock dance!
        // If we're relying on wake locks, we hold a wake lock at all times
        // except during epoll_wait(). This works due to some subtle
        // choreography. When a device driver has pending (unread) events, it
        // acquires a kernel wake lock. However, once the last pending event
        // has been read, the device driver will release the kernel wake lock.
        // To prevent the system from going to sleep when this happens, the
        // InputHub holds onto its own user wake lock while the client is
        // processing events. Thus the system can only sleep if there are no
        // events pending or currently being processed.
        release_wake_lock(WAKE_LOCK_ID);
    }

    struct epoll_event pendingEventItems[EPOLL_MAX_EVENTS];
    int pollResult = epoll_wait(mEpollFd, pendingEventItems, EPOLL_MAX_EVENTS, NO_TIMEOUT);

    if (manageWakeLocks()) {
        acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);
    }

    if (pollResult == 0) {
        ALOGW("epoll_wait should not return 0 with no timeout");
        return UNKNOWN_ERROR;
    }
    if (pollResult < 0) {
        // An error occurred. Return even if it's EINTR, and let the caller
        // restart the poll.
        ALOGE("epoll_wait returned with errno=%d", errno);
        return -errno;
    }

    // pollResult > 0: there are events to process
    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    std::vector<int> removedDeviceFds;
    int inputFd = -1;
    std::shared_ptr<InputDeviceNode> deviceNode;
    for (int i = 0; i < pollResult; ++i) {
        const struct epoll_event& eventItem = pendingEventItems[i];

        int dataFd = static_cast<int>(eventItem.data.u32);
        if (dataFd == mINotifyFd) {
            if (eventItem.events & EPOLLIN) {
                deviceChange = true;
            } else {
                ALOGW("Received unexpected epoll event 0x%08x for INotify.", eventItem.events);
            }
            continue;
        }

        if (dataFd == mWakeEventFd) {
            if (eventItem.events & EPOLLIN) {
                ALOGV("awoken after wake()");
                uint64_t u;
                ssize_t nRead = TEMP_FAILURE_RETRY(read(mWakeEventFd, &u, sizeof(uint64_t)));
                if (nRead != sizeof(uint64_t)) {
                    ALOGW("Could not read event fd; waking anyway.");
                }
            } else {
                ALOGW("Received unexpected epoll event 0x%08x for wake event.",
                        eventItem.events);
            }
            continue;
        }

        // Update the fd and device node when the fd changes. When several
        // events are read back-to-back with the same fd, this saves many reads
        // from the hash table.
        if (inputFd != dataFd) {
            inputFd = dataFd;
            deviceNode = mDeviceNodes[inputFd];
        }
        if (deviceNode == nullptr) {
            ALOGE("could not find device node for fd %d", inputFd);
            continue;
        }
        if (eventItem.events & EPOLLIN) {
            struct input_event ievs[INPUT_MAX_EVENTS];
            for (;;) {
                ssize_t readSize = TEMP_FAILURE_RETRY(read(inputFd, ievs, sizeof(ievs)));
                if (readSize == 0 || (readSize < 0 && errno == ENODEV)) {
                    ALOGW("could not get event, removed? (fd: %d, size: %zd errno: %d)",
                            inputFd, readSize, errno);

                    removedDeviceFds.push_back(inputFd);
                    break;
                } else if (readSize < 0) {
                    if (errno != EAGAIN && errno != EINTR) {
                        ALOGW("could not get event. errno=%d", errno);
                    }
                    break;
                } else if (readSize % sizeof(input_event) != 0) {
                    ALOGE("could not get event. wrong size=%zd", readSize);
                    break;
                } else {
                    size_t count = static_cast<size_t>(readSize) / sizeof(struct input_event);
                    for (size_t i = 0; i < count; ++i) {
                        auto& iev = ievs[i];
                        auto when = s2ns(iev.time.tv_sec) + us2ns(iev.time.tv_usec);
                        InputEvent inputEvent = { when, iev.type, iev.code, iev.value };
                        mInputCallback->onInputEvent(deviceNode, inputEvent, now);
                    }
                }
            }
        } else if (eventItem.events & EPOLLHUP) {
            ALOGI("Removing device fd %d due to epoll hangup event.", inputFd);
            removedDeviceFds.push_back(inputFd);
        } else {
            ALOGW("Received unexpected epoll event 0x%08x for device fd %d",
                    eventItem.events, inputFd);
        }
    }

    if (removedDeviceFds.size()) {
        for (auto deviceFd : removedDeviceFds) {
            auto deviceNode = mDeviceNodes[deviceFd];
            if (deviceNode != nullptr) {
                status_t ret = closeNodeByFd(deviceFd);
                if (ret != OK) {
                    ALOGW("Could not close device with fd %d. errno=%d", deviceFd, ret);
                } else {
                    mInputCallback->onDeviceRemoved(deviceNode);
                }
            }
        }
    }

    if (deviceChange) {
        readNotify();
    }

    return OK;
}

status_t InputHub::wake() {
    ALOGV("wake() called");

    uint64_t u = 1;
    ssize_t nWrite = TEMP_FAILURE_RETRY(write(mWakeEventFd, &u, sizeof(uint64_t)));

    if (nWrite != sizeof(uint64_t) && errno != EAGAIN) {
        ALOGW("Could not write wake signal, errno=%d", errno);
        return -errno;
    }
    return OK;
}

void InputHub::dump(String8& dump) {
    // TODO
}

status_t InputHub::readNotify() {
    char event_buf[512];
    struct inotify_event* event;

    ssize_t res = TEMP_FAILURE_RETRY(read(mINotifyFd, event_buf, sizeof(event_buf)));
    if (res < static_cast<int>(sizeof(*event))) {
        ALOGW("could not get inotify event, %s\n", strerror(errno));
        return -errno;
    }

    size_t event_pos = 0;
    while (res >= static_cast<int>(sizeof(*event))) {
        event = reinterpret_cast<struct inotify_event*>(event_buf + event_pos);
        if (event->len) {
            std::string path = mWatchedPaths[event->wd];
            path.append("/").append(event->name);
            ALOGV("inotify event for path %s", path.c_str());

            if (event->mask & IN_CREATE) {
                auto deviceNode = openNode(path);
                if (deviceNode == nullptr) {
                    ALOGE("could not open device node %s. err=%zd", path.c_str(), res);
                } else {
                    mInputCallback->onDeviceAdded(deviceNode);
                }
            } else {
                auto deviceNode = findNodeByPath(path);
                if (deviceNode != nullptr) {
                    status_t ret = closeNode(deviceNode.get());
                    if (ret != OK) {
                        ALOGW("Could not close device %s. errno=%d", path.c_str(), ret);
                    } else {
                        mInputCallback->onDeviceRemoved(deviceNode);
                    }
                } else {
                    ALOGW("could not find device node for %s", path.c_str());
                }
            }
        }
        int event_size = sizeof(*event) + event->len;
        res -= event_size;
        event_pos += event_size;
    }

    return OK;
}

status_t InputHub::scanDir(const std::string& path) {
    auto dir = ::opendir(path.c_str());
    if (dir == nullptr) {
        ALOGE("could not open device path %s to scan for devices. err=%d", path.c_str(), errno);
        return -errno;
    }

    while (auto dirent = readdir(dir)) {
        if (strcmp(dirent->d_name, ".") == 0 ||
            strcmp(dirent->d_name, "..") == 0) {
            continue;
        }
        std::string filename = path + "/" + dirent->d_name;
        auto node = openNode(filename);
        if (node == nullptr) {
            ALOGE("could not open device node %s", filename.c_str());
        } else {
            mInputCallback->onDeviceAdded(node);
        }
    }
    ::closedir(dir);
    return OK;
}

std::shared_ptr<InputDeviceNode> InputHub::openNode(const std::string& path) {
    ALOGV("opening %s...", path.c_str());
    auto evdevNode = std::shared_ptr<EvdevDeviceNode>(EvdevDeviceNode::openDeviceNode(path));
    if (evdevNode == nullptr) {
        return nullptr;
    }

    auto fd = evdevNode->getFd();
    ALOGV("opened %s with fd %d", path.c_str(), fd);
    mDeviceNodes[fd] = evdevNode;
    struct epoll_event eventItem{};
    eventItem.events = EPOLLIN;
    if (mWakeupMechanism == WakeMechanism::EPOLL_WAKEUP) {
        eventItem.events |= EPOLLWAKEUP;
    }
    eventItem.data.u32 = fd;
    if (epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &eventItem)) {
        ALOGE("Could not add device fd to epoll instance. errno=%d", errno);
        return nullptr;
    }

    if (mNeedToCheckSuspendBlockIoctl) {
#ifndef EVIOCSSUSPENDBLOCK
        // uapi headers don't include EVIOCSSUSPENDBLOCK, and future kernels
        // will use an epoll flag instead, so as long as we want to support this
        // feature, we need to be prepared to define the ioctl ourselves.
#define EVIOCSSUSPENDBLOCK _IOW('E', 0x91, int)
#endif
        if (TEMP_FAILURE_RETRY(ioctl(fd, EVIOCSSUSPENDBLOCK, 1))) {
            // no wake mechanism, continue using explicit wake locks
            ALOGI("Using explicit wakelocks to block suspend while processing input events.");
        } else {
            mWakeupMechanism = WakeMechanism::LEGACY_EVDEV_SUSPENDBLOCK_IOCTL;
            // release any held wakelocks since we won't need them anymore
            release_wake_lock(WAKE_LOCK_ID);
            ALOGI("Using EVIOCSSUSPENDBLOCK to block suspend while processing input events.");
        }
        mNeedToCheckSuspendBlockIoctl = false;
    }

    return evdevNode;
}

status_t InputHub::closeNode(const InputDeviceNode* node) {
    for (const auto& pair : mDeviceNodes) {
        if (pair.second.get() == node) {
            return closeNodeByFd(pair.first);
        }
    }
    return BAD_VALUE;
}

status_t InputHub::closeNodeByFd(int fd) {
    status_t ret = OK;
    if (epoll_ctl(mEpollFd, EPOLL_CTL_DEL, fd, NULL)) {
        ALOGW("Could not remove device fd from epoll instance. errno=%d", errno);
        ret = -errno;
    }
    mDeviceNodes.erase(fd);
    ::close(fd);
    return ret;
}

std::shared_ptr<InputDeviceNode> InputHub::findNodeByPath(const std::string& path) {
    for (const auto& pair : mDeviceNodes) {
        if (pair.second->getPath() == path) return pair.second;
    }
    return nullptr;
}

bool InputHub::manageWakeLocks() const {
    return mWakeupMechanism != WakeMechanism::EPOLL_WAKEUP;
}

}  // namespace android
