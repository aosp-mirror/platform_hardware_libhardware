/*
 * Copyright 2016 The Android Open Source Project
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

// Modified from hardware/libhardware/modules/camera/CameraHAL.cpp

//#define LOG_NDEBUG 0
#define LOG_TAG "V4L2CameraHAL"

#include "v4l2_camera_hal.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <unordered_set>

#include <android-base/parseint.h>

#include "common.h"
#include "v4l2_camera.h"

/*
 * This file serves as the entry point to the HAL. It is modified from the
 * example default HAL available in hardware/libhardware/modules/camera.
 * It contains the module structure and functions used by the framework
 * to load and interface to this HAL, as well as the handles to the individual
 * camera devices.
 */

namespace v4l2_camera_hal {

// Default global camera hal.
static V4L2CameraHAL gCameraHAL;

V4L2CameraHAL::V4L2CameraHAL() : mCameras(), mCallbacks(NULL) {
  HAL_LOG_ENTER();
  // Adds all available V4L2 devices.
  // List /dev nodes.
  DIR* dir = opendir("/dev");
  if (dir == NULL) {
    HAL_LOGE("Failed to open /dev");
    return;
  }
  // Find /dev/video* nodes.
  dirent* ent;
  std::vector<std::string> nodes;
  while ((ent = readdir(dir))) {
    std::string desired = "video";
    size_t len = desired.size();
    if (strncmp(desired.c_str(), ent->d_name, len) == 0) {
      if (strlen(ent->d_name) > len && isdigit(ent->d_name[len])) {
        // ent is a numbered video node.
        nodes.push_back(std::string("/dev/") + ent->d_name);
        HAL_LOGV("Found video node %s.", nodes.back().c_str());
      }
    }
  }
  // Test each for V4L2 support and uniqueness.
  std::unordered_set<std::string> buses;
  std::string bus;
  v4l2_capability cap;
  int fd;
  int id = 0;
  for (const auto& node : nodes) {
    // Open the node.
    fd = TEMP_FAILURE_RETRY(open(node.c_str(), O_RDWR));
    if (fd < 0) {
      HAL_LOGE("failed to open %s (%s).", node.c_str(), strerror(errno));
      continue;
    }
    // Read V4L2 capabilities.
    if (TEMP_FAILURE_RETRY(ioctl(fd, VIDIOC_QUERYCAP, &cap)) != 0) {
      HAL_LOGE(
          "VIDIOC_QUERYCAP on %s fail: %s.", node.c_str(), strerror(errno));
    } else if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
      HAL_LOGE("%s is not a V4L2 video capture device.", node.c_str());
    } else {
      // If the node is unique, add a camera for it.
      bus = reinterpret_cast<char*>(cap.bus_info);
      if (buses.insert(bus).second) {
        HAL_LOGV("Found unique bus at %s.", node.c_str());
        std::unique_ptr<V4L2Camera> cam(V4L2Camera::NewV4L2Camera(id++, node));
        if (cam) {
          mCameras.push_back(std::move(cam));
        } else {
          HAL_LOGE("Failed to initialize camera at %s.", node.c_str());
        }
      }
    }
    close(fd);
  }
}

V4L2CameraHAL::~V4L2CameraHAL() {
  HAL_LOG_ENTER();
}

int V4L2CameraHAL::getNumberOfCameras() {
  HAL_LOGV("returns %zu", mCameras.size());
  return mCameras.size();
}

int V4L2CameraHAL::getCameraInfo(int id, camera_info_t* info) {
  HAL_LOG_ENTER();
  if (id < 0 || static_cast<size_t>(id) >= mCameras.size()) {
    return -EINVAL;
  }
  // TODO(b/29185945): Hotplugging: return -EINVAL if unplugged.
  return mCameras[id]->getInfo(info);
}

int V4L2CameraHAL::setCallbacks(const camera_module_callbacks_t* callbacks) {
  HAL_LOG_ENTER();
  mCallbacks = callbacks;
  return 0;
}

