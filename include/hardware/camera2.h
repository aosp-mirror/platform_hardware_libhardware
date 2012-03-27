/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ANDROID_INCLUDE_CAMERA2_H
#define ANDROID_INCLUDE_CAMERA2_H

#include "camera_common.h"

/**
 * Camera device HAL 2.0 [ CAMERA_DEVICE_API_VERSION_2_0 ]
 *
 * EXPERIMENTAL.
 *
 * Supports both the android.hardware.ProCamera and
 * android.hardware.Camera APIs.
 *
 * Camera devices that support this version of the HAL must return
 * CAMERA_DEVICE_API_VERSION_2_0 in camera_device_t.common.version and in
 * camera_info_t.device_version (from camera_module_t.get_camera_info).
 *
 * Camera modules that may contain version 2.0 devices must implement at least
 * version 2.0 of the camera module interface (as defined by
 * camera_module_t.common.module_api_version).
 *
 * See camera_common.h for more details.
 *
 */

__BEGIN_DECLS

/**
 * Output image stream queue management
 */

typedef struct camera2_stream_ops {
    int (*dequeue_buffer)(struct camera2_stream_ops* w,
                          buffer_handle_t** buffer, int *stride);
    int (*enqueue_buffer)(struct camera2_stream_ops* w,
                buffer_handle_t* buffer);
    int (*cancel_buffer)(struct camera2_stream_ops* w,
                buffer_handle_t* buffer);
    int (*set_buffer_count)(struct camera2_stream_ops* w, int count);
    int (*set_crop)(struct camera2_stream_ops *w,
                int left, int top, int right, int bottom);
    // Timestamps are measured in nanoseconds, and must be comparable
    // and monotonically increasing between two frames in the same
    // preview stream. They do not need to be comparable between
    // consecutive or parallel preview streams, cameras, or app runs.
    // The timestamp must be the time at the start of image exposure.
    int (*set_timestamp)(struct camera2_stream_ops *w, int64_t timestamp);
    int (*set_usage)(struct camera2_stream_ops* w, int usage);
    int (*set_swap_interval)(struct camera2_stream_ops *w, int interval);
    int (*get_min_undequeued_buffer_count)(const struct camera2_stream_ops *w,
                int *count);
    int (*lock_buffer)(struct camera2_stream_ops* w,
                buffer_handle_t* buffer);
} camera2_stream_ops_t;

/**
 * Metadata queue management, used for requests sent to HAL module, and for
 * frames produced by the HAL.
 */

typedef struct camera2_metadata_queue_src_ops {
    /**
     * Get count of buffers in queue
     */
    int (*buffer_count)(camera2_metadata_queue_src_ops *q);

    /**
     * Get a metadata buffer from the source. Returns OK if a request is
     * available, placing a pointer to it in next_request.
     */
    int (*dequeue)(camera2_metadata_queue_src_ops *q,
            camera_metadata_t **buffer);
    /**
     * Return a metadata buffer to the source once it has been used
     */
    int (*free)(camera2_metadata_queue_src_ops *q,
            camera_metadata_t *old_buffer);

} camera2_metadata_queue_src_ops_t;

typedef struct camera2_metadata_queue_dst_ops {
    /**
     * Notify destination that the queue is no longer empty
     */
    int (*notify_queue_not_empty)(struct camera2_metadata_queue_dst_ops *);
} camera2_metadata_queue_dst_ops_t;

/* Defined in camera_metadata.h */
typedef struct vendor_tag_query_ops vendor_tag_query_ops_t;

/**
 * Asynchronous notification callback from the HAL, fired for various
 * reasons. Only for information independent of frame capture, or that require
 * specific timing.
 */
typedef void (*camera2_notify_callback)(int32_t msg_type,
        int32_t ext1,
        int32_t ext2,
        void *user);

/**
 * Possible message types for camera2_notify_callback
 */
enum {
    /**
     * A serious error has occurred. Argument ext1 contains the error code, and
     * ext2 and user contain any error-specific information.
     */
    CAMERA2_MSG_ERROR   = 0x0001,
    /**
     * The exposure of a given request has begun. Argument ext1 contains the
     * request id.
     */
    CAMERA2_MSG_SHUTTER = 0x0002
};

/**
 * Error codes for CAMERA_MSG_ERROR
 */
