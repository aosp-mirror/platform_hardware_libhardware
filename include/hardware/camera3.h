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

#ifndef ANDROID_INCLUDE_CAMERA3_H
#define ANDROID_INCLUDE_CAMERA3_H

#include <system/camera_metadata.h>
#include "camera_common.h"

/**
 * Camera device HAL 3.0 [ CAMERA_DEVICE_API_VERSION_3_0 ]
 *
 * EXPERIMENTAL.
 *
 * Supports the android.hardware.Camera API.
 *
 * Camera devices that support this version of the HAL must return
 * CAMERA_DEVICE_API_VERSION_3_0 in camera_device_t.common.version and in
 * camera_info_t.device_version (from camera_module_t.get_camera_info).
 *
 * Camera modules that may contain version 3.0 devices must implement at least
 * version 2.0 of the camera module interface (as defined by
 * camera_module_t.common.module_api_version).
 *
 * See camera_common.h for more versioning details.
 *
 * Version history:
 *
 * 1.0: Initial Android camera HAL (Android 4.0) [camera.h]:
 *
 *   - Converted from C++ CameraHardwareInterface abstraction layer.
 *
 *   - Supports android.hardware.Camera API.
 *
 * 2.0: Initial release of expanded-capability HAL (Android 4.2) [camera2.h]:
 *
 *   - Sufficient for implementing existing android.hardware.Camera API.
 *
 *   - Allows for ZSL queue in camera service layer
 *
 *   - Not tested for any new features such manual capture control, Bayer RAW
 *     capture, reprocessing of RAW data.
 *
 * 3.0: First revision of expanded-capability HAL:
 *
 *   - Major version change since the ABI is completely different. No change to
 *     the required hardware capabilities or operational model from 2.0.
 *
 *   - Reworked input request and stream queue interfaces: Framework calls into
 *     HAL with next request and stream buffers already dequeued. Sync framework
 *     support is included, necessary for efficient implementations.
 *
 *   - Moved triggers into requests, most notifications into results.
 *
 *   - Consolidated all callbacks into framework into one structure, and all
 *     setup methods into a single initialize() call.
 *
 *   - Made stream configuration into a single call to simplify stream
 *     management. Bidirectional streams replace STREAM_FROM_STREAM construct.
 *
 *   - Limited mode semantics for older/limited hardware devices.
 */

/**
 * Startup and general expected operation sequence:
 *
 * 1. Framework calls camera_module_t->common.open(), which returns a
 *    hardware_device_t structure.
 *
 * 2. Framework inspects the hardware_device_t->version field, and instantiates
 *    the appropriate handler for that version of the camera hardware device. In
 *    case the version is CAMERA_DEVICE_API_VERSION_3_0, the device is cast to
 *    a camera3_device_t.
 *
 * 3. Framework calls camera3_device_t->ops->initialize() with the framework
 *    callback function pointers. This will only be called this one time after
 *    open(), before any other functions in the ops structure are called.
 *
 * 4. The framework calls camera3_device_t->ops->configure_streams() with a list
 *    of input/output streams to the HAL device.
 *
 * 5. The framework allocates gralloc buffers and calls
 *    camera3_device_t->ops->register_stream_buffers() for at least one of the
 *    output streams listed in configure_streams. The same stream is registered
 *    only once.
 *
 * 5. The framework requests default settings for some number of use cases with
 *    calls to camera3_device_t->ops->construct_default_request_settings(). This
 *    may occur any time after step 3.
 *
 * 7. The framework constructs and sends the first capture request to the HAL,
 *    with settings based on one of the sets of default settings, and with at
 *    least one output stream, which has been registered earlier by the
 *    framework. This is sent to the HAL with
 *    camera3_device_t->ops->process_capture_request(). The HAL must block the
 *    return of this call until it is ready for the next request to be sent.
 *
 * 8. The framework continues to submit requests, and possibly call
 *    register_stream_buffers() for not-yet-registered streams, and call
 *    construct_default_request_settings to get default settings buffers for
 *    other use cases.
 *
 * 9. When the capture of a request begins (sensor starts exposing for the
 *    capture), the HAL calls camera3_callback_ops_t->notify() with the SHUTTER
 *    event, including the frame number and the timestamp for start of exposure.
 *
 * 10. After some pipeline delay, the HAL begins to return completed captures to
 *    the framework with camera3_callback_ops_t->process_capture_result(). These
 *    are returned in the same order as the requests were submitted. Multiple
 *    requests can be in flight at once, depending on the pipeline depth of the
 *    camera HAL device.
 *
 * 11. After some time, the framework may stop submitting new requests, wait for
 *    the existing captures to complete (all buffers filled, all results
 *    returned), and then call configure_streams() again. This resets the camera
 *    hardware and pipeline for a new set of input/output streams. Some streams
 *    may be reused from the previous configuration; if these streams' buffers
 *    had already been registered with the HAL, they will not be registered
 *    again. The framework then continues from step 7, if at least one
 *    registered output stream remains (otherwise, step 5 is required first).
 *
 * 12. Alternatively, the framework may call camera3_device_t->common->close()
 *    to end the camera session. This may be called at any time when no other
 *    calls from the framework are active, although the call may block until all
 *    in-flight captures have completed (all results returned, all buffers
 *    filled). After the close call returns, no more calls to the
 *    camera3_callback_ops_t functions are allowed from the HAL. Once the
 *    close() call is underway, the framework may not call any other HAL device
 *    functions.
 *
 * 13. In case of an error or other asynchronous event, the HAL must call
 *    camera3_callback_ops_t->notify() with the appropriate error/event
 *    message. After returning from a fatal device-wide error notification, the
 *    HAL should act as if close() had been called on it. However, the HAL must
 *    either cancel or complete all outstanding captures before calling
 *    notify(), so that once notify() is called with a fatal error, the
 *    framework will not receive further callbacks from the device. Methods
 *    besides close() should return -ENODEV or NULL after the notify() method
 *    returns from a fatal error message.
 */

