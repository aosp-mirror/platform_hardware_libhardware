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

#ifndef ANDROID_VEHICLE_RVC_INTERFACE_H
#define ANDROID_VEHICLE_RVC_INTERFACE_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <errno.h>

#include <hardware/hardware.h>
#include <cutils/native_handle.h>

__BEGIN_DECLS

/*****************************************************************************/

#define VEHICLE_RVC_HEADER_VERSION          1
#define VEHICLE_RVC_MODULE_API_VERSION_1_0  HARDWARE_MODULE_API_VERSION(1, 0)
#define VEHICLE_RVC_DEVICE_API_VERSION_1_0  HARDWARE_DEVICE_API_VERSION_2(1, 0, VEHICLE_RVC_HEADER_VERSION)

/**
 * Vehicle Rearview Camera to provide interfaces for controlling
 * the RVC.
 */

/**
 * The id of this module
 */
#define VEHICLE_RVC_HARDWARE_MODULE_ID  "vehicle_rvc"

/**
 *  Name of the vehicle device to open
 */
#define VEHICLE_RVC_HARDWARE_DEVICE     "vehicle_rvc_hw_device"

/**
 * Describes the current state of RVC module
 */
typedef struct {
    uint32_t overlay_on;
    uint32_t rvc_on;
} vehicle_rvc_state_t;

/**
 * Describes a rectangle for cropping and positioning objects
 * uint32_t left    Position of left border of rectangle
 * uint32_t top     Position of top border of rectangle
 * uint32_t width   Width of rectangle
 * uint32_t height  Height of rectangle
 */
typedef struct {
    uint32_t left;
    uint32_t top;
    uint32_t width;
    uint32_t height;
} vehicle_rvc_rect_t;

/**
 * Bitmask of features supported by RVC module
 */
enum vehicle_rvc_config_flag {
    ANDROID_OVERLAY_SUPPORT_FLAG        = 0x1,
    CAMERA_CROP_SUPPORT_FLAG            = 0x2,
    CAMERA_POSITIONING_SUPPORT_FLAG     = 0x4
};

typedef struct {
    uint32_t capabilites_flags;
    uint32_t camera_width;
    uint32_t camera_height;
    uint32_t display_width;
    uint32_t display_height;
} vehicle_rvc_cap_t;

/************************************************************************************/

/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
typedef struct {
    struct hw_module_t common;
} vehicle_rvc_module_t;


typedef struct vehicle_rvc_device_t {
    struct hw_device_t common;

    /**
     * Returns the capabilities of this RVC.
     * @param device
     * @return
     */
    int (*get_capabilities)(struct vehicle_rvc_device_t *device, vehicle_rvc_cap_t *cap);
    /**
     * Gets the current RVC crop settings.
     * @param device
     * @return
     */
    int (*get_rvc_crop)(struct vehicle_rvc_device_t *device, vehicle_rvc_rect_t *rect);
    /**
     * Sets the RVC crop.
     * @param device
     * @param rect Area of RVC camera input to crop
     * @return
     */
    int (*set_rvc_crop)(struct vehicle_rvc_device_t *device, const vehicle_rvc_rect_t *rect);
    /**
     * Gets position of the RVC on the dispaly.
     * @param device
     * @param rect Area of display the RVC will appear when on
     * @return
     */
    int (*get_rvc_position)(struct vehicle_rvc_device_t *device, vehicle_rvc_rect_t *rect);
    /**
     * Sets position of the RVC on the display.
     * @param device
     * @param rect
     * @return
     */
    int (*set_rvc_position)(struct vehicle_rvc_device_t *device, const vehicle_rvc_rect_t *rect);
    /**
     * Gets the current camera state.
     * @param device
     * @return
     */
    int (*get_camera_state)(struct vehicle_rvc_device_t *device, vehicle_rvc_state_t *state);
    /**
     * Sets the camera state.  Calling this function will generate a
     * callback notifying the user that the camera state has
     * changed.
     * @param device
     * @return
     */
    int (*set_camera_state)(struct vehicle_rvc_device_t *device, const vehicle_rvc_state_t *state);
} vehicle_rvc_device_t;

__END_DECLS

#endif  // ANDROID_VEHICLE_RVC_INTERFACE_H
