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

#ifndef ANDROID_INCLUDE_HARDWARE_POWER_H
#define ANDROID_INCLUDE_HARDWARE_POWER_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <hardware/hardware.h>

__BEGIN_DECLS

/**
 * The id of this module
 */
#define POWER_HARDWARE_MODULE_ID "power"

/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
typedef struct power_module {
    struct hw_module_t common;

    /*
     * (*init)() performs power management setup actions at runtime
     * startup, such as to set default cpufreq parameters.
     */
    void (*init)(struct power_module *module);

    /*
     * (*setInteractive)() performs power management actions upon the
     * system entering interactive state (that is, the system is awake
     * and ready for interaction, often with UI devices such as
     * display and touchscreen enabled) or non-interactive state (the
     * system appears asleep, display usually turned off).  The
     * non-interactive state is usually entered after a period of
     * inactivity, in order to conserve battery power during
     * such inactive periods.
     *
     * Typical actions are to turn on or off devices and adjust
     * cpufreq parameters.  This function may also call the
     * appropriate interfaces to allow the kernel to suspend the
     * system to low-power sleep state when entering non-interactive
     * state, and to disallow low-power suspend when the system is in
     * interactive state.  When low-power suspend state is allowed, the
     * kernel may suspend the system whenever no wakelocks are held.
     *
     * on is non-zero when the system is transitioning to an
     * interactive / awake state, and zero when transitioning to a
     * non-interactive / asleep state.
     *
     * This function is called to enter non-interactive state after
     * turning off the screen (if present), and called to enter
     * interactive state prior to turning on the screen.
     */
    void (*setInteractive)(struct power_module *module, int on);
} power_module_t;


__END_DECLS

#endif  // ANDROID_INCLUDE_HARDWARE_POWER_H
