/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <linux/input.h>
#include <cutils/atomic.h>
#include <cutils/log.h>

#include <vector>
#include <map>

#include <stdio.h>
#include <dlfcn.h>

// comment out to disable debug-level logging
#define LOG_NDEBUG 0

static const char* CONFIG_FILENAME = "/system/etc/sensors/hals.conf";
static const char* LEGAL_SUBHAL_PATH_PREFIX = "/system/lib/hw/";
static const int MAX_CONF_LINE_LENGTH = 1024;

static pthread_mutex_t init_modules_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t init_sensors_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Vector of sub modules, whose indexes are referred to ni this file as module_index.
 */
static std::vector<hw_module_t *> *sub_hw_modules = NULL;

/*
 * Comparable class that globally identifies a sensor, by module index and local handle.
 * A module index is the module's index in sub_hw_modules.
 * A local handle is the handle the sub-module assigns to a sensor.
 */
struct FullHandle {
    int moduleIndex;
    int localHandle;

    bool operator<(const FullHandle &that) const {
        if (moduleIndex < that.moduleIndex) {
            return true;
        }
        if (moduleIndex > that.moduleIndex) {
            return false;
        }
        return localHandle < that.localHandle;
    }

    bool operator=(const FullHandle &that) const {
        return moduleIndex == that.moduleIndex && localHandle == that.localHandle;
    }
};

std::map<int, FullHandle> global_to_full;
std::map<FullHandle, int> full_to_global;
int next_global_handle = 1;

static int assign_global_handle(int module_index, int local_handle) {
    ALOGD("assign_global_handle %d %d", module_index, local_handle);
    int global_handle = next_global_handle++;
    FullHandle *full_handle = new FullHandle();
    full_handle->moduleIndex = module_index;
    full_handle->localHandle = local_handle;
    full_to_global[*full_handle] = global_handle;
    global_to_full[global_handle] = *full_handle;
    return global_handle;
}

static int get_local_handle(int global_handle) {
    return global_to_full[global_handle].localHandle;
}

static int get_module_index(int global_handle) {
    ALOGD("get_module_index %d", global_handle);
    FullHandle f = global_to_full[global_handle];
    ALOGD("FullHandle moduleIndex %d, localHandle %d", f.moduleIndex, f.localHandle);
    return f.moduleIndex;
}


/*
 * Cache of all sensors, with original handles replaced by global handles.
 * This will be handled to get_sensors_list() callers.
 */
static struct sensor_t const* global_sensors_list = NULL;
static int global_sensors_count = -1;

/*
 * Extends a sensors_poll_device_1 by including all the sub-module's devices.
 */
struct sensors_poll_context_t {
    /*
     * This is the device that SensorDevice.cpp uses to make API calls
     * to the multihal, which fans them out to sub-HALs.
     */
    sensors_poll_device_1 proxy_device; // must be first

    void addSubHwDevice(struct hw_device_t*);

    int activate(int handle, int enabled);
    int setDelay(int handle, int64_t ns);
    int poll(sensors_event_t* data, int count);
    int batch(int handle, int flags, int64_t period_ns, int64_t timeout);
    int flush(int handle);
    int close();

    std::vector<hw_device_t*> sub_hw_devices;

    sensors_poll_device_t* get_v0_device_by_handle(int global_handle);
    sensors_poll_device_1_t* get_v1_device_by_handle(int global_handle);
    int get_device_version_by_handle(int global_handle);
};

void sensors_poll_context_t::addSubHwDevice(struct hw_device_t* sub_hw_device) {
    ALOGD("addSubHwDevice");
    this->sub_hw_devices.push_back(sub_hw_device);
}

sensors_poll_device_t* sensors_poll_context_t::get_v0_device_by_handle(int handle) {
    ALOGD("get_v0_device_by_handle(%d)", handle);
    int sub_index = get_module_index(handle);
    ALOGD("sub_index: %d", sub_index);
    return (sensors_poll_device_t*) this->sub_hw_devices[sub_index];
}

sensors_poll_device_1_t* sensors_poll_context_t::get_v1_device_by_handle(int handle) {
    int sub_index = get_module_index(handle);
    return (sensors_poll_device_1_t*) this->sub_hw_devices[sub_index];
}

int sensors_poll_context_t::get_device_version_by_handle(int handle) {
    sensors_poll_device_t* v0 = this->get_v0_device_by_handle(handle);
    return v0->common.version;
}

