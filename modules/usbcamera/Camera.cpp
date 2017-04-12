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

//#define LOG_NDEBUG 0
#define LOG_TAG "Camera"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <cstdlib>

#include <log/log.h>
#include <utils/Mutex.h>

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL)
#include <utils/Trace.h>

#include <hardware/camera3.h>
#include <system/camera_metadata.h>
#include <system/graphics.h>

#include "CameraHAL.h"
#include "Metadata.h"
#include "Stream.h"

#include "Camera.h"

namespace usb_camera_hal {

extern "C" {
// Shim passed to the framework to close an opened device.
static int close_device(hw_device_t* dev) {
    camera3_device_t* cam_dev = reinterpret_cast<camera3_device_t*>(dev);
    Camera* cam = static_cast<Camera*>(cam_dev->priv);
    return cam->close();
}

// Get handle to camera from device priv data
static Camera *camdev_to_camera(const camera3_device_t *dev) {
    return reinterpret_cast<Camera*>(dev->priv);
}

static int initialize(const camera3_device_t *dev,
        const camera3_callback_ops_t *callback_ops) {
    return camdev_to_camera(dev)->initialize(callback_ops);
}

static int configure_streams(const camera3_device_t *dev,
        camera3_stream_configuration_t *stream_list) {
    return camdev_to_camera(dev)->configureStreams(stream_list);
}

static const camera_metadata_t *construct_default_request_settings(
        const camera3_device_t *dev, int type) {
    return camdev_to_camera(dev)->constructDefaultRequestSettings(type);
}

static int process_capture_request(const camera3_device_t *dev,
        camera3_capture_request_t *request) {
    return camdev_to_camera(dev)->processCaptureRequest(request);
}

static void dump(const camera3_device_t *dev, int fd) {
    camdev_to_camera(dev)->dump(fd);
}

static int flush(const camera3_device_t *dev) {
    return camdev_to_camera(dev)->flush();
}

} // extern "C"

const camera3_device_ops_t Camera::sOps = {
    .initialize = usb_camera_hal::initialize,
    .configure_streams = usb_camera_hal::configure_streams,
    .register_stream_buffers = NULL,
    .construct_default_request_settings
        = usb_camera_hal::construct_default_request_settings,
    .process_capture_request = usb_camera_hal::process_capture_request,
    .get_metadata_vendor_tag_ops = NULL,
    .dump = usb_camera_hal::dump,
    .flush = usb_camera_hal::flush,
    .reserved = {0},
};

Camera::Camera(int id)
          : mId(id),
            mStaticInfo(NULL),
            mBusy(false),
            mCallbackOps(NULL),
            mSettings(NULL),
            mIsInitialized(false) {
    memset(&mTemplates, 0, sizeof(mTemplates));
    memset(&mDevice, 0, sizeof(mDevice));
    mDevice.common.tag    = HARDWARE_DEVICE_TAG;
    // TODO: Upgrade to HAL3.3
    mDevice.common.version = CAMERA_DEVICE_API_VERSION_3_2;
    mDevice.common.close  = close_device;
    mDevice.ops           = const_cast<camera3_device_ops_t*>(&sOps);
    mDevice.priv          = this;
}

Camera::~Camera() {
    if (mStaticInfo != NULL) {
        free_camera_metadata(mStaticInfo);
    }

    for (int i = 0; i < CAMERA3_TEMPLATE_COUNT; i++) {
        free_camera_metadata(mTemplates[i]);
    }

    if (mSettings != NULL) {
        free_camera_metadata(mSettings);
    }
}

int Camera::open(const hw_module_t *module, hw_device_t **device) {
    ALOGI("%s:%d: Opening camera device", __func__, mId);
    ATRACE_CALL();
    android::Mutex::Autolock al(mDeviceLock);

    if (mBusy) {
        ALOGE("%s:%d: Error! Camera device already opened", __func__, mId);
        return -EBUSY;
    }

    mBusy = true;
    mDevice.common.module = const_cast<hw_module_t*>(module);
    *device = &mDevice.common;
    return openDevice();
}

int Camera::getInfo(struct camera_info *info) {
    android::Mutex::Autolock al(mStaticInfoLock);

    // TODO: update to CAMERA_FACING_EXTERNAL once the HAL API changes are merged.
    info->facing = CAMERA_FACING_FRONT;
    info->orientation = 0;
    info->device_version = mDevice.common.version;
    if (mStaticInfo == NULL) {
        initStaticInfo();
    }
    info->static_camera_characteristics = mStaticInfo;
    return 0;
}

void Camera::updateInfo() {
    android::Mutex::Autolock al(mStaticInfoLock);
    initStaticInfo();
}

int Camera::close() {
    ALOGI("%s:%d: Closing camera device", __func__, mId);
    ATRACE_CALL();
    android::Mutex::Autolock al(mDeviceLock);

    if (!mBusy) {
        ALOGE("%s:%d: Error! Camera device not open", __func__, mId);
        return -EINVAL;
    }

    mBusy = false;
    mIsInitialized = false;
    return closeDevice();
}

int Camera::initialize(const camera3_callback_ops_t *callback_ops) {
    int res;

    ALOGV("%s:%d: callback_ops=%p", __func__, mId, callback_ops);
    ATRACE_CALL();
    android::Mutex::Autolock al(mDeviceLock);

    mCallbackOps = callback_ops;
    // per-device specific initialization
    res = initDevice();
    if (res != 0) {
        ALOGE("%s:%d: Failed to initialize device!", __func__, mId);
        return res;
    }

    mIsInitialized = true;
    return 0;
}

int Camera::configureStreams(camera3_stream_configuration_t *stream_config) {
    camera3_stream_t *astream;
    android::Vector<Stream *> newStreams;

    ALOGV("%s:%d: stream_config=%p", __func__, mId, stream_config);
    ATRACE_CALL();
    android::Mutex::Autolock al(mDeviceLock);
    if (!mIsInitialized) {
        ALOGE("Device is not initialized yet");
        return -EINVAL;
    }

    if (stream_config == NULL) {
        ALOGE("%s:%d: NULL stream configuration array", __func__, mId);
        return -EINVAL;
    }
    if (stream_config->num_streams == 0) {
        ALOGE("%s:%d: Empty stream configuration array", __func__, mId);
        return -EINVAL;
    }

    ALOGV("%s:%d: Number of Streams: %d", __func__, mId,
            stream_config->num_streams);
    // Mark all current streams unused for now
    for (size_t i = 0; i < mStreams.size(); i++) {
        mStreams[i]->mReuse = false;
    }
    // Fill new stream array with reused streams and new streams
    for (unsigned int i = 0; i < stream_config->num_streams; i++) {
        astream = stream_config->streams[i];
        if (astream->max_buffers > 0) {
            ALOGV("%s:%d: Reusing stream %d", __func__, mId, i);
            newStreams.add(reuseStreamLocked(astream));
        } else {
            ALOGV("%s:%d: Creating new stream %d", __func__, mId, i);
            newStreams.add(new Stream(mId, astream));
        }

        if (newStreams[i] == NULL) {
            ALOGE("%s:%d: Error processing stream %d", __func__, mId, i);
            goto err_out;
        }
        astream->priv = reinterpret_cast<void *>(newStreams[i]);
    }

    // Verify the set of streams in aggregate
    if (!isValidStreamSetLocked(newStreams)) {
        ALOGE("%s:%d: Invalid stream set", __func__, mId);
        goto err_out;
    }

    // Set up all streams (calculate usage/max_buffers for each)
    setupStreamsLocked(newStreams);

    // Destroy all old streams and replace stream array with new one
    destroyStreamsLocked(mStreams);
    mStreams = newStreams;

    // Clear out last seen settings metadata
    updateSettingsLocked(NULL);
    return 0;

err_out:
    // Clean up temporary streams, preserve existing mStreams
    destroyStreamsLocked(newStreams);
    return -EINVAL;
}

void Camera::destroyStreamsLocked(android::Vector<Stream *> &streams) {
    for (size_t i = 0; i < streams.size(); i++) {
        delete streams[i];
    }
    streams.clear();
}

Stream *Camera::reuseStreamLocked(camera3_stream_t *astream) {
    Stream *priv = reinterpret_cast<Stream*>(astream->priv);
    // Verify the re-used stream's parameters match
    if (!priv->isValidReuseStream(mId, astream)) {
        ALOGE("%s:%d: Mismatched parameter in reused stream", __func__, mId);
        return NULL;
    }
    // Mark stream to be reused
    priv->mReuse = true;
    return priv;
}

bool Camera::isValidStreamSetLocked(const android::Vector<Stream *> &streams) {
    int inputs = 0;
    int outputs = 0;

    if (streams.isEmpty()) {
        ALOGE("%s:%d: Zero count stream configuration streams", __func__, mId);
        return false;
    }
    // Validate there is at most one input stream and at least one output stream
    for (size_t i = 0; i < streams.size(); i++) {
        // A stream may be both input and output (bidirectional)
        if (streams[i]->isInputType())
            inputs++;
        if (streams[i]->isOutputType())
            outputs++;
    }
    ALOGV("%s:%d: Configuring %d output streams and %d input streams",
            __func__, mId, outputs, inputs);
    if (outputs < 1) {
        ALOGE("%s:%d: Stream config must have >= 1 output", __func__, mId);
        return false;
    }
    if (inputs > 1) {
        ALOGE("%s:%d: Stream config must have <= 1 input", __func__, mId);
        return false;
    }
    // TODO: check for correct number of Bayer/YUV/JPEG/Encoder streams
    return true;
}

void Camera::setupStreamsLocked(android::Vector<Stream *> &streams) {
    /*
     * This is where the HAL has to decide internally how to handle all of the
     * streams, and then produce usage and max_buffer values for each stream.
     * Note, the stream vector has been checked before this point for ALL invalid
     * conditions, so it must find a successful configuration for this stream
     * array.  The HAL may not return an error from this point.
     *
     * TODO: we just set all streams to be the same dummy values;
     * real implementations will want to avoid USAGE_SW_{READ|WRITE}_OFTEN.
     */
    for (size_t i = 0; i < streams.size(); i++) {
        uint32_t usage = 0;

        if (streams[i]->isOutputType())
            usage |= GRALLOC_USAGE_SW_WRITE_OFTEN |
                     GRALLOC_USAGE_HW_CAMERA_WRITE;
        if (streams[i]->isInputType())
            usage |= GRALLOC_USAGE_SW_READ_OFTEN |
                     GRALLOC_USAGE_HW_CAMERA_READ;

        streams[i]->setUsage(usage);
        streams[i]->setMaxBuffers(1);
    }
}

bool Camera::isValidTemplateType(int type) {
    return type >= 1 && type < CAMERA3_TEMPLATE_COUNT;
}

const camera_metadata_t* Camera::constructDefaultRequestSettings(int type) {
    ALOGV("%s:%d: type=%d", __func__, mId, type);
    android::Mutex::Autolock al(mDeviceLock);

    if (!isValidTemplateType(type)) {
        ALOGE("%s:%d: Invalid template request type: %d", __func__, mId, type);
        return NULL;
    }

    // DO NOT try to initialize the device here, it will be guaranteed deadlock.
    if (!mIsInitialized) {
        ALOGE("Device is not initialized yet");
        return NULL;
    }

    return mTemplates[type];
}

// This implementation is a copy-paste, probably we should override (or move) this to
// device specific class.
int Camera::processCaptureRequest(camera3_capture_request_t *request) {
    camera3_capture_result result;
    ALOGV("%s:%d: request=%p", __func__, mId, request);
    ATRACE_CALL();
    android::Mutex::Autolock al(mDeviceLock);

    if (request == NULL) {
        ALOGE("%s:%d: NULL request recieved", __func__, mId);
        return -EINVAL;
    }

    ALOGV("%s:%d: Request Frame:%d Settings:%p", __func__, mId,
            request->frame_number, request->settings);

    // NULL indicates use last settings
    if (request->settings == NULL) {
        if (mSettings == NULL) {
            ALOGE("%s:%d: NULL settings without previous set Frame:%d Req:%p",
                    __func__, mId, request->frame_number, request);
            return -EINVAL;
        }
    } else {
        updateSettingsLocked(request->settings);
    }

    if (request->input_buffer != NULL) {
        ALOGV("%s:%d: Reprocessing input buffer is not supported yet", __func__, mId);
        return -EINVAL;
    } else {
        ALOGV("%s:%d: Capturing new frame.", __func__, mId);

        if (!isValidCaptureSettings(request->settings)) {
            ALOGE("%s:%d: Invalid settings for capture request: %p",
                    __func__, mId, request->settings);
            return -EINVAL;
        }
    }

    if (request->num_output_buffers <= 0) {
        ALOGE("%s:%d: Invalid number of output buffers: %d", __func__, mId,
                request->num_output_buffers);
        return -EINVAL;
    }
    result.num_output_buffers = request->num_output_buffers;
    result.output_buffers = new camera3_stream_buffer_t[result.num_output_buffers];
    for (unsigned int i = 0; i < request->num_output_buffers; i++) {
        int res = processCaptureBuffer(&request->output_buffers[i],
                const_cast<camera3_stream_buffer_t*>(&result.output_buffers[i]));
        if (res) {
            delete [] result.output_buffers;
            // TODO: this should probably be a total device failure; transient for now
            return -EINVAL;
        }
    }

    result.frame_number = request->frame_number;
    // TODO: return actual captured/reprocessed settings
    result.result = request->settings;
    // TODO: asynchronously return results
    notifyShutter(request->frame_number, 0);
    mCallbackOps->process_capture_result(mCallbackOps, &result);

    // Free up capture result related resources, HAL owns the capture result, and it
    // is only valid during the process_capture_result call.
    delete[] result.output_buffers;

    return 0;
}

int Camera::flush() {
    int res;

    ALOGV("%s:%d: flush device", __func__, mId);
    // per-device specific flush
    res = flushDevice();
    if (res != 0) {
        ALOGE("%s:%d: Failed to flush device!", __func__, mId);
        return res;
    }
    return 0;
}

void Camera::updateSettingsLocked(const camera_metadata_t *new_settings) {
    if (mSettings != NULL) {
        free_camera_metadata(mSettings);
        mSettings = NULL;
    }

    if (new_settings != NULL)
        mSettings = clone_camera_metadata(new_settings);
}

void Camera::notifyShutter(uint32_t frame_number, uint64_t timestamp) {
    int res;
    struct timespec ts;

    // If timestamp is 0, get timestamp from right now instead
    if (timestamp == 0) {
        ALOGW("%s:%d: No timestamp provided, using CLOCK_BOOTTIME",
                __func__, mId);
        res = clock_gettime(CLOCK_BOOTTIME, &ts);
        if (res == 0) {
            timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
        } else {
            ALOGE("%s:%d: No timestamp and failed to get CLOCK_BOOTTIME %s(%d)",
                    __func__, mId, strerror(errno), errno);
        }
    }
    camera3_notify_msg_t m;
    memset(&m, 0, sizeof(m));
    m.type = CAMERA3_MSG_SHUTTER;
    m.message.shutter.frame_number = frame_number;
    m.message.shutter.timestamp = timestamp;
    mCallbackOps->notify(mCallbackOps, &m);
}

void Camera::dump(int fd) {
    ALOGV("%s:%d: Dumping to fd %d", __func__, mId, fd);
    ATRACE_CALL();
    android::Mutex::Autolock al(mDeviceLock);

    dprintf(fd, "Camera ID: %d (Busy: %d)\n", mId, mBusy);

    // TODO: dump all settings
    dprintf(fd, "Most Recent Settings: (%p)\n", mSettings);

    dprintf(fd, "Number of streams: %zu\n", mStreams.size());
    for (size_t i = 0; i < mStreams.size(); i++) {
        dprintf(fd, "Stream %zu/%zu:\n", i, mStreams.size());
        mStreams[i]->dump(fd);
    }
}

const char* Camera::templateToString(int type) {
    switch (type) {
    case CAMERA3_TEMPLATE_PREVIEW:
        return "CAMERA3_TEMPLATE_PREVIEW";
    case CAMERA3_TEMPLATE_STILL_CAPTURE:
        return "CAMERA3_TEMPLATE_STILL_CAPTURE";
    case CAMERA3_TEMPLATE_VIDEO_RECORD:
        return "CAMERA3_TEMPLATE_VIDEO_RECORD";
    case CAMERA3_TEMPLATE_VIDEO_SNAPSHOT:
        return "CAMERA3_TEMPLATE_VIDEO_SNAPSHOT";
    case CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG:
        return "CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG";
    case CAMERA3_TEMPLATE_MANUAL:
        return "CAMERA3_TEMPLATE_MANUAL";
    }

    return "Invalid template type!";
}

int Camera::setTemplate(int type, camera_metadata_t *settings) {
    android::Mutex::Autolock al(mDeviceLock);

    if (!isValidTemplateType(type)) {
        ALOGE("%s:%d: Invalid template request type: %d", __func__, mId, type);
        return -EINVAL;
    }

    if (mTemplates[type] != NULL) {
        ALOGE("%s:%d: Setting already constructed template type %s(%d)",
                __func__, mId, templateToString(type), type);
        return -EINVAL;
    }

    // Make a durable copy of the underlying metadata
    mTemplates[type] = clone_camera_metadata(settings);
    if (mTemplates[type] == NULL) {
        ALOGE("%s:%d: Failed to clone metadata %p for template type %s(%d)",
                __func__, mId, settings, templateToString(type), type);
        return -EINVAL;
    }
    return 0;
}

} // namespace usb_camera_hal