void V4L2CameraHAL::getVendorTagOps(vendor_tag_ops_t* /*ops*/) {
  HAL_LOG_ENTER();
  // No vendor ops for this HAL. From <hardware/camera_common.h>:
  // "leave ops unchanged if no vendor tags are defined."
}

int V4L2CameraHAL::openLegacy(const hw_module_t* /*module*/,
                              const char* /*id*/,
                              uint32_t /*halVersion*/,
                              hw_device_t** /*device*/) {
  HAL_LOG_ENTER();
  // Not supported.
  return -ENOSYS;
}

int V4L2CameraHAL::setTorchMode(const char* /*camera_id*/, bool /*enabled*/) {
  HAL_LOG_ENTER();
  // TODO(b/29158098): HAL is required to respond appropriately if
  // the desired camera actually does support flash.
  return -ENOSYS;
}

int V4L2CameraHAL::openDevice(const hw_module_t* module,
                              const char* name,
                              hw_device_t** device) {
  HAL_LOG_ENTER();

  if (module != &HAL_MODULE_INFO_SYM.common) {
    HAL_LOGE(
        "Invalid module %p expected %p", module, &HAL_MODULE_INFO_SYM.common);
    return -EINVAL;
  }

  int id;
  if (!android::base::ParseInt(name, &id, 0, getNumberOfCameras() - 1)) {
    return -EINVAL;
  }
  // TODO(b/29185945): Hotplugging: return -EINVAL if unplugged.
  return mCameras[id]->openDevice(module, device);
}

/*
 * The framework calls the following wrappers, which in turn
 * call the corresponding methods of the global HAL object.
 */

static int get_number_of_cameras() {
  return gCameraHAL.getNumberOfCameras();
}

static int get_camera_info(int id, struct camera_info* info) {
  return gCameraHAL.getCameraInfo(id, info);
}

static int set_callbacks(const camera_module_callbacks_t* callbacks) {
  return gCameraHAL.setCallbacks(callbacks);
}

static void get_vendor_tag_ops(vendor_tag_ops_t* ops) {
  return gCameraHAL.getVendorTagOps(ops);
}

static int open_legacy(const hw_module_t* module,
                       const char* id,
                       uint32_t halVersion,
                       hw_device_t** device) {
  return gCameraHAL.openLegacy(module, id, halVersion, device);
}

static int set_torch_mode(const char* camera_id, bool enabled) {
  return gCameraHAL.setTorchMode(camera_id, enabled);
}

static int open_dev(const hw_module_t* module,
                    const char* name,
                    hw_device_t** device) {
  return gCameraHAL.openDevice(module, name, device);
}

}  // namespace v4l2_camera_hal

static hw_module_methods_t v4l2_module_methods = {
    .open = v4l2_camera_hal::open_dev};

camera_module_t HAL_MODULE_INFO_SYM __attribute__((visibility("default"))) = {
    .common =
        {
            .tag = HARDWARE_MODULE_TAG,
            .module_api_version = CAMERA_MODULE_API_VERSION_2_4,
            .hal_api_version = HARDWARE_HAL_API_VERSION,
            .id = CAMERA_HARDWARE_MODULE_ID,
            .name = "V4L2 Camera HAL v3",
            .author = "The Android Open Source Project",
            .methods = &v4l2_module_methods,
            .dso = nullptr,
            .reserved = {0},
        },
    .get_number_of_cameras = v4l2_camera_hal::get_number_of_cameras,
    .get_camera_info = v4l2_camera_hal::get_camera_info,
    .set_callbacks = v4l2_camera_hal::set_callbacks,
    .get_vendor_tag_ops = v4l2_camera_hal::get_vendor_tag_ops,
    .open_legacy = v4l2_camera_hal::open_legacy,
    .set_torch_mode = v4l2_camera_hal::set_torch_mode,
    .init = nullptr,
    .get_physical_camera_info = nullptr,
    .reserved = {nullptr, nullptr}};
