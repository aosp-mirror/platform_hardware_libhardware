/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ANDROID_VEHICLE_CAMERA_INTERFACE_H
#define ANDROID_VEHICLE_CAMERA_INTERFACE_H

#include <errno.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <hardware/hardware.h>
#include <cutils/native_handle.h>
#include <system/window.h>

__BEGIN_DECLS

/*****************************************************************************/

#define VEHICLE_CAMERA_HEADER_VERSION          1
#define VEHICLE_CAMERA_MODULE_API_VERSION_1_0  HARDWARE_MODULE_API_VERSION(1, 0)
#define VEHICLE_CAMERA_DEVICE_API_VERSION_1_0  HARDWARE_DEVICE_API_VERSION_2(1, 0, VEHICLE_CAMERA_HEADER_VERSION)

/**
 * Vehicle Camera to provide interfaces for controlling cameras
 */

/**
 * The id of this module
 */
#define VEHICLE_CAMERA_HARDWARE_MODULE_ID  "vehicle_camera"

/**
 *  Name of the vehicle device to open.  Extend this list as
 *  more cameras are added.  Each camera defined in vehicle_camera_type_t
 *  shall have a string defined for it.
 */
#define VEHICLE_CAMERA_RVC_DEVICE       "vehicle_camera_rvc_device"

/**
 * Each camera on the car shall be enumerated
 */
typedef enum {
    VEHICLE_CAMERA_RVC = 1,
} vehicle_camera_type_t;

/**
 * Describes the current state of camera device
 */
typedef struct {
    uint32_t overlay_on;
    uint32_t camera_on;
} vehicle_camera_state_t;

/**
 * Bitmask of features supported by a camera module
 */
typedef enum {
    VEHICLE_CAMERA_CONFIG_FLAG_ANDROID_OVERLAY_SUPPORT      = 0x1,
    VEHICLE_CAMERA_CONFIG_FLAG_CAMERA_CROP_SUPPORT          = 0x2,
    VEHICLE_CAMERA_CONFIG_FLAG_CAMERA_POSITIONING_SUPPORT   = 0x4
} vehicle_camera_config_flag;

typedef struct {
    uint32_t capabilites_flags;
    uint32_t camera_width;
    uint32_t camera_height;
    uint32_t display_width;
    uint32_t display_height;
} vehicle_camera_cap_t;

/************************************************************************************/

/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
typedef struct {
    struct hw_module_t common;
    /**
     * Queries the HW for the cameras installed on the vehicle
     * @param num_cameras - number of camera devices available.  If
     *                    0 is returned, an error has occurred and
     *                    the return pointer shall be NULL.
     * @return pointer to an array of vehicle_camera_type_t to
     *         denote which cameras are installed.  This pointer is
     *         only valid while the vehicle hal is loaded.  If the
     *         pointer is NULL, then an error has occurred and
     *         num_cameras shall be 0.
     */
    const uint32_t * (*get_camera_device_list)(uint32_t *num_cameras);
} vehicle_camera_module_t;


typedef struct vehicle_camera_device_t {
    struct hw_device_t common;

    uint32_t camera_type;

    /**
     * Returns the capabilities of this camera.
     * @param device - device handle
     * @param cap - pointer to capabilities flags being returned
     * @return 0 on success
     *          -EPERM if device is invalid or not initialized
     */
    int (*get_capabilities)(struct vehicle_camera_device_t *device, vehicle_camera_cap_t *cap);

    /**
     * Gets the current camera crop settings.
     * @param device - device handle
     * @param rect - current camera crop settings
     * @return 0 on success
     *          -EPERM if device is not initialized
     *          -errno on error
     */
    int (*get_camera_crop)(struct vehicle_camera_device_t *device, android_native_rect_t *rect);

    /**
     * Sets the camera crop.
     * @param device - device handle
     * @param rect - area of camera input to crop.  Must fit within
     *             camera width and height from camera capabilities.
     * @return 0 on success
     *          -EPERM if device is not initialized
     *          -errno on error
     */
    int (*set_camera_crop)(struct vehicle_camera_device_t *device, const android_native_rect_t *rect);

    /**
     * Gets position of the camera on the display.
     * @param device - device handle
     * @param rect - area of display the camera will appear when on
     * @return 0 on success
     *          -EPERM if device is not initialized
     *          -errno on error
     */
    int (*get_camera_position)(struct vehicle_camera_device_t *device, android_native_rect_t *rect);

    /**
     * Sets position of the camera on the display.
     * @param device - device handle
     * @param rect - area of display the camera will appear when on.
     *             Must fit within display width and height from
     *             camera capabilities.
     * @return 0 on success
     *          -EPERM if device is not initialized
     *          -errno on error
     */
    int (*set_camera_position)(struct vehicle_camera_device_t *device, const android_native_rect_t *rect);

    /**
     * Gets the current camera state.
     * @param device - device handle
     * @param state - last setting for the camera
     * @return 0 on success
     *          -EPERM if device is not initialized
     */
    int (*get_camera_state)(struct vehicle_camera_device_t *device, vehicle_camera_state_t *state);

    /**
     * Sets the camera state.
     * @param device - device handle
     * @param state - desired setting for the camera
     * @return 0 on success
     *          -EPERM if device is not initialized
     *          -errno on error
     */
    int (*set_camera_state)(struct vehicle_camera_device_t *device, const vehicle_camera_state_t *state);
} vehicle_camera_device_t;

__END_DECLS

#endif  // ANDROID_VEHICLE_CAMERA_INTERFACE_H