/**
 * Operational modes:
 *
 * The camera 3 HAL device can implement one of two possible operational modes;
 * limited and full. Full support is expected from new higher-end
 * devices. Limited mode has hardware requirements roughly in line with those
 * for a camera HAL device v1 implementation, and is expected from older or
 * inexpensive devices. Full is a strict superset of limited, and they share the
 * same essential operational flow, as documented above.
 *
 * The HAL must indicate its level of support with the
 * android.info.supportedHardwareLevel static metadata entry, with 0 indicating
 * limited mode, and 1 indicating full mode support.
 *
 * Roughly speaking, limited-mode devices do not allow for application control
 * of capture settings (3A control only), high-rate capture of high-resolution
 * images, raw sensor readout, and support for YUV output streams maximum
 * recording resolution (JPEG only for large images).
 *
 * ** Details of limited mode behavior:
 *
 * - Limited-mode devices do not need to implement accurate synchronization
 *   between capture request settings and the actual image data
 *   captured. Instead, changes to settings may take effect some time in the
 *   future, and possibly not for the same output frame for each settings
 *   entry. Rapid changes in settings may result in some settings never being
 *   used for a capture. However, captures that include high-resolution output
 *   buffers ( > 1080p ) have to use the settings as specified (but see below
 *   for processing rate).
 *
 * - Limited-mode devices do not need to support most of the
 *   settings/result/static info metadata. Full-mode devices must support all
 *   metadata fields listed in TODO. Specifically, only the following settings
 *   are expected to be consumed or produced by a limited-mode HAL device:
 *
 *   android.control.aeAntibandingMode (controls)
 *   android.control.aeExposureCompensation (controls)
 *   android.control.aeLock (controls)
 *   android.control.aeMode (controls)
 *       [OFF means ON_FLASH_TORCH - TODO]
 *   android.control.aeRegions (controls)
 *   android.control.aeTargetFpsRange (controls)
 *   android.control.afMode (controls)
 *       [OFF means infinity focus]
 *   android.control.afRegions (controls)
 *   android.control.awbLock (controls)
 *   android.control.awbMode (controls)
 *       [OFF not supported]
 *   android.control.awbRegions (controls)
 *   android.control.captureIntent (controls)
 *   android.control.effectMode (controls)
 *   android.control.mode (controls)
 *       [OFF not supported]
 *   android.control.sceneMode (controls)
 *   android.control.videoStabilizationMode (controls)
 *   android.control.aeAvailableAntibandingModes (static)
 *   android.control.aeAvailableModes (static)
 *   android.control.aeAvailableTargetFpsRanges (static)
 *   android.control.aeCompensationRange (static)
 *   android.control.aeCompensationStep (static)
 *   android.control.afAvailableModes (static)
 *   android.control.availableEffects (static)
 *   android.control.availableSceneModes (static)
 *   android.control.availableVideoStabilizationModes (static)
 *   android.control.awbAvailableModes (static)
 *   android.control.maxRegions (static)
 *   android.control.sceneModeOverrides (static)
 *   android.control.aeRegions (dynamic)
 *   android.control.aeState (dynamic)
 *   android.control.afMode (dynamic)
 *   android.control.afRegions (dynamic)
 *   android.control.afState (dynamic)
 *   android.control.awbMode (dynamic)
 *   android.control.awbRegions (dynamic)
 *   android.control.awbState (dynamic)
 *   android.control.mode (dynamic)
 *
 *   android.flash.info.available (static)
 *
 *   android.info.supportedHardwareLevel (static)
 *
 *   android.jpeg.gpsCoordinates (controls)
 *   android.jpeg.gpsProcessingMethod (controls)
 *   android.jpeg.gpsTimestamp (controls)
 *   android.jpeg.orientation (controls)
 *   android.jpeg.quality (controls)
 *   android.jpeg.thumbnailQuality (controls)
 *   android.jpeg.thumbnailSize (controls)
 *   android.jpeg.availableThumbnailSizes (static)
 *   android.jpeg.maxSize (static)
 *   android.jpeg.gpsCoordinates (dynamic)
 *   android.jpeg.gpsProcessingMethod (dynamic)
 *   android.jpeg.gpsTimestamp (dynamic)
 *   android.jpeg.orientation (dynamic)
 *   android.jpeg.quality (dynamic)
 *   android.jpeg.size (dynamic)
 *   android.jpeg.thumbnailQuality (dynamic)
 *   android.jpeg.thumbnailSize (dynamic)
 *
 *   android.lens.info.minimumFocusDistance (static)
 *
 *   android.request.id (controls)
 *   android.request.id (dynamic)
 *
 *   android.scaler.cropRegion (controls)
 *       [ignores (x,y), assumes center-zoom]
 *   android.scaler.availableFormats (static)
 *       [RAW not supported]
 *   android.scaler.availableJpegMinDurations (static)
 *   android.scaler.availableJpegSizes (static)
 *   android.scaler.availableMaxDigitalZoom (static)
 *   android.scaler.availableProcessedMinDurations (static)
 *   android.scaler.availableProcessedSizes (static)
 *       [full resolution not supported]
 *   android.scaler.maxDigitalZoom (static)
 *   android.scaler.cropRegion (dynamic)
 *
 *   android.sensor.orientation (static)
 *   android.sensor.timestamp (dynamic)
 *
 *   android.statistics.faceDetectMode (controls)
 *   android.statistics.info.availableFaceDetectModes (static)
 *   android.statistics.faceDetectMode (dynamic)
 *   android.statistics.faceIds (dynamic)
 *   android.statistics.faceLandmarks (dynamic)
 *   android.statistics.faceRectangles (dynamic)
 *   android.statistics.faceScores (dynamic)
 *
 * - Captures in limited mode that include high-resolution (> 1080p) output
 *   buffers may block in process_capture_request() until all the output buffers
 *   have been filled. A full-mode HAL device must process sequences of
 *   high-resolution requests at the rate indicated in the static metadata for
 *   that pixel format. The HAL must still call process_capture_result() to
 *   provide the output; the framework must simply be prepared for
 *   process_capture_request() to block until after process_capture_result() for
 *   that request completes for high-resolution captures for limited-mode
 *   devices.
 *
 */

