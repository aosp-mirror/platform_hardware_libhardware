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


#ifndef ANDROID_GRALLOC_INTERFACE_H
#define ANDROID_GRALLOC_INTERFACE_H

#include <cutils/native_handle.h>

#include <hardware/hardware.h>

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

/**
 * The id of this module
 */
#define GRALLOC_HARDWARE_MODULE_ID "gralloc"

/**
 * Name of the graphics device to open
 */

#define GRALLOC_HARDWARE_FB0 "fb0"
#define GRALLOC_HARDWARE_GPU0 "gpu0"

enum {
    /* buffer is never read in software */
    GRALLOC_USAGE_SW_READ_NEVER   = 0x00000001,
    /* buffer is rarely read in software */
    GRALLOC_USAGE_SW_READ_RARELY  = 0x00000002,
    /* buffer is often read in software */
    GRALLOC_USAGE_SW_READ_OFTEN   = 0x00000003,
    /* mask for the software read values */
    GRALLOC_USAGE_SW_READ_MASK    = 0x0000000F,
    
    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_NEVER  = 0x00000010,
    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_RARELY = 0x00000020,
    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_OFTEN  = 0x00000030,
    /* mask for the software write values */
    GRALLOC_USAGE_SW_WRITE_MASK   = 0x000000F0,

    /* buffer will be used as an OpenGL ES texture */
    GRALLOC_USAGE_HW_TEXTURE      = 0x00000100,
    /* buffer will be used as an OpenGL ES render target */
    GRALLOC_USAGE_HW_RENDER       = 0x00000200,
    /* buffer will be used by the 2D hardware blitter */
    GRALLOC_USAGE_HW_2D           = 0x00000C00,
    /* buffer will be used with the framebuffer device */
    GRALLOC_USAGE_HW_FB           = 0x00001000,
    /* mask for the software usage bit-mask */
    GRALLOC_USAGE_HW_MASK         = 0x00001F00,
};

enum {
    /* the framebuffer is mapped in memory */
    FRAMEBUFFER_RESERVED0                = 0x00000001,
    FRAMEBUFFER_FLAG_MAPPED              = 0x00000002,
};

/*****************************************************************************/

typedef const native_handle* buffer_handle_t;

/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
struct gralloc_module_t {
    struct hw_module_t common;
    
    /* 
     * The (*map)() method maps the buffer in the caller's address space
     * if this operation is allowed (see below). 
     * Mapped buffers are reference-counted in a given process, that is,
     * if a the buffer is already mapped, this function must return the
     * same address and the internal reference counter is incremented.
     * 
     *  Returns 0 on success or -errno on error.
     */
    
    int (*map)(struct gralloc_module_t const* module,
            buffer_handle_t handle, void** vaddr);

    /*
     * The (*unmap)() method, unmaps the buffer from the caller's address
     * space, if the buffer has been mapped more than once, 
     * the (*unmap)() needs to be called the same number of time before 
     * the buffer is actually unmapped. 
     * 
     * Returns 0 on success or -errno on error.
     */

    int (*unmap)(struct gralloc_module_t const* module, buffer_handle_t handle);

    
    /*
     * The (*lock)() method is called before a buffer is accessed for the 
     * specified usage. This call may block, for instance if the h/w needs
     * to finish rendering or if CPU caches need to be synchronized.
     * 
     * The caller promises to modify ALL PIXELS and ONLY THE PIXELS in the area
     * specified by (l,t,w,h).
     * 
     * The content of the buffer outside of the specified area is NOT modified
     * by this call.
     * 
     */
    
    int (*lock)(struct gralloc_module_t const* module,
            buffer_handle_t handle, int usage,
            int l, int t, int w, int h);

    
    /*
     * The (*unlock)() method must be called after all changes to the buffer
     * are completed.
     */
    
    int (*unlock)(struct gralloc_module_t const* module,
            buffer_handle_t handle);

};

/*****************************************************************************/

/**
 * Every device data structure must begin with hw_device_t
 * followed by module specific public methods and attributes.
 */

struct alloc_device_t {
    struct hw_device_t common;

