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

#define LOG_TAG "EvdevModule"
//#define LOG_NDEBUG 0

#include <memory>
#include <string>
#include <thread>

#include <assert.h>
#include <hardware/hardware.h>
#include <hardware/input.h>

#include <utils/Log.h>

#include "InputHub.h"
#include "InputDeviceManager.h"
#include "InputHost.h"

namespace android {

static const char kDevInput[] = "/dev/input";

class EvdevModule {
public:
    // Takes ownership of the InputHostInterface
    explicit EvdevModule(InputHostInterface* inputHost);

    void init();
    void notifyReport(input_report_t* r);

private:
    void loop();

    std::unique_ptr<InputHostInterface> mInputHost;
    std::shared_ptr<InputDeviceManager> mDeviceManager;
    std::unique_ptr<InputHub> mInputHub;
    std::thread mPollThread;
};

static std::unique_ptr<EvdevModule> gEvdevModule;

EvdevModule::EvdevModule(InputHostInterface* inputHost) :
    mInputHost(inputHost),
    mDeviceManager(std::make_shared<InputDeviceManager>(mInputHost.get())),
    mInputHub(std::make_unique<InputHub>(mDeviceManager)) {}

void EvdevModule::init() {
    ALOGV("%s", __func__);

    mInputHub->registerDevicePath(kDevInput);
    mPollThread = std::thread(&EvdevModule::loop, this);
}

void EvdevModule::notifyReport(input_report_t* r) {
    ALOGV("%s", __func__);

    // notifyReport() will be called from an arbitrary thread within the input
    // host. Since InputHub is not threadsafe, this is how I expect this to
    // work:
    //   * notifyReport() will queue up the output report in the EvdevModule and
    //     call wake() on the InputHub.
    //   * In the main loop thread, after returning from poll(), the queue will
    //     be processed with any pending work.
}

void EvdevModule::loop() {
    ALOGV("%s", __func__);
    for (;;) {
        mInputHub->poll();

        // TODO: process any pending work, like notify reports
    }
}

extern "C" {

static int dummy_open(const hw_module_t __unused *module, const char __unused *id,
        hw_device_t __unused **device) {
    ALOGW("open not implemented in the input HAL!");
    return 0;
}

static void input_init(const input_module_t* module,
        input_host_t* host, input_host_callbacks_t cb) {
    LOG_ALWAYS_FATAL_IF(strcmp(module->common.id, INPUT_HARDWARE_MODULE_ID) != 0);
    auto inputHost = new InputHost(host, cb);
    gEvdevModule = std::make_unique<EvdevModule>(inputHost);
    gEvdevModule->init();
}

static void input_notify_report(const input_module_t* module, input_report_t* r) {
    LOG_ALWAYS_FATAL_IF(strcmp(module->common.id, INPUT_HARDWARE_MODULE_ID) != 0);
    LOG_ALWAYS_FATAL_IF(gEvdevModule == nullptr);
    gEvdevModule->notifyReport(r);
}

static struct hw_module_methods_t input_module_methods = {
    .open = dummy_open,
};

input_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = INPUT_MODULE_API_VERSION_1_0,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = INPUT_HARDWARE_MODULE_ID,
        .name               = "Input evdev HAL",
        .author             = "The Android Open Source Project",
        .methods            = &input_module_methods,
        .dso                = NULL,
        .reserved           = {0},
    },

    .init = input_init,
    .notify_report = input_notify_report,
};

}  // extern "C"

}  // namespace input
