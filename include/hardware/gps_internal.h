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

#ifndef ANDROID_INCLUDE_HARDWARE_GPS_INTERNAL_H
#define ANDROID_INCLUDE_HARDWARE_GPS_INTERNAL_H

#include "hardware/gps.h"

/****************************************************************************
 * This file contains legacy structs that are deprecated/retired from gps.h *
 ****************************************************************************/

__BEGIN_DECLS

/**
 * Legacy struct to represent SV status.
 * See GpsSvStatus_v2 for more information.
 */
typedef struct {
    /** set to sizeof(GpsSvStatus_v1) */
    size_t          size;
    int         num_svs;
    GpsSvInfo   sv_list[GPS_MAX_SVS];
    uint32_t    ephemeris_mask;
    uint32_t    almanac_mask;
    uint32_t    used_in_fix_mask;
} GpsSvStatus_v1;

#pragma pack(push,4)
// We need to keep the alignment of this data structure to 4-bytes, to ensure that in 64-bit
// environments the size of this legacy definition does not collide with _v2. Implementations should
// be using _v2 and _v3, so it's OK to pay the 'unaligned' penalty in 64-bit if an old
// implementation is still in use.

/**
 * Legacy struct to represent the status of AGPS.
 */
typedef struct {
    /** set to sizeof(AGpsStatus_v1) */
    size_t          size;
    AGpsType        type;
    AGpsStatusValue status;
} AGpsStatus_v1;

#pragma pack(pop)

/**
 * Legacy struct to represent the status of AGPS augmented with a IPv4 address
 * field.
 */
typedef struct {
    /** set to sizeof(AGpsStatus_v2) */
    size_t          size;
    AGpsType        type;
    AGpsStatusValue status;

    /*-------------------- New fields in _v2 --------------------*/

    uint32_t        ipaddr;
} AGpsStatus_v2;

/**
 * Legacy extended interface for AGPS support.
 * See AGpsInterface_v2 for more information.
 */
typedef struct {
    /** set to sizeof(AGpsInterface_v1) */
    size_t          size;
    void  (*init)( AGpsCallbacks* callbacks );
    int  (*data_conn_open)( const char* apn );
    int  (*data_conn_closed)();
    int  (*data_conn_failed)();
    int  (*set_server)( AGpsType type, const char* hostname, int port );
} AGpsInterface_v1;

/**
 * Legacy struct to represent an estimate of the GPS clock time.
 * See GpsClock_v2 for more details.
 */
typedef struct {
    /** set to sizeof(GpsClock_v1) */
    size_t size;
    GpsClockFlags flags;
    int16_t leap_second;
    GpsClockType type;
    int64_t time_ns;
    double time_uncertainty_ns;
    int64_t full_bias_ns;
    double bias_ns;
    double bias_uncertainty_ns;
    double drift_nsps;
    double drift_uncertainty_nsps;
} GpsClock_v1;

/**
 * Legacy struct to represent a GPS Measurement, it contains raw and computed
 * information.
 * See GpsMeasurement_v2 for more details.
 */
typedef struct {
    /** set to sizeof(GpsMeasurement_v1) */
    size_t size;
    GpsMeasurementFlags flags;
    int8_t prn;
    double time_offset_ns;
    GpsMeasurementState state;
    int64_t received_gps_tow_ns;
    int64_t received_gps_tow_uncertainty_ns;
    double c_n0_dbhz;
    double pseudorange_rate_mps;
    double pseudorange_rate_uncertainty_mps;
    GpsAccumulatedDeltaRangeState accumulated_delta_range_state;
    double accumulated_delta_range_m;
    double accumulated_delta_range_uncertainty_m;
    double pseudorange_m;
    double pseudorange_uncertainty_m;
    double code_phase_chips;
    double code_phase_uncertainty_chips;
    float carrier_frequency_hz;
    int64_t carrier_cycles;
    double carrier_phase;
    double carrier_phase_uncertainty;
    GpsLossOfLock loss_of_lock;
    int32_t bit_number;
    int16_t time_from_last_bit_ms;
    double doppler_shift_hz;
    double doppler_shift_uncertainty_hz;
    GpsMultipathIndicator multipath_indicator;
    double snr_db;
    double elevation_deg;
    double elevation_uncertainty_deg;
    double azimuth_deg;
    double azimuth_uncertainty_deg;
    bool used_in_fix;
} GpsMeasurement_v1;

/** Represents a reading of GPS measurements. */
typedef struct {
    /** set to sizeof(GpsData_v1) */
    size_t size;

    /** Number of measurements. */
    size_t measurement_count;

    /** The array of measurements. */
    GpsMeasurement_v1 measurements[GPS_MAX_MEASUREMENT];

    /** The GPS clock time reading. */
    GpsClock_v1 clock;
} GpsData_v1;

__END_DECLS

#endif /* ANDROID_INCLUDE_HARDWARE_GPS_INTERNAL_H */