    /* 
     * (*alloc)() Allocates a buffer in graphic memory with the requested
     * parameters and returns a buffer_handle_t and the stride in pixels to
     * allow the implementation to satisfy hardware constraints on the width
     * of a pixmap (eg: it may have to be multiple of 8 pixels). 
     * The CALLER TAKES OWNERSHIP of the buffer_handle_t.
     * 
     * Returns 0 on success or -errno on error.
     */
    
    int (*alloc)(struct alloc_device_t* dev,
            int w, int h, int format, int usage,
            buffer_handle_t* handle, int* stride);

    /*
     * (*free)() Frees a previously allocated buffer. 
     * Behavior is undefined if the buffer is still mapped in any process,
     * but shall not result in termination of the program or security breaches
     * (allowing a process to get access to another process' buffers).
     * THIS FUNCTION TAKES OWNERSHIP of the buffer_handle_t which becomes
     * invalid after the call. 
     * 
     * Returns 0 on success or -errno on error.
     */
    int (*free)(struct alloc_device_t* dev,
            buffer_handle_t handle);

};


struct framebuffer_device_t {
    struct hw_device_t common;

    /* flags describing some attributes of the framebuffer */
    const uint32_t  flags;
    
    /* dimensions of the framebuffer in pixels */
    const uint32_t  width;
    const uint32_t  height;

    /* frambuffer stride in pixels */
    const int       stride;
    
    /* framebuffer pixel format */
    const int       format;
    
    /* resolution of the framebuffer's display panel in pixel per inch*/
    const float     xdpi;
    const float     ydpi;

    /* framebuffer's display panel refresh rate in frames per second */
    const float     fps;

    /* min swap interval supported by this framebuffer */
    const int       minSwapInterval;

    /* max swap interval supported by this framebuffer */
    const int       maxSwapInterval;

    int reserved[8];
    
    /* 
     * requests a specific swap-interval (same definition than EGL) 
     * 
     * Returns 0 on success or -errno on error.
     */
    int (*setSwapInterval)(struct framebuffer_device_t* window,
            int interval);

    /*
     * sets a rectangle evaluated during (*post)() specifying which area
     * of the buffer passed in (*post)() needs to be posted.
     * 
     * return -EINVAL if width or height <=0, or if left or top < 0 
     */
    int (*setUpdateRect)(struct framebuffer_device_t* window,
            int left, int top, int width, int height);
    
    /*
     * Post <buffer> to the display (display it on the screen)
     * The buffer must have been allocated with the 
     *   GRALLOC_USAGE_HW_FB usage flag.
     * buffer must be the same width and height as the display and must NOT
     * be locked.
     * 
     * The buffer is shown during the next VSYNC. 
     * 
     * If the same buffer is posted again (possibly after some other buffer),
     * post() will block until the the first post is completed.
     *
     * Internally, post() is expected to lock the buffer so that a 
     * subsequent call to gralloc_module_t::(*lock)() with USAGE_RENDER or
     * USAGE_*_WRITE will block until it is safe; that is typically once this
     * buffer is shown and another buffer has been posted.
     *
     * Returns 0 on success or -errno on error.
     */
    int (*post)(struct framebuffer_device_t* dev, buffer_handle_t buffer);

    void* reserved_proc[8];
};


/** convenience API for opening and closing a supported device */

static inline int gralloc_open(const struct hw_module_t* module, 
        struct alloc_device_t** device) {
    return module->methods->open(module, 
            GRALLOC_HARDWARE_GPU0, (struct hw_device_t**)device);
}

static inline int gralloc_close(struct alloc_device_t* device) {
    return device->common.close(&device->common);
}


static inline int framebuffer_open(const struct hw_module_t* module, 
        struct framebuffer_device_t** device) {
    return module->methods->open(module, 
            GRALLOC_HARDWARE_FB0, (struct hw_device_t**)device);
}

static inline int framebuffer_close(struct framebuffer_device_t* device) {
    return device->common.close(&device->common);
}


__END_DECLS

#endif  // ANDROID_ALLOC_INTERFACE_H
