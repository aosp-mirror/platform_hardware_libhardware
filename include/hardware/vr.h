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

#ifndef ANDROID_INCLUDE_HARDWARE_VR_H
#define ANDROID_INCLUDE_HARDWARE_VR_H

#include <stdbool.h>
#include <sys/cdefs.h>
#include <hardware/hardware.h>

__BEGIN_DECLS

#define VR_HARDWARE_MODULE_ID "vr_module"

#define VR_MODULE_API_VERSION_1_0 HARDWARE_MODULE_API_VERSION(1, 0)

/**
 * Implement this HAL to receive callbacks when a virtual reality (VR)
 * application is being used.  VR applications characteristically have a number
 * of special display and performance requirements, including:
 * - Low sensor latency - Total end-to-end latency from the IMU, accelerometer,
 *   and gyro to an application-visible callback must be extremely low (<5ms
 *   typically).  This is required for HIFI sensor support.
 * - Low display latency - Total end-to-end latency from the GPU draw calls to
 *   the actual display update must be as low as possible.  This is typically
 *   achieved by using SurfaceFlinger in a single-buffered mode, and assuring
 *   that draw calls are synchronized with the display scanout correctly.  Any
 *   GPU settings required to allow consistent performance of this operation
 *   are required, including the EGL extensions: EGL_IMG_context_priority, and
 *   EGL_XXX_set_render_buffer_mode.
 * - Low-persistence display - Display persistence settings must be set as low as
 *   possible while still maintaining a reasonable brightness.  For a typical
 *   display running at 60Hz, pixels should be illuminated for <4ms to be
 *   considered low-persistence (<2ms is desirable).  This avoids ghosting
 *   during movements in a VR setting.
 * - Consistent performance of the GPU and CPU - When given a mixed GPU/CPU
 *   workload for a VR application with bursts of work at regular intervals
 *   several times a frame.  CPU scheduling should ensure that the application
 *   render thread is run consistently within 1ms of when required for the
 *   draw window, and an appropriate clockrate is maintained to ensure the
 *   workload finishes within the time alloted to the draw window.  Likewise,
 *   GPU scheduling should ensure that work from the application render thread
 *   is given priority over other GPU work, and that a high enough clockrate can
 *   be maintained to ensure that this completes within the draw window.  CTS
 *   tests with example VR workloads will be available to assess performance
 *   tuning.
 *
 * Vendors implementing this HAL are expected to use set_vr_mode as a hint to
 * enable VR-specific performance tuning, and to turn on any device features
 * optimal for VR display modes (or do nothing if none are available). Devices
 * that advertise FEATURE_VR_MODE_HIGH_PERFORMANCE are must pass the additional
 * CTS performance tests required for this feature and follow the additional
 * guidelines for hardware implementation for "VR Ready" devices.
 *
 * No methods in this HAL will be called concurrently from the Android framework.
 */
typedef struct vr_module {
    /**
     * Common methods of the  module.  This *must* be the first member of
     * vr_module as users of * this structure will cast a hw_module_t to a
     * vr_module pointer in contexts where it's known that the hw_module_t
     * references a vr_module.
     */
    struct hw_module_t common;

    /**
     * Convenience method for the HAL implementation to set up any state needed
     * at runtime startup.  This is called once from the VrManagerService during
     * its boot phase.  No methods from this HAL will be called before init.
     */
    void (*init)(struct vr_module *module);

    /**
     * Set the VR mode state.  Possible states of the enabled parameter are:
     * false - VR mode is disabled, turn off all VR-specific settings.
     * true - VR mode is enabled, turn on all VR-specific settings.
     *
     * This is called from the VrManagerService whenever the application(s)
     * currently in use enters or leaves VR mode. This will typically occur
     * when the user switches or from an application that has indicated to
     * system_server that it should run in VR mode.
     */
    void (*set_vr_mode)(struct vr_module *module, bool enabled);

    /* Reserved for future use. Must be NULL. */
    void* reserved[8 - 2];
} vr_module_t;

__END_DECLS

#endif /* ANDROID_INCLUDE_HARDWARE_VR_H */