/**
 * Error management:
 *
 * Camera HAL device ops functions that have a return value will all return
 * -ENODEV / NULL in case of a serious error. This means the device cannot
 * continue operation, and must be closed by the framework. Once this error is
 * returned by some method, or if notify() is called with ERROR_DEVICE, only
 * the close() method can be called successfully. All other methods will return
 * -ENODEV / NULL.
 *
 * Transient errors in image capture must be reported through notify() as follows:
 *
 * - The failure of an entire capture to occur must be reported by the HAL by
 *   calling notify() with ERROR_REQUEST. Individual errors for the result
 *   metadata or the output buffers must not be reported in this case.
 *
 * - If the metadata for a capture cannot be produced, but some image buffers
 *   were filled, the HAL must call notify() with ERROR_RESULT.
 *
 * - If an output image buffer could not be filled, but either the metadata was
 *   produced or some other buffers were filled, the HAL must call notify() with
 *   ERROR_BUFFER for each failed buffer.
 *
 * In each of these transient failure cases, the HAL must still call
 * process_capture_result, with valid output buffer_handle_t. If the result
 * metadata could not be produced, it should be NULL. If some buffers could not
 * be filled, their sync fences must be set to the error state.
 *
 * Invalid input aguments result in -EINVAL from the appropriate methods. In
 * that case, the framework should act as if that call had never been made.
 *
 */

__BEGIN_DECLS

struct camera3_device;

/**********************************************************************
 *
 * Camera3 stream and stream buffer definitions.
 *
 * These structs and enums define the handles and contents of the input and
 * output streams connecting the HAL to various framework and application buffer
 * consumers. Each stream is backed by a gralloc buffer queue.
 *
 */

/**
 * camera3_stream_type_t:
 *
 * The type of the camera stream, which defines whether the camera HAL device is
 * the producer or the consumer for that stream, and how the buffers of the
 * stream relate to the other streams.
 */
typedef enum camera3_stream_type {
    /**
     * This stream is an output stream; the camera HAL device will be
     * responsible for filling buffers from this stream with newly captured or
     * reprocessed image data.
     */
    CAMERA3_STREAM_OUTPUT = 0,

    /**
     * This stream is an input stream; the camera HAL device will be responsible
     * for reading buffers from this stream and sending them through the camera
     * processing pipeline, as if the buffer was a newly captured image from the
     * imager.
     */
    CAMERA3_STREAM_INPUT = 1,

    /**
     * This stream can be used for input and output. Typically, the stream is
     * used as an output stream, but occasionally one already-filled buffer may
     * be sent back to the HAL device for reprocessing.
     *
     * This kind of stream is meant generally for zero-shutter-lag features,
     * where copying the captured image from the output buffer to the
     * reprocessing input buffer would be expensive. The stream will be used by
     * the framework as follows:
     *
     * 1. The framework includes a buffer from this stream as output buffer in a
     *    request as normal.
     *
     * 2. Once the HAL device returns a filled output buffer to the framework,
     *    the framework may do one of two things with the filled buffer:
     *
     * 2. a. The framework uses the filled data, and returns the now-used buffer
     *       to the stream queue for reuse. This behavior exactly matches the
     *       OUTPUT type of stream.
     *
     * 2. b. The framework wants to reprocess the filled data, and uses the
     *       buffer as an input buffer for a request. Once the HAL device has
     *       used the reprocessing buffer, it then returns it to the
     *       framework. The framework then returns the now-used buffer to the
     *       stream queue for reuse.
     *
     * 3. The HAL device will be given the buffer again as an output buffer for
     *    a request at some future point.
     *
     * Note that the HAL will always be reprocessing data it produced.
     *
     */
    CAMERA3_STREAM_BIDIRECTIONAL = 2,

    /**
     * Total number of framework-defined stream types
     */
    CAMERA3_NUM_STREAM_TYPES

} camera3_stream_type_t;

/**
 * camera3_stream_t:
 *
 * A handle to a single camera input or output stream. A stream is defined by
 * the framework by its buffer resolution and format, and additionally by the
 * HAL with the gralloc usage flags and the maximum in-flight buffer count.
 *
 * The stream structures are owned by the framework, but pointers to a
 * camera3_stream passed into the HAL by configure_streams() are valid until the
 * end of the first subsequent configure_streams() call that _does not_ include
 * that camera3_stream as an argument, or until the end of the close() call.
 *
 * All camera3_stream framework-controlled members are immutable once the
 * camera3_stream is passed into configure_streams().  The HAL may only change
 * the HAL-controlled parameters during a configure_streams() call, except for
 * the contents of the private pointer.
 *
 * If a configure_streams() call returns a non-fatal error, all active streams
 * remain valid as if configure_streams() had not been called.
 *
 * The endpoint of the stream is not visible to the camera HAL device.
 */
typedef struct camera3_stream {

    /*****
     * Set by framework before configure_streams()
     */

    /**
     * The type of the stream, one of the camera3_stream_type_t values.
     */
    int stream_type;

    /**
     * The width in pixels of the buffers in this stream
     */
    uint32_t width;

    /**
     * The height in pixels of the buffers in this stream
     */
    uint32_t height;

    /**
     * The pixel format for the buffers in this stream. Format is a value from
     * the HAL_PIXEL_FORMAT_* list in system/core/include/system/graphics.h, or
     * from device-specific headers.
     *
     * If HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED is used, then the platform
     * gralloc module will select a format based on the usage flags provided by
     * the camera device and the other endpoint of the stream.
     *
     * The camera HAL device must inspect the buffers handed to it in the
     * subsequent register_stream_buffers() call to obtain the
     * implementation-specific format details, if necessary.
     */
    int format;

    /*****
     * Set by HAL during configure_streams().
     */

    /**
     * The gralloc usage flags for this stream, as needed by the HAL. The usage
     * flags are defined in gralloc.h (GRALLOC_USAGE_*), or in device-specific
     * headers.
     *
     * For output streams, these are the HAL's producer usage flags. For input
     * streams, these are the HAL's consumer usage flags. The usage flags from
     * the producer and the consumer will be combined together and then passed
     * to the platform gralloc HAL module for allocating the gralloc buffers for
     * each stream.
     */
    uint32_t usage;

    /**
     * The maximum number of buffers the HAL device may need to have dequeued at
     * the same time. The HAL device may not have more buffers in-flight from
     * this stream than this value.
     */
    uint32_t max_buffers;

    /**
     * A handle to HAL-private information for the stream. Will not be inspected
     * by the framework code.
     */
    void *priv;

} camera3_stream_t;

