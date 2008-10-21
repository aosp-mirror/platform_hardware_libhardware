/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Sensors"

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>

#include <linux/input.h>
#include <linux/akm8976.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

/*****************************************************************************/

#define AKM_DEVICE_NAME             "/dev/akm8976_aot"

#define SUPPORTED_SENSORS  (SENSORS_ORIENTATION  |  \
                            SENSORS_ACCELERATION |  \
                            SENSORS_MAGNETIC_FIELD | \
                            SENSORS_ORIENTATION_RAW)


// sensor IDs must be a power of two and
// must match values in SensorManager.java
#define EVENT_TYPE_ACCEL_X          ABS_X
#define EVENT_TYPE_ACCEL_Y          ABS_Z
#define EVENT_TYPE_ACCEL_Z          ABS_Y
#define EVENT_TYPE_ACCEL_STATUS     ABS_WHEEL

#define EVENT_TYPE_YAW              ABS_RX
#define EVENT_TYPE_PITCH            ABS_RY
#define EVENT_TYPE_ROLL             ABS_RZ
#define EVENT_TYPE_ORIENT_STATUS    ABS_RUDDER

#define EVENT_TYPE_MAGV_X           ABS_HAT0X
#define EVENT_TYPE_MAGV_Y           ABS_HAT0Y
#define EVENT_TYPE_MAGV_Z           ABS_BRAKE

#define EVENT_TYPE_TEMPERATURE      ABS_THROTTLE
#define EVENT_TYPE_STEP_COUNT       ABS_GAS

// 720 LSG = 1G
#define LSG                         (720.0f)

// conversion of acceleration data to SI units (m/s^2)
#define CONVERT_A                   (GRAVITY_EARTH / LSG)
#define CONVERT_A_X                 (CONVERT_A)
#define CONVERT_A_Y                 (-CONVERT_A)
#define CONVERT_A_Z                 (CONVERT_A)

// conversion of magnetic data to uT units
#define CONVERT_M                   (1.0f/16.0f)
#define CONVERT_M_X                 (CONVERT_M)
#define CONVERT_M_Y                 (CONVERT_M)
#define CONVERT_M_Z                 (CONVERT_M)

#define SENSOR_STATE_MASK           (0x7FFF)

/*****************************************************************************/

static int sAkmFD = -1;
static uint32_t sActiveSensors = 0;

/*****************************************************************************/

/*
 * We use a Least Mean Squares filter to smooth out the output of the yaw
 * sensor.
 *
 * The goal is to estimate the output of the sensor based on previous acquired
 * samples.
 *
 * We approximate the input by a line with the equation:
 *      Z(t) = a * t + b
 *
 * We use the Least Mean Squares method to calculate a and b so that the
 * distance between the line and the measured COUNT inputs Z(t) is minimal.
 *
 * In practice we only need to compute b, which is the value we're looking for
 * (it's the estimated Z at t=0). However, to improve the latency a little bit,
 * we're going to discard a certain number of samples that are too far from
 * the estimated line and compute b again with the new (trimmed down) samples.
 *
 * notes:
 * 'a' is the slope of the line, and physicaly represent how fast the input
 * is changing. In our case, how fast the yaw is changing, that is, how fast the
 * user is spinning the device (in degre / nanosecond). This value should be
 * zero when the device is not moving.
 *
 * The minimum distance between the line and the samples (which we are not
 * explicitely computing here), is an indication of how bad the samples are
 * and gives an idea of the "quality" of the estimation (well, really of the
 * sensor values).
 *
 */

/* sensor rate in me */
#define SENSORS_RATE_MS     20
/* timeout (constant value) in ms */
#define SENSORS_TIMEOUT_MS  100
/* # of samples to look at in the past for filtering */
#define COUNT               24
/* prediction ratio */
#define PREDICTION_RATIO    (1.0f/3.0f)
/* prediction time in seconds (>=0) */
#define PREDICTION_TIME     ((SENSORS_RATE_MS*COUNT/1000.0f)*PREDICTION_RATIO)

