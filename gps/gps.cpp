#include <hardware/gps.h>
#include <cutils/properties.h>

#define LOG_TAG "libhardware"
#include <utils/Log.h>

static const GpsInterface*  sGpsInterface = NULL;

static void
gps_find_hardware( void )
{
#ifdef HAVE_QEMU_GPS_HARDWARE
    char  propBuf[PROPERTY_VALUE_MAX];

    property_get("ro.kernel.qemu", propBuf, "");
    if (propBuf[0] == '1') {
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
