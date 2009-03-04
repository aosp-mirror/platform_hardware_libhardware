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

#define LOG_TAG "Overlay"

#include <hardware/hardware.h>
#include <hardware/overlay.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

/*****************************************************************************/

struct overlay_control_context_t {
    struct overlay_control_device_t device;
    /* our private state goes below here */
};

struct overlay_data_context_t {
    struct overlay_data_device_t device;
    /* our private state goes below here */
};

static int overlay_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t overlay_module_methods = {
    open: overlay_device_open
};

const struct overlay_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: OVERLAY_HARDWARE_MODULE_ID,
        name: "Sample Overlay module",
        author: "The Android Open Source Project",
        methods: &overlay_module_methods,
    }
};

/*****************************************************************************/

/*
 * This is the overlay_t object, it is returned to the user and represents
 * an overlay.
 * This handles will be passed across processes and possibly given to other
 * HAL modules (for instance video decode modules).
 */

class overlay_object : public overlay_t {
    
    struct handle_t : public native_handle {
        /* add the data fields we need here, for instance: */
        int width;
        int height;
    };
    
    handle_t mHandle;
    
    static overlay_handle_t getHandleRef(struct overlay_t* overlay) {
        /* returns a reference to the handle, caller doesn't take ownership */
        return &(static_cast<overlay_object *>(overlay)->mHandle);
    }
    
public:
    overlay_object() {
        this->overlay_t::getHandleRef = getHandleRef;
        mHandle.version = sizeof(native_handle);
        mHandle.numFds = 0;
        mHandle.numInts = 2; // extra ints we have in  our handle
    }
};

// ****************************************************************************
// Control module
// ****************************************************************************

static int overlay_get(struct overlay_control_device_t *dev, int name) {
    int result = -1;
    switch (name) {
        case OVERLAY_MINIFICATION_LIMIT:
            result = 0; // 0 = no limit
            break;
        case OVERLAY_MAGNIFICATION_LIMIT:
            result = 0; // 0 = no limit
            break;
        case OVERLAY_SCALING_FRAC_BITS:
            result = 0; // 0 = infinite
            break;
        case OVERLAY_ROTATION_STEP_DEG:
            result = 90; // 90 rotation steps (for instance)
            break;
        case OVERLAY_HORIZONTAL_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
        case OVERLAY_VERTICAL_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
        case OVERLAY_WIDTH_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
        case OVERLAY_HEIGHT_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
    }
    return result;
}

static overlay_t* overlay_createOverlay(struct overlay_control_device_t *dev,
         uint32_t w, uint32_t h, int32_t format) 
{
    /* check the input params, reject if not supported or invalid */
    switch (format) {
        case OVERLAY_FORMAT_RGBA_8888:
        case OVERLAY_FORMAT_RGB_565:
        case OVERLAY_FORMAT_BGRA_8888:
        case OVERLAY_FORMAT_YCbCr_422_SP:
        case OVERLAY_FORMAT_YCbCr_420_SP:
        case OVERLAY_FORMAT_YCbCr_422_I:
        case OVERLAY_FORMAT_YCbCr_420_I:
            break;
        default:
            return NULL;
    }
    
    /* Create overlay object. Talk to the h/w here and adjust to what it can
     * do. the overlay_t returned can  be a C++ object, subclassing overlay_t
     * if needed.
     * 
     * we probably want to keep a list of the overlay_t created so they can
     * all be cleaned up in overlay_close(). 
     */
    return new overlay_object( /* pass needed params here*/ );
}

static void overlay_destroyOverlay(struct overlay_control_device_t *dev,
         overlay_t* overlay) 
{
    /* free resources associated with this overlay_t */
    delete overlay;
}

static int overlay_setPosition(struct overlay_control_device_t *dev,
         overlay_t* overlay, 
         int x, int y, uint32_t w, uint32_t h) {
    /* set this overlay's position (talk to the h/w) */
    return -EINVAL;
}