/**
 * camera3_stream_configuration_t:
 *
 * A structure of stream definitions, used by configure_streams(). This
 * structure defines all the output streams and the reprocessing input
 * stream for the current camera use case.
 */
typedef struct camera3_stream_configuration {
    /**
     * The total number of streams requested by the framework.  This includes
     * both input and output streams. The number of streams will be at least 1,
     * and there will be at least one output-capable stream.
     */
    uint32_t num_streams;

    /**
     * An array of camera stream pointers, defining the input/output
     * configuration for the camera HAL device.
     *
     * At most one input-capable stream may be defined (INPUT or BIDIRECTIONAL)
     * in a single configuration.
     *
     * At least one output-capable stream must be defined (OUTPUT or
     * BIDIRECTIONAL).
     */
    camera3_stream_t **streams;

} camera3_stream_configuration_t;

/**
 * camera3_buffer_status_t:
 *
 * The current status of a single stream buffer.
 */
typedef enum camera3_buffer_status {
    /**
     * The buffer is in a normal state, and can be used after waiting on its
     * sync fence.
     */
    CAMERA3_BUFFER_STATUS_OK = 0,

    /**
     * The buffer does not contain valid data, and the data in it should not be
     * used. The sync fence must still be waited on before reusing the buffer.
     */
    CAMERA3_BUFFER_STATUS_ERROR = 1

} camera3_buffer_status_t;

/**
 * camera3_stream_buffer_t:
 *
 * A single buffer from a camera3 stream. It includes a handle to its parent
 * stream, the handle to the gralloc buffer itself, and sync fences
 *
 * The buffer does not specify whether it is to be used for input or output;
 * that is determined by its parent stream type and how the buffer is passed to
 * the HAL device.
 */
typedef struct camera3_stream_buffer {
    /**
     * The handle of the stream this buffer is associated with
     */
    camera3_stream_t *stream;

    /**
     * The native handle to the buffer
     */
    buffer_handle_t *buffer;

    /**
     * Current state of the buffer, one of the camera3_buffer_status_t
     * values. The framework will not pass buffers to the HAL that are in an
     * error state. In case a buffer could not be filled by the HAL, it must
     * have its status set to CAMERA3_BUFFER_STATUS_ERROR when returned to the
     * framework with process_capture_result().
     */
    int status;

    /**
     * The acquire sync fence for this buffer. The HAL must wait on this fence
     * fd before attempting to read from or write to this buffer.
     *
     * The framework may be set to -1 to indicate that no waiting is necessary
     * for this buffer.
     *
     * When the HAL returns an output buffer to the framework with
     * process_capture_result(), the acquire_fence must be set to -1. If the HAL
     * never waits on the acquire_fence due to an error in filling a buffer,
     * when calling process_capture_result() the HAL must set the release_fence
     * of the buffer to be the acquire_fence passed to it by the framework. This
     * will allow the framework to wait on the fence before reusing the buffer.
     *
     * For input buffers, the HAL must not change the acquire_fence field during
     * the process_capture_request() call.
     */
     int acquire_fence;

    /**
     * The release sync fence for this buffer. The HAL must set this fence when
     * returning buffers to the framework, or write -1 to indicate that no
     * waiting is required for this buffer.
     *
     * For the input buffer, the release fence must be set by the
     * process_capture_request() call. For the output buffers, the fences must
     * be set in the output_buffers array passed to process_capture_result().
     *
     */
    int release_fence;

} camera3_stream_buffer_t;

/**
 * camera3_stream_buffer_set_t:
 *
 * The complete set of gralloc buffers for a stream. This structure is given to
 * register_stream_buffers() to allow the camera HAL device to register/map/etc
 * newly allocated stream buffers.
 */
typedef struct camera3_stream_buffer_set {
    /**
     * The stream handle for the stream these buffers belong to
     */
    camera3_stream_t *stream;

    /**
     * The number of buffers in this stream. It is guaranteed to be at least
     * stream->max_buffers.
     */
    uint32_t num_buffers;

    /**
     * The array of gralloc buffer handles for this stream. If the stream format
     * is set to HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, the camera HAL device
     * should inspect the passed-in buffers to determine any platform-private
     * pixel format information.
     */
    buffer_handle_t **buffers;

} camera3_stream_buffer_set_t;

/**
 * camera3_jpeg_blob:
 *
 * Transport header for compressed JPEG buffers in output streams.
 *
 * To capture JPEG images, a stream is created using the pixel format
 * HAL_PIXEL_FORMAT_BLOB, and the static metadata field android.jpeg.maxSize is
 * used as the buffer size. Since compressed JPEG images are of variable size,
 * the HAL needs to include the final size of the compressed image using this
 * structure inside the output stream buffer. The JPEG blob ID field must be set
 * to CAMERA3_JPEG_BLOB_ID.
 *
 * Transport header should be at the end of the JPEG output stream buffer.  That
 * means the jpeg_blob_id must start at byte[android.jpeg.maxSize -
 * sizeof(camera3_jpeg_blob)].  Any HAL using this transport header must
 * account for it in android.jpeg.maxSize.  The JPEG data itself starts at
 * the beginning of the buffer and should be jpeg_size bytes long.
 */