static float mV[COUNT*2];
static float mT[COUNT*2];
static int mIndex;

static inline
float normalize(float x)
{
    x *= (1.0f / 360.0f);
    if (fabsf(x) >= 0.5f)
        x = x - ceilf(x + 0.5f) + 1.0f;
    if (x < 0)
        x += 1.0f;
    x *= 360.0f;
    return x;
}

static void LMSInit(void)
{
    memset(mV, 0, sizeof(mV));
    memset(mT, 0, sizeof(mT));
    mIndex = COUNT;
}

static float LMSFilter(int64_t time, int v)
{
    const float ns = 1.0f / 1000000000.0f;
    const float t = time*ns;
    float v1 = mV[mIndex];
    if ((v-v1) > 180) {
        v -= 360;
    } else if ((v1-v) > 180) {
        v += 360;
    }
    /* Manage the circular buffer, we write the data twice spaced by COUNT
     * values, so that we don't have to memcpy() the array when it's full */
    mIndex++;
    if (mIndex >= COUNT*2)
        mIndex = COUNT;
    mV[mIndex] = v;
    mT[mIndex] = t;
    mV[mIndex-COUNT] = v;
    mT[mIndex-COUNT] = t;

    float A, B, C, D, E;
    float a, b;
    int i;

    A = B = C = D = E = 0;
    for (i=0 ; i<COUNT-1 ; i++) {
        const int j = mIndex - 1 - i;
        const float Z = mV[j];
        const float T = 0.5f*(mT[j] + mT[j+1]) - t;
        float dT = mT[j] - mT[j+1];
        dT *= dT;
        A += Z*dT;
        B += T*(T*dT);
        C +=   (T*dT);
        D += Z*(T*dT);
        E += dT;
    }
    b = (A*B + C*D) / (E*B + C*C);
    a = (E*b - A) / C;
    float f = b + PREDICTION_TIME*a;

    //LOGD("A=%f, B=%f, C=%f, D=%f, E=%f", A,B,C,D,E);
    //LOGD("%lld  %d  %f  %f", time, v, f, a);

    f = normalize(f);
    return f;
}

/*****************************************************************************/

static int open_input()
{
    /* scan all input drivers and look for "compass" */
    int fd = -1;
    const char *dirname = "/dev/input";
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if(dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        fd = open(devname, O_RDONLY);
        if (fd>=0) {
            char name[80];
            if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
                name[0] = '\0';
            }
            if (!strcmp(name, "compass")) {
                LOGD("using %s (name=%s)", devname, name);
                break;
            }
            close(fd);
            fd = -1;
        }
    }
    closedir(dir);

    if (fd < 0) {
        LOGE("Couldn't find or open 'compass' driver (%s)", strerror(errno));
    }
    return fd;
}

static int open_akm()
{
    if (sAkmFD <= 0) {
        sAkmFD = open(AKM_DEVICE_NAME, O_RDONLY);
        LOGD("%s, fd=%d", __PRETTY_FUNCTION__, sAkmFD);
        LOGE_IF(sAkmFD<0, "Couldn't open %s (%s)",
                AKM_DEVICE_NAME, strerror(errno));
        if (sAkmFD >= 0) {
            sActiveSensors = 0;
        }
    }
    return sAkmFD;
}

static void close_akm()
{
    if (sAkmFD > 0) {
        LOGD("%s, fd=%d", __PRETTY_FUNCTION__, sAkmFD);
        close(sAkmFD);
        sAkmFD = -1;
    }
}