int sensors_poll_context_t::activate(int handle, int enabled) {
    ALOGD("activate");
    sensors_poll_device_t* v0 = this->get_v0_device_by_handle(handle);
    int retval = v0->activate(v0, get_local_handle(handle), enabled);
    ALOGD("retval %d", retval);
    return retval;
}

int sensors_poll_context_t::setDelay(int handle, int64_t ns) {
    ALOGD("setDelay");
    sensors_poll_device_t* v0 = this->get_v0_device_by_handle(handle);
    int retval = v0->setDelay(v0, get_local_handle(handle), ns);
    ALOGD("retval %d", retval);
    return retval;
}

int sensors_poll_context_t::poll(sensors_event_t *data, int count) {
    ALOGD("poll");

    // This only gets the first device. Parallel polling of multiple devices is coming soon.
    int sub_index = 0;
    sensors_poll_device_t* v0 = (sensors_poll_device_t*) this->sub_hw_devices[sub_index];

    ALOGD("poll's blocking read begins...");
    int retval = v0->poll(v0, data, count);
    ALOGD("...poll's blocking read ends");
    ALOGD("rewriting %d sensor handles...", retval);
    // A normal event's "sensor" field is a local handles. Convert it to a global handle.
    // A meta-data event must have its sensor set to 0, but it has a nested event
    // with a local handle that needs to be converted to a global handle.
    FullHandle full_handle;
    full_handle.moduleIndex = sub_index;
    for (int i = 0; i < retval; i++) {
        sensors_event_t *event = &data[i];
        // If it's a metadata event, rewrite the inner payload, not the sensor field.
        if (event->type == SENSOR_TYPE_META_DATA) {
            full_handle.localHandle = event->meta_data.sensor;
            event->meta_data.sensor = full_to_global[full_handle];
        } else {
            full_handle.localHandle = event->sensor;
            event->sensor = full_to_global[full_handle];
        }
    }
    return retval;
}

int sensors_poll_context_t::batch(int handle, int flags, int64_t period_ns, int64_t timeout) {
    ALOGD("batch");
    int retval = -EINVAL;
    int version = this->get_device_version_by_handle(handle);
    if (version >= SENSORS_DEVICE_API_VERSION_1_0) {
        sensors_poll_device_1_t* v1 = this->get_v1_device_by_handle(handle);
        retval = v1->batch(v1, get_local_handle(handle), flags, period_ns, timeout);
    }
    ALOGD("retval %d", retval);
    return retval;
}

int sensors_poll_context_t::flush(int handle) {
    ALOGD("flush");
    int retval = -EINVAL;
    int version = this->get_device_version_by_handle(handle);
    if (version >= SENSORS_DEVICE_API_VERSION_1_0) {
        sensors_poll_device_1_t* v1 = this->get_v1_device_by_handle(handle);
        retval = v1->flush(v1, get_local_handle(handle));
    }
    ALOGD("retval %d", retval);
    return retval;
}

int sensors_poll_context_t::close() {
    ALOGD("close");
    for (std::vector<hw_device_t*>::iterator it = this->sub_hw_devices.begin();
            it != this->sub_hw_devices.end(); it++) {
        hw_device_t* dev = *it;
        int retval = dev->close(dev);
        ALOGD("retval %d", retval);
    }
    return 0;
}


static int device__close(struct hw_device_t *dev) {
    sensors_poll_context_t* ctx = (sensors_poll_context_t*) dev;
    if (ctx != NULL) {
        int retval = ctx->close();
        delete ctx;
    }
    return 0;
}

static int device__activate(struct sensors_poll_device_t *dev, int handle,
        int enabled) {
    sensors_poll_context_t* ctx = (sensors_poll_context_t*) dev;
    return ctx->activate(handle, enabled);
}

static int device__setDelay(struct sensors_poll_device_t *dev, int handle,
        int64_t ns) {
    sensors_poll_context_t* ctx = (sensors_poll_context_t*) dev;
    return ctx->setDelay(handle, ns);
}

static int device__poll(struct sensors_poll_device_t *dev, sensors_event_t* data,
        int count) {
    sensors_poll_context_t* ctx = (sensors_poll_context_t*) dev;
    return ctx->poll(data, count);
}

static int device__batch(struct sensors_poll_device_1 *dev, int handle,
        int flags, int64_t period_ns, int64_t timeout) {
    sensors_poll_context_t* ctx = (sensors_poll_context_t*) dev;
    return ctx->batch(handle, flags, period_ns, timeout);
}