typedef struct camera3_jpeg_blob {
    uint16_t jpeg_blob_id;
    uint32_t jpeg_size;
} camera3_jpeg_blob_t;

enum {
    CAMERA3_JPEG_BLOB_ID = 0x00FF
};

/**********************************************************************
 *
 * Message definitions for the HAL notify() callback.
 *
 * These definitions are used for the HAL notify callback, to signal
 * asynchronous events from the HAL device to the Android framework.
 *
 */

/**
 * camera3_msg_type:
 *
 * Indicates the type of message sent, which specifies which member of the
 * message union is valid.
 *
 */
typedef enum camera3_msg_type {
    /**
     * An error has occurred. camera3_notify_msg.message.error contains the
     * error information.
     */
    CAMERA3_MSG_ERROR = 1,

    /**
     * The exposure of a given request has
     * begun. camera3_notify_msg.message.shutter contains the information
     * the capture.
     */
    CAMERA3_MSG_SHUTTER = 2,

    /**
     * Number of framework message types
     */
    CAMERA3_NUM_MESSAGES

} camera3_msg_type_t;

/**
 * Defined error codes for CAMERA_MSG_ERROR
 */
typedef enum camera3_error_msg_code {
    /**
     * A serious failure occured. No further frames or buffer streams will
     * be produced by the device. Device should be treated as closed. The
     * client must reopen the device to use it again. The frame_number field
     * is unused.
     */
    CAMERA3_MSG_ERROR_DEVICE = 1,

    /**
     * An error has occurred in processing a request. No output (metadata or
     * buffers) will be produced for this request. The frame_number field
     * specifies which request has been dropped. Subsequent requests are
     * unaffected, and the device remains operational.
     */
    CAMERA3_MSG_ERROR_REQUEST = 2,

    /**
     * An error has occurred in producing an output result metadata buffer
     * for a request, but output stream buffers for it will still be
     * available. Subsequent requests are unaffected, and the device remains
     * operational.  The frame_number field specifies the request for which
     * result metadata won't be available.
     */
    CAMERA3_MSG_ERROR_RESULT = 3,

    /**
     * An error has occurred in placing an output buffer into a stream for a
     * request. The frame metadata and other buffers may still be
     * available. Subsequent requests are unaffected, and the device remains
     * operational. The frame_number field specifies the request for which the
     * buffer was dropped, and error_stream contains a pointer to the stream
     * that dropped the frame.u
     */
    CAMERA3_MSG_ERROR_BUFFER = 4,

    /**
     * Number of error types
     */
    CAMERA3_MSG_NUM_ERRORS

} camera3_error_msg_code_t;

/**
 * camera3_error_msg_t:
 *
 * Message contents for CAMERA3_MSG_ERROR
 */
typedef struct camera3_error_msg {
    /**
     * Frame number of the request the error applies to. 0 if the frame number
     * isn't applicable to the error.
     */
    uint32_t frame_number;

    /**
     * Pointer to the stream that had a failure. NULL if the stream isn't
     * applicable to the error.
     */
    camera3_stream_t *error_stream;

    /**
     * The code for this error; one of the CAMERA_MSG_ERROR enum values.
     */
    int error_code;

} camera3_error_msg_t;

/**
 * camera3_shutter_msg_t:
 *
 * Message contents for CAMERA3_MSG_SHUTTER
 */
typedef struct camera3_shutter_msg {
    /**
     * Frame number of the request that has begun exposure
     */
    uint32_t frame_number;

    /**
     * Timestamp for the start of capture. This must match the capture result
     * metadata's sensor exposure start timestamp.
     */
    uint64_t timestamp;

} camera3_shutter_msg_t;

/**
 * camera3_notify_msg_t:
 *
 * The message structure sent to camera3_callback_ops_t.notify()
 */
typedef struct camera3_notify_msg {

    /**
     * The message type. One of camera3_notify_msg_type, or a private extension.
     */
    int type;

    union {
        /**
         * Error message contents. Valid if type is CAMERA3_MSG_ERROR
         */
        camera3_error_msg_t error;

        /**
         * Shutter message contents. Valid if type is CAMERA3_MSG_SHUTTER
         */
        camera3_shutter_msg_t shutter;

        /**
         * Generic message contents. Used to ensure a minimum size for custom
         * message types.
         */
        uint8_t generic[32];
    } message;

} camera3_notify_msg_t;

/**********************************************************************
 *
 * Capture request/result definitions for the HAL process_capture_request()
 * method, and the process_capture_result() callback.
 *
 */

/**
 * camera3_request_template_t:
 *
 * Available template types for
 * camera3_device_ops.construct_default_request_settings()
 */
typedef enum camera3_request_template {
    /**
     * Standard camera preview operation with 3A on auto.
     */
    CAMERA3_TEMPLATE_PREVIEW = 1,

    /**
     * Standard camera high-quality still capture with 3A and flash on auto.
     */
    CAMERA3_TEMPLATE_STILL_CAPTURE = 2,

    /**
     * Standard video recording plus preview with 3A on auto, torch off.
     */
    CAMERA3_TEMPLATE_VIDEO_RECORD = 3,

    /**
     * High-quality still capture while recording video. Application will
     * include preview, video record, and full-resolution YUV or JPEG streams in
     * request. Must not cause stuttering on video stream. 3A on auto.
     */
    CAMERA3_TEMPLATE_VIDEO_SNAPSHOT = 4,

    /**
     * Zero-shutter-lag mode. Application will request preview and
     * full-resolution data for each frame, and reprocess it to JPEG when a
     * still image is requested by user. Settings should provide highest-quality
     * full-resolution images without compromising preview frame rate. 3A on
     * auto.
     */
    CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG = 5,

    /* Total number of templates */
    CAMERA3_TEMPLATE_COUNT,

    /**
     * First value for vendor-defined request templates
     */
    CAMERA3_VENDOR_TEMPLATE_START = 0x40000000

} camera3_request_template_t;

