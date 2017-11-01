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

// Modified from hardware/libhardware/modules/camera/CameraHAL.h

#ifndef V4L2_CAMERA_HAL_V4L2_CAMERA_HAL_H_
#define V4L2_CAMERA_HAL_V4L2_CAMERA_HAL_H_

#include <vector>

#include <hardware/camera_common.h>
#include <hardware/hardware.h>

#include "camera.h"
#include "common.h"

namespace v4l2_camera_hal {
/*
 * V4L2CameraHAL contains all module state that isn't specific to an
 * individual camera device. This class is based off of the sample
 * default CameraHAL from /hardware/libhardware/modules/camera.
 */
class V4L2CameraHAL {
 public:
  V4L2CameraHAL();
  ~V4L2CameraHAL();

  // Camera Module Interface (see <hardware/camera_common.h>).
  int getNumberOfCameras();
  int getCameraInfo(int camera_id, camera_info_t* info);
  int setCallbacks(const camera_module_callbacks_t* callbacks);
  void getVendorTagOps(vendor_tag_ops_t* ops);
  int openLegacy(const hw_module_t* module,
                 const char* id,
                 uint32_t halVersion,
                 hw_device_t** device);
  int setTorchMode(const char* camera_id, bool enabled);

  // Hardware Module Interface (see <hardware/hardware.h>).
  int openDevice(const hw_module_t* mod, const char* name, hw_device_t** dev);

 private:
  // Vector of cameras.
  std::vector<std::unique_ptr<default_camera_hal::Camera>> mCameras;
  // Callback handle.
  const camera_module_callbacks_t* mCallbacks;

  DISALLOW_COPY_AND_ASSIGN(V4L2CameraHAL);
};

}  // namespace v4l2_camera_hal

extern camera_module_t HAL_MODULE_INFO_SYM;

#endif  // V4L2_CAMERA_HAL_V4L2_CAMERA_HAL_H_