static int device__flush(struct sensors_poll_device_1 *dev, int handle) {
    sensors_poll_context_t* ctx = (sensors_poll_context_t*) dev;
    return ctx->flush(handle);
}

static int open_sensors(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static bool starts_with(const char* s, const char* prefix) {
    if (s == NULL || prefix == NULL) {
        return false;
    }
    size_t s_size = strlen(s);
    size_t prefix_size = strlen(prefix);
    return s_size >= prefix_size && strncmp(s, prefix, prefix_size) == 0;
}

/*
 * Adds valid paths from the config file to the vector passed in.
 * The vector must not be null.
 */
static void get_so_paths(std::vector<char*> *so_paths) {
    FILE *conf_file = fopen(CONFIG_FILENAME, "r");
    if (conf_file == NULL) {
        ALOGD("No multihal config file found at %s", CONFIG_FILENAME);
        return;
    }
    ALOGD("Multihal config file found at %s", CONFIG_FILENAME);
    char *line = NULL;
    size_t len = 0;
    int line_count = 0;
    while (getline(&line, &len, conf_file) != -1) {
        // overwrite trailing eoln with null char
        char* pch = strchr(line, '\n');
        if (pch != NULL) {
            *pch = '\0';
        }
        ALOGD("config file line #%d: '%s'", ++line_count, line);
        char *real_path = realpath(line, NULL);
        if (starts_with(real_path, LEGAL_SUBHAL_PATH_PREFIX)) {
            ALOGD("accepting valid path '%s'", real_path);
            char* compact_line = new char[strlen(real_path) + 1];
            strcpy(compact_line, real_path);
            so_paths->push_back(compact_line);
        } else {
            ALOGD("rejecting path '%s' because it does not start with '%s'",
                    real_path, LEGAL_SUBHAL_PATH_PREFIX);
        }
        free(real_path);
    }
    free(line);
    fclose(conf_file);
    ALOGD("hals.conf contained %d lines", line_count);
}

/*
 * Ensures that the sub-module array is initialized.
 * This can be first called from get_sensors_list or from open_sensors.
 */
static void lazy_init_modules() {
    pthread_mutex_lock(&init_modules_mutex);
    if (sub_hw_modules != NULL) {
        pthread_mutex_unlock(&init_modules_mutex);
        return;
    }
    std::vector<char*> *so_paths = new std::vector<char*>();
    get_so_paths(so_paths);

    // dlopen the module files and cache their module symbols in sub_hw_modules
    sub_hw_modules = new std::vector<hw_module_t *>();
    dlerror(); // clear any old errors
    const char* sym = HAL_MODULE_INFO_SYM_AS_STR;
    for (std::vector<char*>::iterator it = so_paths->begin(); it != so_paths->end(); it++) {
        char* path = *it;
        void* lib_handle = dlopen(path, RTLD_LAZY);
        if (lib_handle == NULL) {
            ALOGD("dlerror(): %s", dlerror());
        } else {
            ALOGD("hal lib was loaded: %s", path);
            ALOGD("Opening symbol \"%s\"", sym);
            // clear old errors
            dlerror();
            struct hw_module_t* module = (hw_module_t*) dlsym(lib_handle, sym);
            const char* error;
            if ((error = dlerror()) != NULL) {
                ALOGD("Error calling dlsym: %s", error);
            } else if (module == NULL) {
                ALOGD("module == NULL");
            } else {
                ALOGD("OK, dlsym()'ed \"%s\"", sym);
                sub_hw_modules->push_back(module);
            }
        }
    }
    pthread_mutex_unlock(&init_modules_mutex);
}

/*
 * Lazy-initializes global_sensors_count, global_sensors_list, and module_sensor_handles.
 */
static void lazy_init_sensors_list() {
    ALOGD("lazy_init_sensors_list");
    pthread_mutex_lock(&init_sensors_mutex);
    if (global_sensors_list != NULL) {
        // already initialized
        pthread_mutex_unlock(&init_sensors_mutex);
        ALOGD("lazy_init_sensors_list - early return");
        return;
    }

    ALOGD("lazy_init_sensors_list needs to do work");
    lazy_init_modules();

    // Count all the sensors, then allocate an array of blanks.
    global_sensors_count = 0;
    const struct sensor_t *subhal_sensors_list;
    for (std::vector<hw_module_t*>::iterator it = sub_hw_modules->begin();
            it != sub_hw_modules->end(); it++) {
        struct sensors_module_t *module = (struct sensors_module_t*) *it;
        global_sensors_count += module->get_sensors_list(module, &subhal_sensors_list);
        ALOGD("increased global_sensors_count to %d", global_sensors_count);
    }

    // The global_sensors_list is full of consts.
    // Manipulate this non-const list, and point the const one to it when we're done.
    sensor_t* mutable_sensor_list = new sensor_t[global_sensors_count];

    // index of the next sensor to set in mutable_sensor_list
    int mutable_sensor_index = 0;
    int module_index = 0;

    for (std::vector<hw_module_t*>::iterator it = sub_hw_modules->begin();
            it != sub_hw_modules->end(); it++) {
        hw_module_t *hw_module = *it;
        ALOGD("examine one module");
        // Read the sub-module's sensor list.
        struct sensors_module_t *module = (struct sensors_module_t*) hw_module;
        int module_sensor_count = module->get_sensors_list(module, &subhal_sensors_list);
        ALOGD("the module has %d sensors", module_sensor_count);

        // Copy the HAL's sensor list into global_sensors_list,
        // with the handle changed to be a global handle.
        for (int i = 0; i < module_sensor_count; i++) {
            ALOGD("examining one sensor");
            const struct sensor_t *local_sensor = &subhal_sensors_list[i];
            int local_handle = local_sensor->handle;
            memcpy(&mutable_sensor_list[mutable_sensor_index], local_sensor,
                sizeof(struct sensor_t));

            // Overwrite the global version's handle with a global handle.
            int global_handle = assign_global_handle(module_index, local_handle);

            mutable_sensor_list[mutable_sensor_index].handle = global_handle;
            ALOGD("module_index %d, local_handle %d, global_handle %d",
                    module_index, local_handle, global_handle);

            mutable_sensor_index++;
        }
        module_index++;
    }
    // Set the const static global_sensors_list to the mutable one allocated by this function.
    global_sensors_list = mutable_sensor_list;

    pthread_mutex_unlock(&init_sensors_mutex);
    ALOGD("end lazy_init_sensors_list");
}

static int module__get_sensors_list(struct sensors_module_t* module,
        struct sensor_t const** list) {
    ALOGD("module__get_sensors_list");
    lazy_init_sensors_list();
    *list = global_sensors_list;
    return global_sensors_count;
}

static struct hw_module_methods_t sensors_module_methods = {
    open : open_sensors
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
    common :{
        tag : HARDWARE_MODULE_TAG,
        version_major : 1,
        version_minor : 0,
        id : SENSORS_HARDWARE_MODULE_ID,
        name : "MultiHal Sensor Module",
        author : "Google, Inc",
        methods : &sensors_module_methods,
        dso : NULL,
        reserved : {0},
    },
    get_sensors_list : module__get_sensors_list
};

static int open_sensors(const struct hw_module_t* hw_module, const char* name,
        struct hw_device_t** hw_device_out) {
    ALOGD("open_sensors begin...");

    lazy_init_modules();

    // Create proxy device, to return later.
    sensors_poll_context_t *dev = new sensors_poll_context_t();
    memset(dev, 0, sizeof(sensors_poll_device_1_t));
    dev->proxy_device.common.tag = HARDWARE_DEVICE_TAG;
    dev->proxy_device.common.version = SENSORS_DEVICE_API_VERSION_1_0;
    dev->proxy_device.common.module = const_cast<hw_module_t*>(hw_module);
    dev->proxy_device.common.close = device__close;
    dev->proxy_device.activate = device__activate;
    dev->proxy_device.setDelay = device__setDelay;
    dev->proxy_device.poll = device__poll;
    dev->proxy_device.batch = device__batch;
    dev->proxy_device.flush = device__flush;

    // Open() the subhal modules. Remember their devices in a vector parallel to sub_hw_modules.
    for (std::vector<hw_module_t*>::iterator it = sub_hw_modules->begin();
            it != sub_hw_modules->end(); it++) {
        sensors_module_t *sensors_module = (sensors_module_t*) *it;
        struct hw_device_t* sub_hw_device;
        int sub_open_result = sensors_module->common.methods->open(*it, name, &sub_hw_device);
        dev->addSubHwDevice(sub_hw_device);
    }

    // Prepare the output param and return
    *hw_device_out = &dev->proxy_device.common;
    ALOGD("...open_sensors end");
    return 0;
}