/**
 * camera3_capture_request_t:
 *
 * A single request for image capture/buffer reprocessing, sent to the Camera
 * HAL device by the framework in process_capture_request().
 *
 * The request contains the settings to be used for this capture, and the set of
 * output buffers to write the resulting image data in. It may optionally
 * contain an input buffer, in which case the request is for reprocessing that
 * input buffer instead of capturing a new image with the camera sensor. The
 * capture is identified by the frame_number.
 *
 * In response, the camera HAL device must send a camera3_capture_result
 * structure asynchronously to the framework, using the process_capture_result()
 * callback.
 */
typedef struct camera3_capture_request {
    /**
     * The frame number is an incrementing integer set by the framework to
     * uniquely identify this capture. It needs to be returned in the result
     * call, and is also used to identify the request in asynchronous
     * notifications sent to camera3_callback_ops_t.notify().
     */
    uint32_t frame_number;

    /**
     * The settings buffer contains the capture and processing parameters for
     * the request. As a special case, a NULL settings buffer indicates that the
     * settings are identical to the most-recently submitted capture request. A
     * NULL buffer cannot be used as the first submitted request after a
     * configure_streams() call.
     */
    const camera_metadata_t *settings;

    /**
     * The input stream buffer to use for this request, if any.
     *
     * If input_buffer is NULL, then the request is for a new capture from the
     * imager. If input_buffer is valid, the request is for reprocessing the
     * image contained in input_buffer.
     *
     * In the latter case, the HAL must set the release_fence of the
     * input_buffer to a valid sync fence, or to -1 if the HAL does not support
     * sync, before process_capture_request() returns.
     *
     * The HAL is required to wait on the acquire sync fence of the input buffer
     * before accessing it.
     *
     * Any input buffer included here will have been registered with the HAL
     * through register_stream_buffers() before its inclusion in a request.
     */
    camera3_stream_buffer_t *input_buffer;

    /**
     * The number of output buffers for this capture request. Must be at least
     * 1.
     */
    uint32_t num_output_buffers;

    /**
     * An array of num_output_buffers stream buffers, to be filled with image
     * data from this capture/reprocess. The HAL must wait on the acquire fences
     * of each stream buffer before writing to them. All the buffers included
     * here will have been registered with the HAL through
     * register_stream_buffers() before their inclusion in a request.
     *
     * The HAL takes ownership of the actual buffer_handle_t entries in
     * output_buffers; the framework does not access them until they are
     * returned in a camera3_capture_result_t.
     */
    const camera3_stream_buffer_t *output_buffers;

} camera3_capture_request_t;

/**
 * camera3_capture_result_t:
 *
 * The result of a single capture/reprocess by the camera HAL device. This is
 * sent to the framework asynchronously with process_capture_result(), in
 * response to a single capture request sent to the HAL with
 * process_capture_request().
 *
 * The result structure contains the output metadata from this capture, and the
 * set of output buffers that have been/will be filled for this capture. Each
 * output buffer may come with a release sync fence that the framework will wait
 * on before reading, in case the buffer has not yet been filled by the HAL.
 *
 */
typedef struct camera3_capture_result {
    /**
     * The frame number is an incrementing integer set by the framework in the
     * submitted request to uniquely identify this capture. It is also used to
     * identify the request in asynchronous notifications sent to
     * camera3_callback_ops_t.notify().
    */
    uint32_t frame_number;

    /**
     * The result metadata for this capture. This contains information about the
     * final capture parameters, the state of the capture and post-processing
     * hardware, the state of the 3A algorithms, if enabled, and the output of
     * any enabled statistics units.
     */
    const camera_metadata_t *result;

    /**
     * The number of output buffers used for this capture. Must equal the
     * matching capture request's count.
     */
    uint32_t num_output_buffers;

    /**
     * The handles for the output stream buffers for this capture. They may not
     * yet be filled at the time the HAL calls process_capture_result(); the
     * framework will wait on the release sync fences provided by the HAL before
     * reading the buffers.
     *
     * The HAL must set the stream buffer's release sync fence to a valid sync
     * fd, or to -1 if the buffer has already been filled.
     *
     * If the HAL encounters an error while processing the buffer, and the
     * buffer is not filled, the buffer's status field must be set to
     * CAMERA3_BUFFER_STATUS_ERROR. If the HAL did not wait on the acquire fence
     * before encountering the error, the acquire fence should be copied into
     * the release fence, to allow the framework to wait on the fence before
     * reusing the buffer.
     *
     * The acquire fence must be set to -1 for all output buffers.
     *
     */
     const camera3_stream_buffer_t *output_buffers;

} camera3_capture_result_t;

/**********************************************************************
 *
 * Callback methods for the HAL to call into the framework.
 *
 * These methods are used to return metadata and image buffers for a completed
 * or failed captures, and to notify the framework of asynchronous events such
 * as errors.
 *
 * The framework will not call back into the HAL from within these callbacks,
 * and these calls will not block for extended periods.
 *
 */
