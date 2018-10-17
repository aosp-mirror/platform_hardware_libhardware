# V4L2 Camera HALv3

The camera.v4l2 library implements a Camera HALv3 using the
Video For Linux 2 (V4L2) interface. This allows it to theoretically
work with a wide variety of devices, though the limitations of V4L2
introduce some [caveats](#V4L2-Deficiencies), causing this HAL to
not be fully spec-compliant.

## Current status

People are free to use that library if that works for their purpose,
but it's not maintained by Android Camera team. There is another V4L2
camera HAL implementation which is maintained by Android Camera team
starting in Android P. See more information
[here](https://source.android.com/devices/camera/external-usb-cameras).

## Building a Device with the HAL

To ensure the HAL is built for a device, include the following in your
`<device>.mk`:

```
USE_CAMERA_V4L2_HAL := true
PRODUCT_PACKAGES += camera.v4l2
PRODUCT_PROPERTY_OVERRIDES += ro.hardware.camera=v4l2
```

The first line ensures the V4L2 HAL module is visible to the build system.
This prevents checkbuilds on devices that don't have the necessary support
from failing. The product packages tells the build system to include the V4L2
HALv3 library in the system image. The final line tells the hardware manager
to load the V4L2 HAL instead of a default Camera HAL.

## Requirements for Using the HAL

Devices and cameras wishing to use this HAL must meet
the following requirements:

* The camera must support BGR32, YUV420, and JPEG formats.
* The gralloc and other graphics modules used by the device must use
`HAL_PIXEL_FORMAT_RGBA_8888` as the `HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED`

## Understanding the HAL Code

There are three large pieces to the V4L2 Camera HAL: the general HALv3
Camera & HAL code, the specific implementation using V4L2,
and the Metadata system.

For context, you may also wish to read some of the documentation in
libhardware/include/camera3.h about how the framework interacts with the HAL.

### Camera & HAL Interface

The camera and HAL interfaces are implemented by the Camera and
V4L2CameraHAL classes.

The V4L2CameraHAL class deals primarily with initialization of the system.
On creation, it searches /dev/video* nodes for ones with the necessary
capabilities. These are then all presented to the framework as available
for use. Further operations are passed to the individual Cameras as appropriate.

The Camera class implements the general logic for handling the camera -
opening and closing, configuring streams, preparing and tracking requests, etc.
While it handles the logistics surrounding the camera, actual image
capture and settings logic are implemented by calling down into the
[V4L2 Camera](#V4L2-Camera). The Camera (using helper classes) enforces
restrictions given in the [Metadata](#Metadata) initialized by the V4L2Camera,
such as limits on the number of in-flight requests per stream.
Notably, this means you should be able to replace the V4L2 implementation
with something else, and as long as you fill in the metadata correctly the
Camera class should "just work".

### V4L2 Specific Implementation

The V4L2Camera class is the implementation of all the capture functionality.
It includes some methods for the Camera class to verify the setup, but the
bulk of the class is the request queue. The Camera class submits CaptureRequests
as they come in and are verified. The V4L2Camera runs these through a three
stage asynchronous pipeline:

* Acceptance: the V4L2Camera accepts the request, and puts it into waiting to be
picked up by the enqueuer.
* Enqueuing: the V4L2Camera reads the request settings, applies them to the
device, takes a snapshot of the settings, and hands the buffer over to the
V4L2 driver.
* Dequeueing: A completed frame is reclaimed from the driver, and sent
back to the Camera class for final processing (validation, filling in the
result object, and sending the data back to the framework).

Much of this work is aided by the V4L2Wrapper helper class,
which provides simpler inputs and outputs around the V4L2 ioctls
based on their known use by the HAL; filling in common values automatically
and extracting the information useful to the HAL from the results.
This wrapper is also used to expose V4L2 controls to their corresponding
Metadata components.

### Metadata

The Metadata subsystem attempts to organize and simplify handling of
camera metadata (system/media/camera/docs/docs.html). At the top level
is the Metadata class and the PartialMetadataInterface. The Metadata
class provides high level interaction with the individual components -
filling the static metadata, validating, getting, and setting settings,
etc. The Metadata class passes all of these things on to the component
PartialMetadataInterfaces, each of which filter for their specific
metadata components and perform the requested task.

Some generalized metadata classes are provided to simplify common logic
for this filtering and application. At a high level, there are three
types:

* Properties: a static value.
* Controls: dynamically adjustable values, and optionally an
associated static property indicating what allowable values are.
* States: a dynamic read-only value.

The Metadata system uses further interfaces and subclasses to distinguish
the variety of different functionalities necessary for different metadata
tags.

#### Metadata Factory

This V4L2 Camera HAL implementation utilizes a metadata factory method.
This method initializes all the 100+ required metadata components for
basic HAL spec compliance. Most do nothing/report fixed values,
but a few are hooked up to the V4L2 driver.

This HAL was initially designed for use with the Raspberry Pi camera module
v2.1, so the fixed defaults are usually assigned based on that camera.

## V4L2 Deficiencies

* One stream at a time is supported. Notably, this means you must re-configure
the stream between preview and capture if they're not the same format.
This makes this HAL not backwards compatible with the Android Camera (v1) API
as many of its methods attempt to do just that; Camera2 must be used instead.
* A variety of metadata properties can't be filled in from V4L2,
such as physical properties of the camera. Thus this HAL will never be capable
of providing perfectly accurate information for all cameras it can theoretically
support.
* Android requires HALs support YUV420, JPEG, and a format of the graphics
stack's choice ("implementation defined"). Very few cameras actually support
all of these formats (so far the Raspberry Pi cameras are the only known ones),
so some form of format conversion built in to the HAL would be a useful feature
to expand the reach/usefulness of this HAL.
* V4L2 doesn't make promises about how fast settings will apply, and there's no
good way to determine what settings were in effect for a given frame. Thus,
the settings passed into requests and out with results are applied/read as
a best effort and may be incorrect.
* Many features V4L2 is capable of are not hooked up to the HAL, so the HAL
is underfeatured compared to the ideal/what is possible.

## Other Known Issues

* A variety of features are unimplemented: High speed capture,
flash torch mode, hotplugging/unplugging.
