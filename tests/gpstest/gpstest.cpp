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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

extern "C" size_t  dlmalloc_footprint();

#include "hardware/gps.h"

static const GpsInterface* sGpsInterface = NULL;

static bool sDone = false;
static int sFixes = 0;
static int sMaxFixes = 0;
static int sStatus = GPS_STATUS_ENGINE_OFF;

static void location_callback(GpsLocation* location)
{
    printf("Got Fix: latitude: %lf longitude: %lf altitude: %.1lf\n", 
            location->latitude, location->longitude, location->altitude);
    sFixes++;
    if (sMaxFixes > 0 && sFixes >= sMaxFixes) {
        sDone = true;
    }
}

static void status_callback(GpsStatus* status)
{
    switch (status->status) {
        case GPS_STATUS_NONE:
            printf("status: GPS_STATUS_NONE\n");
            break;
        case GPS_STATUS_SESSION_BEGIN:
            printf("status: GPS_STATUS_SESSION_BEGIN\n");
            break;
        case GPS_STATUS_SESSION_END:
            printf("status: GPS_STATUS_SESSION_END\n");
            break;
        case GPS_STATUS_ENGINE_ON:
            printf("status: GPS_STATUS_ENGINE_ON\n");
            break;
        case GPS_STATUS_ENGINE_OFF:
            printf("status: GPS_STATUS_ENGINE_OFF\n");
            break;
        default:
            printf("unknown status: %d\n", status->status);
            break;
    }

    sStatus = status->status;
}

static void sv_status_callback(GpsSvStatus* sv_status)
{
    if (sv_status->num_svs > 0) {
        for (int i = 0; i < sv_status->num_svs; i++) {
            printf("SV: %2d SNR: %.1f Elev: %.1f Azim: %.1f %s %s\n", sv_status->sv_list[i].prn, 
                    sv_status->sv_list[i].snr, sv_status->sv_list[i].elevation, 
                    sv_status->sv_list[i].azimuth,
             ((sv_status->ephemeris_mask & (1 << (sv_status->sv_list[i].prn - 1))) ? "E" : " "), 
             ((sv_status->almanac_mask & (1 << (sv_status->sv_list[i].prn - 1))) ? "A" : " ")
            );
        }
        printf("\n");
    }
}

GpsCallbacks sCallbacks = {
    location_callback,
    status_callback,
    sv_status_callback,
};

int main(int argc, char *argv[])
{
    size_t   initial = dlmalloc_footprint();
    
    if (argc >= 2) {
        sMaxFixes = atoi(argv[1]);
        printf("max fixes: %d\n", sMaxFixes);
    }

    sGpsInterface = gps_get_interface();
    if (!sGpsInterface) {
        fprintf(stderr, "could not get gps interface\n");
        return -1;
    }

    int err = sGpsInterface->init(&sCallbacks);
    if (err) {
        fprintf(stderr, "gps_init failed %d\n", err);
        return err;
    }

    sGpsInterface->start();
    
    while (!sDone) {
        sleep(1);
    }
    
    sGpsInterface->stop();
    
    printf("waiting for GPS to shut down\n");
     while (sStatus != GPS_STATUS_ENGINE_OFF) {
        sleep(1);
    }
   
    sGpsInterface->cleanup();

    size_t   final = dlmalloc_footprint();
    fprintf(stderr, "KO: initial == %d, final == %d\n", initial, final );

    return 0;
}
