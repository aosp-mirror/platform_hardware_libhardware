#include <hardware/gps.h>
#include <cutils/properties.h>

#define LOG_TAG "libhardware"
#include <utils/Log.h>
#include "qemu.h"

static const GpsInterface*  sGpsInterface = NULL;

static void
gps_find_hardware( void )
{
#ifdef HAVE_QEMU_GPS_HARDWARE
    if (qemu_check()) {
        sGpsInterface = gps_get_qemu_interface();
        if (sGpsInterface) {
            LOGD("using QEMU GPS Hardware emulation\n");
            return;
        }
    }
#endif

#ifdef HAVE_GPS_HARDWARE
    sGpsInterface = gps_get_hardware_interface();
#endif
    if (!sGpsInterface)
        LOGD("no GPS hardware on this device\n");
}

const GpsInterface*
gps_get_interface()
{
    if (sGpsInterface == NULL)
         gps_find_hardware();

    return sGpsInterface;
}