static int overlay_getPosition(struct overlay_control_device_t *dev,
         overlay_t* overlay, 
         int* x, int* y, uint32_t* w, uint32_t* h) {
    /* get this overlay's position */
    return -EINVAL;
}

static int overlay_setParameter(struct overlay_control_device_t *dev,
         overlay_t* overlay, int param, int value) {
    
    int result = 0;
    /* set this overlay's parameter (talk to the h/w) */
    switch (param) {
        case OVERLAY_ROTATION_DEG:
            /* if only 90 rotations are supported, the call fails
             * for other values */
            break;
        case OVERLAY_DITHER: 
            break;
        case OVERLAY_TRANSFORM: 
            // see OVERLAY_TRANSFORM_*
            break;
        default:
            result = -EINVAL;
            break;
    }
    return result;
}

static int overlay_control_close(struct hw_device_t *dev) 
{
    struct overlay_control_context_t* ctx = (struct overlay_control_context_t*)dev;
    if (ctx) {
        /* free all resources associated with this device here
         * in particular the overlay_handle_t, outstanding overlay_t, etc...
         */
        free(ctx);
    }
    return 0;
}
 
// ****************************************************************************
// Data module
// ****************************************************************************

int overlay_initialize(struct overlay_data_device_t *dev,
        overlay_handle_t handle)
{
    /* 
     * overlay_handle_t should contain all the information to "inflate" this
     * overlay. Typically it'll have a file descriptor, informations about
     * how many buffers are there, etc...
     * It is also the place to mmap all buffers associated with this overlay
     * (see getBufferAddress).
     * 
     * NOTE: this function doesn't take ownership of overlay_handle_t
     * 
     */
    
    return -EINVAL;
}

int overlay_dequeueBuffer(struct overlay_data_device_t *dev,
			  overlay_buffer_t* buf) 
{
    /* blocks until a buffer is available and return an opaque structure
     * representing this buffer.
     */
    return -EINVAL;
}

int overlay_queueBuffer(struct overlay_data_device_t *dev,
        overlay_buffer_t buffer)
{
    /* Mark this buffer for posting and recycle or free overlay_buffer_t. */
    return -EINVAL;
}

void *overlay_getBufferAddress(struct overlay_data_device_t *dev,
        overlay_buffer_t buffer)
{
    /* this may fail (NULL) if this feature is not supported. In that case,
     * presumably, there is some other HAL module that can fill the buffer,
     * using a DSP for instance */
    return NULL;
}

static int overlay_data_close(struct hw_device_t *dev) 
{
    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    if (ctx) {
        /* free all resources associated with this device here
         * in particular all pending overlay_buffer_t if needed.
         * 
         * NOTE: overlay_handle_t passed in initialize() is NOT freed and
         * its file descriptors are not closed (this is the responsibility
         * of the caller).
         */
        free(ctx);
    }
    return 0;
}

/*****************************************************************************/

static int overlay_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, OVERLAY_HARDWARE_CONTROL)) {
        struct overlay_control_context_t *dev;
        dev = (overlay_control_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = overlay_control_close;
        
        dev->device.get = overlay_get;
        dev->device.createOverlay = overlay_createOverlay;
        dev->device.destroyOverlay = overlay_destroyOverlay;
        dev->device.setPosition = overlay_setPosition;
        dev->device.getPosition = overlay_getPosition;
        dev->device.setParameter = overlay_setParameter;

        *device = &dev->device.common;
        status = 0;
    } else if (!strcmp(name, OVERLAY_HARDWARE_DATA)) {
        struct overlay_data_context_t *dev;
        dev = (overlay_data_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = overlay_data_close;
        
        dev->device.initialize = overlay_initialize;
        dev->device.dequeueBuffer = overlay_dequeueBuffer;
        dev->device.queueBuffer = overlay_queueBuffer;
        dev->device.getBufferAddress = overlay_getBufferAddress;
        
        *device = &dev->device.common;
        status = 0;
    }
    return status;
}
