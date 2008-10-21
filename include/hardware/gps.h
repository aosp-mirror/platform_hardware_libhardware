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

/** milliseconds since January 1, 1970 */
typedef int64_t GpsUtcTime;

/** maximum number of Space Vehicles for gps_sv_status_callback */
#define GPS_MAX_SVS 32


typedef uint16_t GpsPositionMode;
#define GPS_POSITION_MODE_STANDALONE    0
#define GPS_POSITION_MODE_MS_BASED      1
#define GPS_POSITION_MODE_MS_ASSISTED   2

typedef uint16_t GpsStatusValue;
// IMPORTANT - these symbols here must match constants in GpsLocationProvider.java
#define GPS_STATUS_NONE             0
#define GPS_STATUS_SESSION_BEGIN    1
#define GPS_STATUS_SESSION_END      2
#define GPS_STATUS_ENGINE_ON        3
#define GPS_STATUS_ENGINE_OFF       4

typedef uint16_t GpsLocationFlags;
// IMPORTANT - these symbols here must match constants in GpsLocationProvider.java
#define GPS_LOCATION_HAS_LAT_LONG   0x0001
#define GPS_LOCATION_HAS_ALTITUDE   0x0002
#define GPS_LOCATION_HAS_SPEED      0x0004
#define GPS_LOCATION_HAS_BEARING    0x0008
#define GPS_LOCATION_HAS_ACCURACY   0x0010

typedef uint16_t GpsAidingData;
// IMPORTANT - these symbols here must match constants in GpsLocationProvider.java
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
 * names for GPS XTRA interface
 */
#define GPS_XTRA_INTERFACE      "gps-xtra"

/**
 * names for GPS supplemental interface
 * TODO: Remove not used.
 */
#define GPS_SUPL_INTERFACE      "gps-supl"

/** The location */
typedef struct {
    /** contains GpsLocationFlags bits */
    uint16_t        flags;
    double          latitude;
    double          longitude;
    double          altitude;
    float           speed;
    float           bearing;
    float           accuracy;
    GpsUtcTime      timestamp;
} GpsLocation;

/** The status */
typedef struct {
    GpsStatusValue status;
} GpsStatus;

/** Space Vehicle info */
typedef struct {
    int     prn;
    float   snr;
    float   elevation;
    float   azimuth;
} GpsSvInfo;

/** Space Vehicle status */
typedef struct {
        /** number of SVs currently visible */
        int         num_svs;

        /** Array of space vehicle info */
        GpsSvInfo   sv_list[GPS_MAX_SVS];

        /** bit mask indicating which SVs have ephemeris data */
        uint32_t    ephemeris_mask;

        /** bit mask indicating which SVs have almanac data */
        uint32_t    almanac_mask;

        /**
         * bit mask indicating which SVs were used for
         * computing the most recent position fix
         */
        uint32_t    used_in_fix_mask;
} GpsSvStatus;

/** Callback with location information */
typedef void (* gps_location_callback)(GpsLocation* location);

/** Callback with the status information */
typedef void (* gps_status_callback)(GpsStatus* status);

/** Callback with the space vehicle status information */
typedef void (* gps_sv_status_callback)(GpsSvStatus* sv_info);

/** GPS call back structure */
typedef struct {
        gps_location_callback location_cb;
        gps_status_callback status_cb;
        gps_sv_status_callback sv_status_cb;
} GpsCallbacks;


/** Standard GPS interface */
typedef struct {
    /**
     * Open the interface and provide the callback routines
     * to the implemenation of this interface.
     */
    int   (*init)( GpsCallbacks* callbacks );

    /** Start navigating */
    int   (*start)( void );

    /** Stop navigating */
    int   (*stop)( void );

    /** Set requested frequency of fixes in seconds */
    void  (*set_fix_frequency)( int frequency );

    /** Close the interface */
    void  (*cleanup)( void );

    /** Inject the current time */
    int   (*inject_time)(GpsUtcTime time, int64_t timeReference,
                         int uncertainty);

    /**
     * The next call to start will not use the information
     * defined in the flags. GPS_DELETE_ALL  is passed for
     * a cold start.
     */
    void  (*delete_aiding_data)(GpsAidingData flags);

    /**
     * fix_frequency is time between fixes in seconds.
     * set fix_frequency to zero for a single shot fix.
     */
    int   (*set_position_mode)(GpsPositionMode mode, int fix_frequency);

    /** Get a pointer to extension information. */
    const void* (*get_extension)(const char* name);
} GpsInterface;

/** The download request callback routine. */
typedef void (* gps_xtra_download_request)();

/** The download request callback structure. */
typedef struct {
        gps_xtra_download_request download_request_cb;
} GpsXtraCallbacks;

/** Extended interface for XTRA support. */
typedef struct {
    int  (*init)( GpsXtraCallbacks* callbacks );
    int  (*inject_xtra_data)( char* data, int length );
} GpsXtraInterface;

/** returns the hardware GPS interface. */
const GpsInterface* gps_get_hardware_interface();

/**
 * returns the qemu hardware interface GPS interface.
 */
const GpsInterface* gps_get_qemu_interface();

/**
 * returns the default GPS interface,
 * implemented in lib/hardware/gps.cpp.
 */
const GpsInterface* gps_get_interface();

#if __cplusplus
}  // extern "C"
#endif

#endif  // _HARDWARE_GPS_H
