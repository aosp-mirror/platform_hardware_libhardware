/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <cstdlib>
#include <pthread.h>
#include <hardware/camera3.h>
#include "CameraHAL.h"

//#define LOG_NDEBUG 0
#define LOG_TAG "Camera"
#include <cutils/log.h>

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL)
#include <cutils/trace.h>

#include "Camera.h"

namespace default_camera_hal {

extern "C" {
// Shim passed to the framework to close an opened device.
static int close_device(hw_device_t* dev)
{
    camera3_device_t* cam_dev = reinterpret_cast<camera3_device_t*>(dev);
    Camera* cam = static_cast<Camera*>(cam_dev->priv);
    return cam->close();
}
} // extern "C"

Camera::Camera(int id)
  : mId(id),
    mBusy(false),
    mCallbackOps(NULL)
{
    pthread_mutex_init(&mMutex,
                       NULL); // No pthread mutex attributes.

    memset(&mDevice, 0, sizeof(mDevice));
    mDevice.common.tag    = HARDWARE_DEVICE_TAG;
    mDevice.common.close  = close_device;
    mDevice.ops           = const_cast<camera3_device_ops_t*>(&sOps);
    mDevice.priv          = this;
}

Camera::~Camera()
{
}

int Camera::open(const hw_module_t *module, hw_device_t **device)
{
    ALOGI("%s:%d: Opening camera device", __func__, mId);
    ATRACE_BEGIN(__func__);
    pthread_mutex_lock(&mMutex);
    if (mBusy) {
        pthread_mutex_unlock(&mMutex);
        ATRACE_END();
        ALOGE("%s:%d: Error! Camera device already opened", __func__, mId);
        return -EBUSY;
    }

    // TODO: open camera dev nodes, etc
    mBusy = true;
    mDevice.common.module = const_cast<hw_module_t*>(module);
    *device = &mDevice.common;

    pthread_mutex_unlock(&mMutex);
    ATRACE_END();
    return 0;
}

int Camera::close()
{
    ALOGI("%s:%d: Closing camera device", __func__, mId);
    ATRACE_BEGIN(__func__);
    pthread_mutex_lock(&mMutex);
    if (!mBusy) {
        pthread_mutex_unlock(&mMutex);
        ATRACE_END();
        ALOGE("%s:%d: Error! Camera device not open", __func__, mId);
        return -EINVAL;
    }

    // TODO: close camera dev nodes, etc
    mBusy = false;

    pthread_mutex_unlock(&mMutex);
    ATRACE_END();
    return 0;
}

int Camera::initialize(const camera3_callback_ops_t *callback_ops)
{
    ALOGV("%s:%d: callback_ops=%p", __func__, mId, callback_ops);
    mCallbackOps = callback_ops;
    return 0;
}

int Camera::configureStreams(camera3_stream_configuration_t *stream_list)
{
    ALOGV("%s:%d: stream_list=%p", __func__, mId, stream_list);
    // TODO: validate input, create internal stream representations
    return 0;
}

int Camera::registerStreamBuffers(const camera3_stream_buffer_set_t *buf_set)
{
    ALOGV("%s:%d: buffer_set=%p", __func__, mId, buf_set);
    // TODO: register buffers with hardware
    return 0;
}

const camera_metadata_t* Camera::constructDefaultRequestSettings(int type)
{
    ALOGV("%s:%d: type=%d", __func__, mId, type);
    // TODO: return statically built default request
    return NULL;
}

int Camera::processCaptureRequest(camera3_capture_request_t *request)
{
    ALOGV("%s:%d: request=%p", __func__, mId, request);
    ATRACE_BEGIN(__func__);

    if (request == NULL) {
        ALOGE("%s:%d: NULL request recieved", __func__, mId);
        ATRACE_END();
        return -EINVAL;
    }

    // TODO: verify request; submit request to hardware
    ATRACE_END();
    return 0;
}

void Camera::getMetadataVendorTagOps(vendor_tag_query_ops_t *ops)
{
    ALOGV("%s:%d: ops=%p", __func__, mId, ops);
    // TODO: return vendor tag ops
}

void Camera::dump(int fd)
{
    ALOGV("%s:%d: Dumping to fd %d", fd);
    // TODO: dprintf all relevant state to fd
}

extern "C" {
// Get handle to camera from device priv data
static Camera *camdev_to_camera(const camera3_device_t *dev)
{
    return reinterpret_cast<Camera*>(dev->priv);
}

static int initialize(const camera3_device_t *dev,
        const camera3_callback_ops_t *callback_ops)
{
    return camdev_to_camera(dev)->initialize(callback_ops);
}

static int configure_streams(const camera3_device_t *dev,
        camera3_stream_configuration_t *stream_list)
{
    return camdev_to_camera(dev)->configureStreams(stream_list);
}

static int register_stream_buffers(const camera3_device_t *dev,
        const camera3_stream_buffer_set_t *buffer_set)
{
    return camdev_to_camera(dev)->registerStreamBuffers(buffer_set);
}

static const camera_metadata_t *construct_default_request_settings(
        const camera3_device_t *dev, int type)
{
    return camdev_to_camera(dev)->constructDefaultRequestSettings(type);
}

static int process_capture_request(const camera3_device_t *dev,
        camera3_capture_request_t *request)
{
    return camdev_to_camera(dev)->processCaptureRequest(request);
}

static void get_metadata_vendor_tag_ops(const camera3_device_t *dev,
        vendor_tag_query_ops_t *ops)
{
    camdev_to_camera(dev)->getMetadataVendorTagOps(ops);
}

static void dump(const camera3_device_t *dev, int fd)
{
    camdev_to_camera(dev)->dump(fd);
}
} // extern "C"

const camera3_device_ops_t Camera::sOps = {
    .initialize              = default_camera_hal::initialize,
    .configure_streams       = default_camera_hal::configure_streams,
    .register_stream_buffers = default_camera_hal::register_stream_buffers,
    .construct_default_request_settings =
            default_camera_hal::construct_default_request_settings,
    .process_capture_request = default_camera_hal::process_capture_request,
    .get_metadata_vendor_tag_ops =
            default_camera_hal::get_metadata_vendor_tag_ops,
    .dump                    = default_camera_hal::dump
};

} // namespace default_camera_hal