static void enable_disable(int fd, uint32_t sensors, uint32_t mask)
{
    if (fd<0) return;
    short flags;
    
    if (sensors & SENSORS_ORIENTATION_RAW) {
        sensors |= SENSORS_ORIENTATION;
        mask |= SENSORS_ORIENTATION;
    } else if (mask & SENSORS_ORIENTATION_RAW) {
        mask |= SENSORS_ORIENTATION;
    }
    
    if (mask & SENSORS_ORIENTATION) {
        flags = (sensors & SENSORS_ORIENTATION) ? 1 : 0;
        if (ioctl(fd, ECS_IOCTL_APP_SET_MFLAG, &flags) < 0) {
            LOGE("ECS_IOCTL_APP_SET_MFLAG error (%s)", strerror(errno));
        }
    }
    if (mask & SENSORS_ACCELERATION) {
        flags = (sensors & SENSORS_ACCELERATION) ? 1 : 0;
        if (ioctl(fd, ECS_IOCTL_APP_SET_AFLAG, &flags) < 0) {
            LOGE("ECS_IOCTL_APP_SET_AFLAG error (%s)", strerror(errno));
        }
    }
    if (mask & SENSORS_TEMPERATURE) {
        flags = (sensors & SENSORS_TEMPERATURE) ? 1 : 0;
        if (ioctl(fd, ECS_IOCTL_APP_SET_TFLAG, &flags) < 0) {
            LOGE("ECS_IOCTL_APP_SET_TFLAG error (%s)", strerror(errno));
        }
    }
#ifdef ECS_IOCTL_APP_SET_MVFLAG
    if (mask & SENSORS_MAGNETIC_FIELD) {
        flags = (sensors & SENSORS_MAGNETIC_FIELD) ? 1 : 0;
        if (ioctl(fd, ECS_IOCTL_APP_SET_MVFLAG, &flags) < 0) {
            LOGE("ECS_IOCTL_APP_SET_MVFLAG error (%s)", strerror(errno));
        }
    }
#endif
}

static uint32_t read_sensors_state(int fd)
{
    if (fd<0) return 0;
    short flags;
    uint32_t sensors = 0;
    // read the actual value of all sensors
    if (!ioctl(fd, ECS_IOCTL_APP_GET_MFLAG, &flags)) {
        if (flags)  sensors |= SENSORS_ORIENTATION;
        else        sensors &= ~SENSORS_ORIENTATION;
    }
    if (!ioctl(fd, ECS_IOCTL_APP_GET_AFLAG, &flags)) {
        if (flags)  sensors |= SENSORS_ACCELERATION;
        else        sensors &= ~SENSORS_ACCELERATION;
    }
    if (!ioctl(fd, ECS_IOCTL_APP_GET_TFLAG, &flags)) {
        if (flags)  sensors |= SENSORS_TEMPERATURE;
        else        sensors &= ~SENSORS_TEMPERATURE;
    }
#ifdef ECS_IOCTL_APP_SET_MVFLAG
    if (!ioctl(fd, ECS_IOCTL_APP_GET_MVFLAG, &flags)) {
        if (flags)  sensors |= SENSORS_MAGNETIC_FIELD;
        else        sensors &= ~SENSORS_MAGNETIC_FIELD;
    }
#endif
    return sensors;
}

/*****************************************************************************/

uint32_t sensors_control_init()
{
    return SUPPORTED_SENSORS;
}

int sensors_control_open()
{
    return open_input();
}

uint32_t sensors_control_activate(uint32_t sensors, uint32_t mask)
{
    mask &= SUPPORTED_SENSORS;
    uint32_t active = sActiveSensors;
    uint32_t new_sensors = (active & ~mask) | (sensors & mask);
    uint32_t changed = active ^ new_sensors;
    if (changed) {
        int fd = open_akm();
        if (fd < 0) return 0;

        if (!active && new_sensors) {
            // force all sensors to be updated
            changed = SUPPORTED_SENSORS;
        }

        enable_disable(fd, new_sensors, changed);

        if (active && !new_sensors) {
            // close the driver
            close_akm();
        }
        sActiveSensors = active = new_sensors;
        LOGD("sensors=%08x, real=%08x",
                sActiveSensors, read_sensors_state(fd));
    }
    return active;
}

int sensors_control_delay(int32_t ms)
{
#ifdef ECS_IOCTL_APP_SET_DELAY
    if (sAkmFD <= 0) {
        return -1;
    }
    short delay = ms;
    if (!ioctl(sAkmFD, ECS_IOCTL_APP_SET_DELAY, &delay)) {
        return -errno;
    }
    return 0;
#else
    return -1;
#endif
}

