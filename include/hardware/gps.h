/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef _HARDWARE_GPS_H
#define _HARDWARE_GPS_H

#include <stdint.h>

#if __cplusplus
extern "C" {
#endif

/** Milliseconds since January 1, 1970 */
typedef int64_t GpsUtcTime;

/** Maximum number of SVs for gps_sv_status_callback(). */
#define GPS_MAX_SVS 32

/** Requested mode for GPS operation. */
typedef uint16_t GpsPositionMode;
// IMPORTANT: Note that the following values must match
// constants in GpsLocationProvider.java.
/** Mode for running GPS standalone (no assistance). */
#define GPS_POSITION_MODE_STANDALONE    0
/** SUPL MS-Based mode. */
#define GPS_POSITION_MODE_MS_BASED      1
/** SUPL MS-Assisted mode. */
#define GPS_POSITION_MODE_MS_ASSISTED   2

/** GPS status event values. */
typedef uint16_t GpsStatusValue;
// IMPORTANT: Note that the following values must match
// constants in GpsLocationProvider.java.
/** GPS status unknown. */
#define GPS_STATUS_NONE             0
/** GPS has begun navigating. */
#define GPS_STATUS_SESSION_BEGIN    1
/** GPS has stopped navigating. */
#define GPS_STATUS_SESSION_END      2
/** GPS has powered on but is not navigating. */
#define GPS_STATUS_ENGINE_ON        3
/** GPS is powered off. */
#define GPS_STATUS_ENGINE_OFF       4

/** Flags to indicate which values are valid in a GpsLocation. */
typedef uint16_t GpsLocationFlags;
// IMPORTANT: Note that the following values must match
// constants in GpsLocationProvider.java.
/** GpsLocation has valid latitude and longitude. */
#define GPS_LOCATION_HAS_LAT_LONG   0x0001
/** GpsLocation has valid altitude. */
#define GPS_LOCATION_HAS_ALTITUDE   0x0002
/** GpsLocation has valid speed. */
#define GPS_LOCATION_HAS_SPEED      0x0004
/** GpsLocation has valid bearing. */
#define GPS_LOCATION_HAS_BEARING    0x0008
/** GpsLocation has valid accuracy. */
#define GPS_LOCATION_HAS_ACCURACY   0x0010

/** Flags used to specify which aiding data to delete
    when calling delete_aiding_data(). */
typedef uint16_t GpsAidingData;
// IMPORTANT: Note that the following values must match
// constants in GpsLocationProvider.java.
#define GPS_DELETE_EPHEMERIS        0x0001
#define GPS_DELETE_ALMANAC          0x0002
#define GPS_DELETE_POSITION         0x0004
#define GPS_DELETE_TIME             0x0008
#define GPS_DELETE_IONO             0x0010
#define GPS_DELETE_UTC              0x0020
#define GPS_DELETE_HEALTH           0x0040
#define GPS_DELETE_SVDIR            0x0080
#define GPS_DELETE_SVSTEER          0x0100
#define GPS_DELETE_SADATA           0x0200
#define GPS_DELETE_RTI              0x0400
#define GPS_DELETE_CELLDB_INFO      0x8000
#define GPS_DELETE_ALL              0xFFFF

/**
 * Name for the GPS XTRA interface.
 */
#define GPS_XTRA_INTERFACE      "gps-xtra"

/**
 * Name for the GPS SUPL interface.
 */
#define GPS_SUPL_INTERFACE      "gps-supl"

/** Represents a location. */
typedef struct {
    /** Contains GpsLocationFlags bits. */
    uint16_t        flags;
    /** Represents latitude in degrees. */
    double          latitude;
    /** Represents longitude in degrees. */
    double          longitude;
    /** Represents altitude in meters above the WGS 84 reference
     * ellipsoid. */
    double          altitude;
    /** Represents speed in meters per second. */
    float           speed;
    /** Represents heading in degrees. */
    float           bearing;
    /** Represents expected accuracy in meters. */
    float           accuracy;
    /** Timestamp for the location fix. */
    GpsUtcTime      timestamp;
} GpsLocation;

/** Represents the status. */
typedef struct {
    GpsStatusValue status;
} GpsStatus;

/** Represents SV information. */
typedef struct {
    /** Pseudo-random number for the SV. */
    int     prn;
    /** Signal to noise ratio. */
    float   snr;
    /** Elevation of SV in degrees. */
    float   elevation;
    /** Azimuth of SV in degrees. */
    float   azimuth;
} GpsSvInfo;

/** Represents SV status. */
typedef struct {
        /** Number of SVs currently visible. */
        int         num_svs;

        /** Contains an array of SV information. */
        GpsSvInfo   sv_list[GPS_MAX_SVS];

        /** Represents a bit mask indicating which SVs
         * have ephemeris data.
         */
        uint32_t    ephemeris_mask;

        /** Represents a bit mask indicating which SVs
         * have almanac data.
         */
        uint32_t    almanac_mask;

        /**
         * Represents a bit mask indicating which SVs
         * were used for computing the most recent position fix.
         */
        uint32_t    used_in_fix_mask;
} GpsSvStatus;

/** Callback with location information. */
typedef void (* gps_location_callback)(GpsLocation* location);

/** Callback with status information. */
typedef void (* gps_status_callback)(GpsStatus* status);

/** Callback with SV status information. */
typedef void (* gps_sv_status_callback)(GpsSvStatus* sv_info);

/** GPS callback structure. */
typedef struct {
        gps_location_callback location_cb;
        gps_status_callback status_cb;
        gps_sv_status_callback sv_status_cb;
} GpsCallbacks;


/** Represents the standard GPS interface. */
typedef struct {
    /**
     * Opens the interface and provides the callback routines
     * to the implemenation of this interface.
     */
    int   (*init)( GpsCallbacks* callbacks );

    /** Starts navigating. */
    int   (*start)( void );

    /** Stops navigating. */
    int   (*stop)( void );

    /** Sets requested frequency of fixes in seconds. */
    void  (*set_fix_frequency)( int frequency );

    /** Closes the interface. */
    void  (*cleanup)( void );

    /** Injects the current time. */
    int   (*inject_time)(GpsUtcTime time, int64_t timeReference,
                         int uncertainty);

    /**
     * Specifies that the next call to start will not use the
     * information defined in the flags. GPS_DELETE_ALL is passed for
     * a cold start.
     */
    void  (*delete_aiding_data)(GpsAidingData flags);

    /**
     * fix_frequency represents the time between fixes in seconds.
     * Set fix_frequency to zero for a single-shot fix.
     */
    int   (*set_position_mode)(GpsPositionMode mode, int fix_frequency);

    /** Get a pointer to extension information. */
    const void* (*get_extension)(const char* name);
} GpsInterface;

/** Callback to request the client to download XTRA data.
    The client should download XTRA data and inject it by calling
     inject_xtra_data(). */
typedef void (* gps_xtra_download_request)();

/** Callback structure for the XTRA interface. */
typedef struct {
        gps_xtra_download_request download_request_cb;
} GpsXtraCallbacks;

/** Extended interface for XTRA support. */
typedef struct {
    /**
     * Opens the XTRA interface and provides the callback routines
     * to the implemenation of this interface.
     */
    int  (*init)( GpsXtraCallbacks* callbacks );
    /** Injects XTRA data into the GPS. */
    int  (*inject_xtra_data)( char* data, int length );
} GpsXtraInterface;

/** Returns the hardware GPS interface. */
const GpsInterface* gps_get_hardware_interface();

/**
 * Returns the qemu emulated GPS interface.
 */
const GpsInterface* gps_get_qemu_interface();

/**
 * Returns the default GPS interface.
 */
const GpsInterface* gps_get_interface();

#if __cplusplus
}  // extern "C"
#endif

#endif  // _HARDWARE_GPS_H