typedef struct camera3_callback_ops {

    /**
     * process_capture_result:
     *
     * Send a completed capture result metadata buffer to the framework, along
     * with the possibly completed output stream buffers.
     *
     * Captures must be processed in-order, so that the Nth request submitted
     * will match with the Nth result returned. Only one call to
     * process_capture_request() should be made at a time to ensure correct
     * ordering.
     *
     * The HAL retains ownership of result structure, which only needs to be
     * valid to access during this call. The framework will copy whatever it
     * needs before this call returns.
     *
     * The output buffers do not need to be filled yet; the framework will wait
     * on the stream buffer release sync fence before reading the buffer
     * data. Therefore, this method must be called by the HAL as soon as the
     * result metadata is available, even if some or all of the output buffers
     * are still in processing. The HAL must include valid release sync fences
     * into each output_buffers stream buffer entry, or -1 if it does not
     * support streams or if that stream buffer is already filled.
     *
     * If the result buffer cannot be constructed for a request, the HAL should
     * return a NULL buffer here, but still provide the output buffers and their
     * sync fences. In addition, notify() must be called with an ERROR_RESULT
     * message.
     *
     * If an output buffer cannot be filled, its status field must be set to
     * STATUS_ERROR. In addition, notify() must be called with a ERROR_BUFFER
     * message.
     *
     * If the entire capture has failed, then this method still needs to be
     * called to return the output buffers to the framework. All the buffer
     * statuses should be STATUS_ERROR, and the result metadata should be
     * NULL. In addition, notify() must be called with a ERROR_REQUEST
     * message. In this case, individual ERROR_RESULT/ERROR_BUFFER messages
     * should not be sent.
     *
     */
    void (*process_capture_result)(const struct camera3_callback_ops *,
            const camera3_capture_result_t *result);

    /**
     * notify:
     *
     * Asynchronous notification callback from the HAL, fired for various
     * reasons. Only for information independent of frame capture, or that
     * require specific timing. The ownership of the message structure remains
     * with the HAL, and the msg only needs to be valid for the duration of this
     * call.
     *
     * Multiple threads may call notify() simultaneously.
     */
    void (*notify)(const struct camera3_callback_ops *,
            const camera3_notify_msg_t *msg);

} camera3_callback_ops_t;

/**********************************************************************
 *
 * Camera device operations
 *
 */