/*****************************************************************************/

#define MAX_NUM_SENSORS 8
static int sInputFD = -1;
static const int ID_O  = 0;
static const int ID_A  = 1;
static const int ID_T  = 2;
static const int ID_M  = 3;
static const int ID_OR = 7; // orientation raw
static sensors_data_t sSensors[MAX_NUM_SENSORS];
static uint32_t sPendingSensors;

int sensors_data_open(int fd)
{
    int i;
    LMSInit();
    memset(&sSensors, 0, sizeof(sSensors));
    for (i=0 ; i<MAX_NUM_SENSORS ; i++) {
        // by default all sensors have high accuracy
        // (we do this because we don't get an update if the value doesn't
        // change).
        sSensors[i].vector.status = SENSOR_STATUS_ACCURACY_HIGH;
    }
    sPendingSensors = 0;
    sInputFD = dup(fd);
    LOGD("sensors_data_open: fd = %d", sInputFD);
    return 0;
}

int sensors_data_close()
{
    close(sInputFD);
    sInputFD = -1;
    return 0;
}

static int pick_sensor(sensors_data_t* values)
{
    uint32_t mask = SENSORS_MASK;
    while(mask) {
        uint32_t i = 31 - __builtin_clz(mask);
        mask &= ~(1<<i);
        if (sPendingSensors & (1<<i)) {
            sPendingSensors &= ~(1<<i);
            *values = sSensors[i];
            values->sensor = (1<<i);
            LOGD_IF(0, "%d [%f, %f, %f]", (1<<i),
                    values->vector.x,
                    values->vector.y,
                    values->vector.z);
            return (1<<i);
        }
    }
    LOGE("No sensor to return!!! sPendingSensors=%08x", sPendingSensors);
    // we may end-up in a busy loop, slow things down, just in case.
    usleep(100000);
    return -1;
}

