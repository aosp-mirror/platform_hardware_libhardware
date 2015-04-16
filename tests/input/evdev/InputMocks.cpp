#include "InputMocks.h"

// Private test definitions of opaque HAL structs

// Not used
struct input_property_map {};

// Holds the key and value from the mock host's PropertyMap
struct input_property {
    android::String8 key;
    android::String8 value;
};

namespace android {

bool MockInputHost::checkAllocations() const {
    bool ret = true;
    if (mMapAllocations != 0) {
        ALOGE("Leaked %d device property map allocations", mMapAllocations);
        ret = false;
    }
    for (auto entry : mPropertyAllocations) {
        if (entry.second != 0) {
            ALOGE("Leaked %d property allocation for %s", entry.second, entry.first.c_str());
            ret = false;
        }
    }
    return ret;
}

input_device_identifier_t* MockInputHost::createDeviceIdentifier(
        const char* name, int32_t product_id, int32_t vendor_id,
        input_bus_t bus, const char* unique_id) {
    mDeviceId.reset(new input_device_identifier_t{
            .name = name,
            .productId = product_id,
            .vendorId = vendor_id,
            .bus = bus,
            .uniqueId = unique_id
            });
    // Just return the raw pointer. We don't have a method for deallocating
    // device identifiers yet, and they should exist throughout the lifetime of
    // the input process for now.
    return mDeviceId.get();
}

input_property_map_t* MockInputHost::getDevicePropertyMap(input_device_identifier_t* id) {
    mMapAllocations++;
    // Handled in the MockInputHost.
    return nullptr;
}

input_property_t* MockInputHost::getDeviceProperty(input_property_map_t* map, const char* key) {
    mPropertyAllocations[key]++;
    return new input_property_t{.key = String8(key)};
}

const char* MockInputHost::getPropertyKey(input_property_t* property) {
    return property->key.string();
}

const char* MockInputHost::getPropertyValue(input_property_t* property) {
    if (!mDevicePropertyMap.tryGetProperty(property->key, property->value)) {
        return nullptr;
    }
    return property->value.string();
}

void MockInputHost::freeDeviceProperty(input_property_t* property) {
    if (property != nullptr) {
        mPropertyAllocations[property->key.string()]--;
        delete property;
    }
}

void MockInputHost::freeDevicePropertyMap(input_property_map_t* map) {
    mMapAllocations--;
}

input_host_callbacks_t kTestCallbacks = {
    .create_device_identifier = create_device_identifier,
    .create_device_definition = create_device_definition,
    .create_input_report_definition = create_input_report_definition,
    .create_output_report_definition = create_output_report_definition,
    .input_device_definition_add_report = input_device_definition_add_report,
    .input_report_definition_add_collection = input_report_definition_add_collection,
    .input_report_definition_declare_usage_int = input_report_definition_declare_usage_int,
    .input_report_definition_declare_usages_bool = input_report_definition_declare_usages_bool,
    .register_device = register_device,
    .input_allocate_report = input_allocate_report,
    .input_report_set_usage_int = input_report_set_usage_int,
    .input_report_set_usage_bool = input_report_set_usage_bool,
    .report_event = report_event,
    .input_get_device_property_map = input_get_device_property_map,
    .input_get_device_property = input_get_device_property,
    .input_get_property_key = input_get_property_key,
    .input_get_property_value = input_get_property_value,
    .input_free_device_property = input_free_device_property,
    .input_free_device_property_map = input_free_device_property_map,
};

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

::input_device_identifier_t* create_device_identifier(input_host_t* host,
        const char* name, int32_t product_id, int32_t vendor_id,
        input_bus_t bus, const char* unique_id) {
    auto mockHost = static_cast<MockInputHost*>(host);
    return mockHost->createDeviceIdentifier(name, product_id, vendor_id, bus, unique_id);
}

input_device_definition_t* create_device_definition(input_host_t* host) {
    return nullptr;
}

input_report_definition_t* create_input_report_definition(input_host_t* host) {
    return nullptr;
}

input_report_definition_t* create_output_report_definition(input_host_t* host) {
    return nullptr;
}

void input_device_definition_add_report(input_host_t* host,
        input_device_definition_t* d, input_report_definition_t* r) { }

void input_report_definition_add_collection(input_host_t* host,
        input_report_definition_t* report, input_collection_id_t id, int32_t arity) { }

void input_report_definition_declare_usage_int(input_host_t* host,
        input_report_definition_t* report, input_collection_id_t id,
        input_usage_t usage, int32_t min, int32_t max, float resolution) { }

void input_report_definition_declare_usages_bool(input_host_t* host,
        input_report_definition_t* report, input_collection_id_t id,
        input_usage_t* usage, size_t usage_count) { }


input_device_handle_t* register_device(input_host_t* host,
        input_device_identifier_t* id, input_device_definition_t* d) {
    return nullptr;
}

input_report_t* input_allocate_report(input_host_t* host, input_report_definition_t* r) {
    return nullptr;
}
void input_report_set_usage_int(input_host_t* host, input_report_t* r,
        input_collection_id_t id, input_usage_t usage, int32_t value, int32_t arity_index) { }

void input_report_set_usage_bool(input_host_t* host, input_report_t* r,
        input_collection_id_t id, input_usage_t usage, bool value, int32_t arity_index) { }

void report_event(input_host_t* host, input_device_handle_t* d, input_report_t* report) { }

input_property_map_t* input_get_device_property_map(input_host_t* host,
        input_device_identifier_t* id) {
    auto mockHost = static_cast<MockInputHost*>(host);
    return mockHost->getDevicePropertyMap(id);
}

input_property_t* input_get_device_property(input_host_t* host, input_property_map_t* map,
        const char* key) {
    auto mockHost = static_cast<MockInputHost*>(host);
    return mockHost->getDeviceProperty(map, key);
}

const char* input_get_property_key(input_host_t* host, input_property_t* property) {
    auto mockHost = static_cast<MockInputHost*>(host);
    return mockHost->getPropertyKey(property);
}

const char* input_get_property_value(input_host_t* host, input_property_t* property) {
    auto mockHost = static_cast<MockInputHost*>(host);
    return mockHost->getPropertyValue(property);
}

void input_free_device_property(input_host_t* host, input_property_t* property) {
    auto mockHost = static_cast<MockInputHost*>(host);
    return mockHost->freeDeviceProperty(property);
}

void input_free_device_property_map(input_host_t* host, input_property_map_t* map) {
    auto mockHost = static_cast<MockInputHost*>(host);
    return mockHost->freeDevicePropertyMap(map);
}

}  // namespace android
