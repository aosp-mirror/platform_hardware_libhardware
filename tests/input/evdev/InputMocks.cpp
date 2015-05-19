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

#include "InputMocks.h"

namespace android {

bool MockInputDeviceNode::hasKeyInRange(int32_t startKey, int32_t endKey) const {
    auto iter = mKeys.lower_bound(startKey);
    if (iter == mKeys.end()) return false;
    return *iter < endKey;
}

namespace MockNexus7v2 {

MockInputDeviceNode* getElanTouchscreen() {
    auto node = new MockInputDeviceNode();
    node->setPath("/dev/input/event0");
    node->setName("elan-touchscreen");
    // Location not set
    // UniqueId not set
    node->setBusType(0);
    node->setVendorId(0);
    node->setProductId(0);
    node->setVersion(0);
    // No keys
    // No relative axes
    // TODO: set the AbsoluteAxisInfo pointers
    node->addAbsAxis(ABS_MT_SLOT, nullptr);
    node->addAbsAxis(ABS_MT_TOUCH_MAJOR, nullptr);
    node->addAbsAxis(ABS_MT_POSITION_X, nullptr);
    node->addAbsAxis(ABS_MT_POSITION_Y, nullptr);
    node->addAbsAxis(ABS_MT_TRACKING_ID, nullptr);
    node->addAbsAxis(ABS_MT_PRESSURE, nullptr);
    // No switches
    // No forcefeedback
    node->addInputProperty(INPUT_PROP_DIRECT);
    return node;
}

MockInputDeviceNode* getLidInput() {
    auto node = new MockInputDeviceNode();
    node->setPath("/dev/input/event1");
    node->setName("lid_input");
    node->setLocation("/dev/input/lid_indev");
    // UniqueId not set
    node->setBusType(0);
    node->setVendorId(0);
    node->setProductId(0);
    node->setVersion(0);
    // No keys
    // No relative axes
    // No absolute axes
    node->addSwitch(SW_LID);
    // No forcefeedback
    node->addInputProperty(INPUT_PROP_DIRECT);
    return node;
}

MockInputDeviceNode* getButtonJack() {
    auto node = new MockInputDeviceNode();
    node->setPath("/dev/input/event2");
    node->setName("apq8064-tabla-snd-card Button Jack");
    node->setLocation("ALSA");
    // UniqueId not set
    node->setBusType(0);
    node->setVendorId(0);
    node->setProductId(0);
    node->setVersion(0);
    node->addKeys(BTN_0, BTN_1, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6, BTN_7);
    // No relative axes
    // No absolute axes
    // No switches
    // No forcefeedback
    node->addInputProperty(INPUT_PROP_DIRECT);
    return node;
}

MockInputDeviceNode* getHeadsetJack() {
    auto node = new MockInputDeviceNode();
    node->setPath("/dev/input/event3");
    node->setName("apq8064-tabla-snd-card Headset Jack");
    node->setLocation("ALSA");
    // UniqueId not set
    node->setBusType(0);
    node->setVendorId(0);
    node->setProductId(0);
    node->setVersion(0);
    // No keys
    // No relative axes
    // No absolute axes
    node->addSwitch(SW_HEADPHONE_INSERT);
    node->addSwitch(SW_MICROPHONE_INSERT);
    node->addSwitch(SW_LINEOUT_INSERT);
    // ASUS adds some proprietary switches, but we'll only see two of them.
    node->addSwitch(0x0e);  // SW_HPHL_OVERCURRENT
    node->addSwitch(0x0f);  // SW_HPHR_OVERCURRENT
    // No forcefeedback
    node->addInputProperty(INPUT_PROP_DIRECT);
    return node;
}

MockInputDeviceNode* getH2wButton() {
    auto node = new MockInputDeviceNode();
    node->setPath("/dev/input/event4");
    node->setName("h2w button");
    // Location not set
    // UniqueId not set
    node->setBusType(0);
    node->setVendorId(0);
    node->setProductId(0);
    node->setVersion(0);
    node->addKeys(KEY_MEDIA);
    // No relative axes
    // No absolute axes
    // No switches
    node->addInputProperty(INPUT_PROP_DIRECT);
    return node;
}

MockInputDeviceNode* getGpioKeys() {
    auto node = new MockInputDeviceNode();
    node->setPath("/dev/input/event5");
    node->setName("gpio-keys");
    node->setLocation("gpio-keys/input0");
    // UniqueId not set
    node->setBusType(0x0019);
    node->setVendorId(0x0001);
    node->setProductId(0x0001);
    node->setVersion(0x0100);
    node->addKeys(KEY_VOLUMEDOWN, KEY_VOLUMEUP, KEY_POWER);
    // No relative axes
    // No absolute axes
    // No switches
    node->addInputProperty(INPUT_PROP_DIRECT);
    return node;
}

}  // namespace MockNexus7v2

namespace MockNexusPlayer {

MockInputDeviceNode* getGpioKeys() {
    auto node = new MockInputDeviceNode();
    node->setPath("/dev/input/event0");
    node->setName("gpio-keys");
    node->setLocation("gpio-keys/input0");
    // UniqueId not set
    node->setBusType(0x0019);
    node->setVendorId(0x0001);
    node->setProductId(0x0001);
    node->setVersion(0x0100);
    node->addKeys(KEY_CONNECT);
    // No relative axes
    // No absolute axes
    // No switches
    node->addInputProperty(INPUT_PROP_DIRECT);
    return node;
}

MockInputDeviceNode* getMidPowerBtn() {
    auto node = new MockInputDeviceNode();
    node->setPath("/dev/input/event1");
    node->setName("mid_powerbtn");
    node->setLocation("power-button/input0");
    // UniqueId not set
    node->setBusType(0x0019);
    node->setVendorId(0);
    node->setProductId(0);
    node->setVersion(0);
    node->addKeys(KEY_POWER);
    // No relative axes
    // No absolute axes
    // No switches
    node->addInputProperty(INPUT_PROP_DIRECT);
    return node;
}

MockInputDeviceNode* getNexusRemote() {
    auto node = new MockInputDeviceNode();
    node->setPath("/dev/input/event2");
    node->setName("Nexus Remote");
    // Location not set
    node->setUniqueId("78:86:D9:50:A0:54");
    node->setBusType(0x0005);
    node->setVendorId(0x18d1);
    node->setProductId(0x2c42);
    node->setVersion(0);
    node->addKeys(KEY_UP, KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_BACK, KEY_PLAYPAUSE,
            KEY_HOMEPAGE, KEY_SEARCH, KEY_SELECT);
    // No relative axes
    node->addAbsAxis(ABS_MISC, nullptr);
    // No switches
    node->addInputProperty(INPUT_PROP_DIRECT);
    return node;
}

MockInputDeviceNode* getAsusGamepad() {
    auto node = new MockInputDeviceNode();
    node->setPath("/dev/input/event3");
    node->setName("ASUS Gamepad");
    // Location not set
    node->setUniqueId("C5:30:CD:50:A0:54");
    node->setBusType(0x0005);
    node->setVendorId(0x0b05);
    node->setProductId(0x4500);
    node->setVersion(0x0040);
    node->addKeys(KEY_BACK, KEY_HOMEPAGE, BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR,
            BTN_MODE, BTN_THUMBL, BTN_THUMBR);
    // No relative axes
    node->addAbsAxis(ABS_X, nullptr);
    node->addAbsAxis(ABS_Y, nullptr);
    node->addAbsAxis(ABS_Z, nullptr);
    node->addAbsAxis(ABS_RZ, nullptr);
    node->addAbsAxis(ABS_GAS, nullptr);
    node->addAbsAxis(ABS_BRAKE, nullptr);
    node->addAbsAxis(ABS_HAT0X, nullptr);
    node->addAbsAxis(ABS_HAT0Y, nullptr);
    node->addAbsAxis(ABS_MISC, nullptr);
    node->addAbsAxis(0x29, nullptr);
    node->addAbsAxis(0x2a, nullptr);
    // No switches
    node->addInputProperty(INPUT_PROP_DIRECT);
    // Note: this device has MSC and LED bitmaps as well.
    return node;
}

}  // namespace MockNexusPlayer

}  // namespace android