typedef struct camera3_device_ops {

    /**
     * initialize:
     *
     * One-time initialization to pass framework callback function pointers to
     * the HAL. Will be called once after a successful open() call, before any
     * other functions are called on the camera3_device_ops structure.
     *
     * Return values:
     *
     *  0:     On successful initialization
     *
     * -ENODEV: If initialization fails. Only close() can be called successfully
     *          by the framework after this.
     */
    int (*initialize)(const struct camera3_device *,
            const camera3_callback_ops_t *callback_ops);

    /**********************************************************************
     * Stream management
     */

    /**
     * configure_streams:
     *
     * Reset the HAL camera device processing pipeline and set up new input and
     * output streams. This call replaces any existing stream configuration with
     * the streams defined in the stream_list. This method will be called at
     * least once after initialize() before a request is submitted with
     * process_capture_request().
     *
     * The stream_list must contain at least one output-capable stream, and may
     * not contain more than one input-capable stream.
     *
     * The stream_list may contain streams that are also in the currently-active
     * set of streams (from the previous call to configure_stream()). These
     * streams will already have valid values for usage, max_buffers, and the
     * private pointer. If such a stream has already had its buffers registered,
     * register_stream_buffers() will not be called again for the stream, and
     * buffers from the stream can be immediately included in input requests.
     *
     * If the HAL needs to change the stream configuration for an existing
     * stream due to the new configuration, it may rewrite the values of usage
     * and/or max_buffers during the configure call. The framework will detect
     * such a change, and will then reallocate the stream buffers, and call
     * register_stream_buffers() again before using buffers from that stream in
     * a request.
     *
     * If a currently-active stream is not included in stream_list, the HAL may
     * safely remove any references to that stream. It will not be reused in a
     * later configure() call by the framework, and all the gralloc buffers for
     * it will be freed after the configure_streams() call returns.
     *
     * The stream_list structure is owned by the framework, and may not be
     * accessed once this call completes. The address of an individual
     * camera3_stream_t structure will remain valid for access by the HAL until
     * the end of the first configure_stream() call which no longer includes
     * that camera3_stream_t in the stream_list argument. The HAL may not change
     * values in the stream structure outside of the private pointer, except for
     * the usage and max_buffers members during the configure_streams() call
     * itself.
     *
     * If the stream is new, the usage, max_buffer, and private pointer fields
     * of the stream structure will all be set to 0. The HAL device must set
     * these fields before the configure_streams() call returns. These fields
     * are then used by the framework and the platform gralloc module to
     * allocate the gralloc buffers for each stream.
     *
     * Before such a new stream can have its buffers included in a capture
     * request, the framework will call register_stream_buffers() with that
     * stream. However, the framework is not required to register buffers for
     * _all_ streams before submitting a request. This allows for quick startup
     * of (for example) a preview stream, with allocation for other streams
     * happening later or concurrently.
     *
     * Preconditions:
     *
     * The framework will only call this method when no captures are being
     * processed. That is, all results have been returned to the framework, and
     * all in-flight input and output buffers have been returned and their
     * release sync fences have been signaled by the HAL. The framework will not
     * submit new requests for capture while the configure_streams() call is
     * underway.
     *
     * Postconditions:
     *
     * The HAL device must configure itself to provide maximum possible output
     * frame rate given the sizes and formats of the output streams, as
     * documented in the camera device's static metadata.
     *
     * Performance expectations:
     *
     * This call is expected to be heavyweight and possibly take several hundred
     * milliseconds to complete, since it may require resetting and
     * reconfiguring the image sensor and the camera processing pipeline.
     * Nevertheless, the HAL device should attempt to minimize the
     * reconfiguration delay to minimize the user-visible pauses during
     * application operational mode changes (such as switching from still
     * capture to video recording).
     *
     * Return values:
     *
     *  0:      On successful stream configuration
     *
     * -EINVAL: If the requested stream configuration is invalid. Some examples
     *          of invalid stream configurations include:
     *
     *          - Including more than 1 input-capable stream (INPUT or
     *            BIDIRECTIONAL)
     *
     *          - Not including any output-capable streams (OUTPUT or
     *            BIDIRECTIONAL)
     *
     *          - Including streams with unsupported formats, or an unsupported
     *            size for that format.
     *
     *          - Including too many output streams of a certain format.
     *
     *          Note that the framework submitting an invalid stream
     *          configuration is not normal operation, since stream
     *          configurations are checked before configure. An invalid
     *          configuration means that a bug exists in the framework code, or
     *          there is a mismatch between the HAL's static metadata and the
     *          requirements on streams.
     *
     * -ENODEV: If there has been a fatal error and the device is no longer
     *          operational. Only close() can be called successfully by the
     *          framework after this error is returned.
     */
    int (*configure_streams)(const struct camera3_device *,
            camera3_stream_configuration_t *stream_list);

    /**
     * register_stream_buffers:
     *
     * Register buffers for a given stream with the HAL device. This method is
     * called by the framework after a new stream is defined by
     * configure_streams, and before buffers from that stream are included in a
     * capture request. If the same stream is listed in a subsequent
     * configure_streams() call, register_stream_buffers will _not_ be called
     * again for that stream.
     *
     * The framework does not need to register buffers for all configured
     * streams before it submits the first capture request. This allows quick
     * startup for preview (or similar use cases) while other streams are still
     * being allocated.
     *
     * This method is intended to allow the HAL device to map or otherwise
     * prepare the buffers for later use. The buffers passed in will already be
     * locked for use. At the end of the call, all the buffers must be ready to
     * be returned to the stream.  The buffer_set argument is only valid for the
     * duration of this call.
     *
     * If the stream format was set to HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
     * the camera HAL should inspect the passed-in buffers here to determine any
     * platform-private pixel format information.
     *
     * Return values:
     *
     *  0:      On successful registration of the new stream buffers
     *
     * -EINVAL: If the stream_buffer_set does not refer to a valid active
     *          stream, or if the buffers array is invalid.
     *
     * -ENOMEM: If there was a failure in registering the buffers. The framework
     *          must consider all the stream buffers to be unregistered, and can
     *          try to register again later.
     *
     * -ENODEV: If there is a fatal error, and the device is no longer
     *          operational. Only close() can be called successfully by the
     *          framework after this error is returned.
     */
    int (*register_stream_buffers)(const struct camera3_device *,
            const camera3_stream_buffer_set_t *buffer_set);

    /**********************************************************************
     * Request creation and submission
     */

    /**
     * construct_default_request_settings:
     *
     * Create capture settings for standard camera use cases.
     *
     * The device must return a settings buffer that is configured to meet the
     * requested use case, which must be one of the CAMERA3_TEMPLATE_*
     * enums. All request control fields must be included.
     *
     * The HAL retains ownership of this structure, but the pointer to the
     * structure must be valid until the device is closed. The framework and the
     * HAL may not modify the buffer once it is returned by this call. The same
     * buffer may be returned for subsequent calls for the same template, or for
     * other templates.
     *
     * Return values:
     *
     *   Valid metadata: On successful creation of a default settings
     *                   buffer.
     *
     *   NULL:           In case of a fatal error. After this is returned, only
     *                   the close() method can be called succesfully by the
     *                   framework.
     */
    const camera_metadata_t* (*construct_default_request_settings)(
            const struct camera3_device *,
            int type);

    /**
     * process_capture_request:
     *
     * Send a new capture request to the HAL. The HAL should not return from
     * this call until it is ready to accept the next request to process. Only
     * one call to process_capture_request() will be made at a time by the
     * framework, and the calls will all be from the same thread. The next call
     * to process_capture_request() will be made as soon as a new request and
     * its associated buffers are available. In a normal preview scenario, this
     * means the function will be called again by the framework almost
     * instantly.
     *
     * The actual request processing is asynchronous, with the results of
     * capture being returned by the HAL through the process_capture_result()
     * call. This call requires the result metadata to be available, but output
     * buffers may simply provide sync fences to wait on. Multiple requests are
     * expected to be in flight at once, to maintain full output frame rate.
     *
     * The framework retains ownership of the request structure. It is only
     * guaranteed to be valid during this call. The HAL device must make copies
     * of the information it needs to retain for the capture processing.
     *
     * The HAL must write the file descriptor for the input buffer's release
     * sync fence into input_buffer->release_fence, if input_buffer is not
     * NULL. If the HAL returns -1 for the input buffer release sync fence, the
     * framework is free to immediately reuse the input buffer. Otherwise, the
     * framework will wait on the sync fence before refilling and reusing the
     * input buffer.
     *
     * Return values:
     *
     *  0:      On a successful start to processing the capture request
     *
     * -EINVAL: If the input is malformed (the settings are NULL when not
     *          allowed, there are 0 output buffers, etc) and capture processing
     *          cannot start. Failures during request processing should be
     *          handled by calling camera3_callback_ops_t.notify().
     *
     * -ENODEV: If the camera device has encountered a serious error. After this
     *          error is returned, only the close() method can be successfully
     *          called by the framework.
     *
     */
    int (*process_capture_request)(const struct camera3_device *,
            camera3_capture_request_t *request);

    /**********************************************************************
     * Miscellaneous methods
     */

    /**
     * get_metadata_vendor_tag_ops:
     *
     * Get methods to query for vendor extension metadata tag infomation. The
     * HAL should fill in all the vendor tag operation methods, or leave ops
     * unchanged if no vendor tags are defined.
     *
     * The definition of vendor_tag_query_ops_t can be found in
     * system/media/camera/include/system/camera_metadata.h.
     *
     */
    void (*get_metadata_vendor_tag_ops)(const struct camera3_device*,
            vendor_tag_query_ops_t* ops);

    /**
     * dump:
     *
     * Print out debugging state for the camera device. This will be called by
     * the framework when the camera service is asked for a debug dump, which
     * happens when using the dumpsys tool, or when capturing a bugreport.
     *
     * The passed-in file descriptor can be used to write debugging text using
     * dprintf() or write(). The text should be in ASCII encoding only.
     */
    void (*dump)(const struct camera3_device *, int fd);

} camera3_device_ops_t;

/**********************************************************************
 *
 * Camera device definition
 *
 */
typedef struct camera3_device {
    /**
     * common.version must equal CAMERA_DEVICE_API_VERSION_3_0 to identify this
     * device as implementing version 3.0 of the camera device HAL.
     */
    hw_device_t common;
    camera3_device_ops_t *ops;
    void *priv;
} camera3_device_t;

__END_DECLS

#endif /* #ifdef ANDROID_INCLUDE_CAMERA3_H */