int sensors_data_poll(sensors_data_t* values, uint32_t sensors_of_interest)
{
    struct input_event event;
    int nread;
    int64_t t;

    int fd = sInputFD;
    if (fd <= 0)
        return -1;

    // there are pending sensors, returns them now...
    if (sPendingSensors) {
        return pick_sensor(values);
    }

    uint32_t new_sensors = 0;
    struct pollfd fds;
    fds.fd = fd;
    fds.events = POLLIN;
    fds.revents = 0;

    // wait until we get a complete event for an enabled sensor
    while (1) {
        nread = 0;
        if (sensors_of_interest & SENSORS_ORIENTATION) {
            /* We do some special processing if the orientation sensor is
             * activated. In particular the yaw value is filtered with a
             * LMS filter. Since the kernel only sends an event when the
             * value changes, we need to wake up at regular intervals to
             * generate an output value (the output value may not be
             * constant when the input value is constant)
             */
            int err = poll(&fds, 1, SENSORS_TIMEOUT_MS);
            if (err == 0) {
                struct timespec time;
                time.tv_sec = time.tv_nsec = 0;
                clock_gettime(CLOCK_MONOTONIC, &time);

                /* generate an output value */
                t = time.tv_sec*1000000000LL+time.tv_nsec;
                new_sensors |= SENSORS_ORIENTATION;
                sSensors[ID_O].orientation.yaw =
                        LMSFilter(t, sSensors[ID_O].orientation.yaw);

                /* generate a fake sensors event */
                event.type = EV_SYN;
                event.time.tv_sec = time.tv_sec;
                event.time.tv_usec = time.tv_nsec/1000;
                nread = sizeof(event);
            }
        }
        if (nread == 0) {
            /* read the next event */
            nread = read(fd, &event, sizeof(event));
        }
        if (nread == sizeof(event)) {
            uint32_t v;
            if (event.type == EV_ABS) {
                //LOGD("type: %d code: %d value: %-5d time: %ds",
                //        event.type, event.code, event.value,
                //      (int)event.time.tv_sec);
                switch (event.code) {

                    case EVENT_TYPE_ACCEL_X:
                        new_sensors |= SENSORS_ACCELERATION;
                        sSensors[ID_A].acceleration.x = event.value * CONVERT_A_X;
                        break;
                    case EVENT_TYPE_ACCEL_Y:
                        new_sensors |= SENSORS_ACCELERATION;
                        sSensors[ID_A].acceleration.y = event.value * CONVERT_A_Y;
                        break;
                    case EVENT_TYPE_ACCEL_Z:
                        new_sensors |= SENSORS_ACCELERATION;
                        sSensors[ID_A].acceleration.z = event.value * CONVERT_A_Z;
                        break;

                    case EVENT_TYPE_MAGV_X:
                        new_sensors |= SENSORS_MAGNETIC_FIELD;
                        sSensors[ID_M].magnetic.x = event.value * CONVERT_M_X;
                        break;
                    case EVENT_TYPE_MAGV_Y:
                        new_sensors |= SENSORS_MAGNETIC_FIELD;
                        sSensors[ID_M].magnetic.y = event.value * CONVERT_M_Y;
                        break;
                    case EVENT_TYPE_MAGV_Z:
                        new_sensors |= SENSORS_MAGNETIC_FIELD;
                        sSensors[ID_M].magnetic.z = event.value * CONVERT_M_Z;
                        break;

                    case EVENT_TYPE_YAW:
                        new_sensors |= SENSORS_ORIENTATION | SENSORS_ORIENTATION_RAW;
                        t = event.time.tv_sec*1000000000LL +
                                event.time.tv_usec*1000;
                        sSensors[ID_O].orientation.yaw = 
                            (sensors_of_interest & SENSORS_ORIENTATION) ?
                                    LMSFilter(t, event.value) : event.value;
                        sSensors[ID_OR].orientation.yaw = event.value;
                        break;
                    case EVENT_TYPE_PITCH:
                        new_sensors |= SENSORS_ORIENTATION | SENSORS_ORIENTATION_RAW;
                        sSensors[ID_O].orientation.pitch = event.value;
                        sSensors[ID_OR].orientation.pitch = event.value;
                        break;
                    case EVENT_TYPE_ROLL:
                        new_sensors |= SENSORS_ORIENTATION | SENSORS_ORIENTATION_RAW;
                        sSensors[ID_O].orientation.roll = event.value;
                        sSensors[ID_OR].orientation.roll = event.value;
                        break;

                    case EVENT_TYPE_TEMPERATURE:
                        new_sensors |= SENSORS_TEMPERATURE;
                        sSensors[ID_T].temperature = event.value;
                        break;

                    case EVENT_TYPE_STEP_COUNT:
                        // step count (only reported in MODE_FFD)
                        // we do nothing with it for now.
                        break;
                    case EVENT_TYPE_ACCEL_STATUS:
                        // accuracy of the calibration (never returned!)
                        //LOGD("G-Sensor status %d", event.value);
                        break;
                    case EVENT_TYPE_ORIENT_STATUS:
                        // accuracy of the calibration
                        v = (uint32_t)(event.value & SENSOR_STATE_MASK);
                        LOGD_IF(sSensors[ID_O].orientation.status != (uint8_t)v,
                                "M-Sensor status %d", v);
                        sSensors[ID_O].orientation.status = (uint8_t)v;
                        sSensors[ID_OR].orientation.status = (uint8_t)v;
                        break;
                }
            } else if (event.type == EV_SYN) {
                if (new_sensors) {
                    sPendingSensors = new_sensors;
                    int64_t t = event.time.tv_sec*1000000000LL +
                            event.time.tv_usec*1000;
                    while (new_sensors) {
                        uint32_t i = 31 - __builtin_clz(new_sensors);
                        new_sensors &= ~(1<<i);
                        sSensors[i].time = t;
                    }
                    return pick_sensor(values);
                }
            }
        }
    }
}

uint32_t sensors_data_get_sensors() {
    return SUPPORTED_SENSORS;
}