enum {
    /**
     * A serious failure occured. Camera device may not work without reboot, and
     * no further frames or buffer streams will be produced by the
     * device. Device should be treated as closed.
     */
    CAMERA2_MSG_ERROR_HARDWARE_FAULT = 0x0001,
    /**
     * A serious failure occured. No further frames or buffer streams will be
     * produced by the device. Device should be treated as closed. The client
     * must reopen the device to use it again.
     */
    CAMERA2_MSG_ERROR_DEVICE_FAULT =   0x0002,
    /**
     * The camera service has failed. Device should be treated as released. The client
     * must reopen the device to use it again.
     */
    CAMERA2_MSG_ERROR_SERVER_FAULT =   0x0003
};


struct camera2_device;
typedef struct camera2_device_ops {
    /**
     * Input request queue methods
     */
    int (*set_request_queue_ops)(struct camera2_device *,
            camera2_metadata_queue_src_ops *request_queue_src_ops);

    camera2_metadata_queue_dst_ops_t *request_queue_dst_ops;

    /**
     * Input reprocessing queue methods
     */
    int (*set_reprocess_queue_ops)(struct camera2_device *,
            camera2_metadata_queue_src_ops *reprocess_queue_src_ops);

    camera2_metadata_queue_dst_ops_t *reprocess_queue_dst_ops;

    /**
     * Output frame queue methods
     */
    int (*set_frame_queue_ops)(struct camera2_device *,
            camera2_metadata_queue_dst_ops *frame_queue_dst_ops);

    camera2_metadata_queue_src_ops_t *frame_queue_src_ops;

    /**
     * Pass in notification methods
     */
    int (*set_notify_callback)(struct camera2_device *,
            camera2_notify_callback notify_cb);

    /**
     * Number of camera frames being processed by the device
     * at the moment (frames that have had their request dequeued,
     * but have not yet been enqueued onto output pipeline(s) )
     */
    int (*get_in_progress_count)(struct camera2_device *);

    /**
     * Flush all in-progress captures. This includes all dequeued requests
     * (regular or reprocessing) that have not yet placed any outputs into a
     * stream or the frame queue. Partially completed captures must be completed
     * normally. No new requests may be dequeued from the request or
     * reprocessing queues until the flush completes.
     */
    int (*flush_captures_in_progress)(struct camera2_device *);

    /**
     * Camera stream management
     */

    /**
     * Operations on the input reprocessing stream
     */
    camera2_stream_ops_t *reprocess_stream_ops;

    /**
     * Get the number of streams that can be simultaneously allocated.
     * A request may include any allocated pipeline for its output, without
     * causing a substantial delay in frame production.
     */
    int (*get_stream_slot_count)(struct camera2_device *);

    /**
     * Allocate a new stream for use. Requires specifying which pipeline slot
     * to use. Specifies the buffer width, height, and format.
     * Error conditions:
     *  - Allocating an already-allocated slot without first releasing it
     *  - Requesting a width/height/format combination not listed as supported
     *  - Requesting a pipeline slot >= pipeline slot count.
     */
    int (*allocate_stream)(
        struct camera2_device *,
        uint32_t stream_slot,
        uint32_t width,
        uint32_t height,
        int format,
        camera2_stream_ops_t *camera2_stream_ops);

    /**
     * Release a stream. Returns an error if called when
     * get_in_progress_count is non-zero, or if the pipeline slot is not
     * allocated.
     */
    int (*release_stream)(
        struct camera2_device *,
        uint32_t stream_slot);

    /**
     * Release the camera hardware.  Requests that are in flight will be
     * canceled. No further buffers will be pushed into any allocated pipelines
     * once this call returns.
     */
    void (*release)(struct camera2_device *);

    /**
     * Methods to query for vendor extension metadata tag infomation. May be NULL
     * if no vendor extension tags are defined.
     */
    vendor_tag_query_ops *camera_metadata_vendor_tag_ops;

    /**
     * Dump state of the camera hardware
     */
    int (*dump)(struct camera2_device *, int fd);

} camera2_device_ops_t;

typedef struct camera2_device {
    /**
     * common.version must equal CAMERA_DEVICE_API_VERSION_2_0 to identify
     * this device as implementing version 2.0 of the camera device HAL.
     */
    hw_device_t common;
    camera2_device_ops_t *ops;
    void *priv;
} camera2_device_t;

__END_DECLS

#endif /* #ifdef ANDROID_INCLUDE_CAMERA2_H */
