# Usage Examples

GStreamer provides a larges selection of plugins, which can be used to define flexible pipelines for
many uses. Some examples of common goals are provided in this file.

## `vmbsrc` element properties

The examples below set camera features by passing them as properties directly on the `vmbsrc`
element, e.g. `vmbsrc camera=DEV_1AB22D01BBB8 gain=10 ! ...`. The full, up to date list can always be
queried with `gst-inspect-1.0 vmbsrc`; the table below is a quick reference.

| Property                | Type   | Default    | Description                                                                                          |
|--------------------------|--------|------------|-------------------------------------------------------------------------------------------------------|
| `camera`                | string | `""`       | ID of the camera images should be recorded from                                                       |
| `settingsfile`          | string | `""`       | Path to an XML file with camera settings to load. Mutually exclusive with `userset` and the individual feature properties below |
| `userset`               | string | `""`       | Name of a camera user set (e.g. `UserSet1`) to load via `UserSetSelector`/`UserSetLoad`. Mutually exclusive with `settingsfile` and the individual feature properties below |
| `exposuretime`          | double | `-1`       | `ExposureTime` in microseconds. Only applied when `ExposureAuto` is `Off`. `-1` leaves the camera's currently applied value unchanged |
| `exposureauto`          | enum   | `UNCHANGED`| `ExposureAuto` mode: `UNCHANGED`, `Off`, `Once`, `Continuous`                                          |
| `balancewhiteauto`      | enum   | `UNCHANGED`| `BalanceWhiteAuto` mode: `UNCHANGED`, `Off`, `Once`, `Continuous`                                      |
| `gain`                  | double | `-1`       | `Gain`, as an absolute physical value. `-1` leaves the camera's currently applied value unchanged      |
| `offsetx`               | int    | `G_MAXINT` | `OffsetX` in pixels. `-1` centers the ROI horizontally on the sensor. `G_MAXINT` leaves the camera's currently applied value unchanged |
| `offsety`               | int    | `G_MAXINT` | `OffsetY` in pixels. `-1` centers the ROI vertically on the sensor. `G_MAXINT` leaves the camera's currently applied value unchanged |
| `width`                 | int    | `-1`       | `Width` in pixels. `-1` leaves the camera's currently applied value unchanged                          |
| `height`                | int    | `-1`       | `Height` in pixels. `-1` leaves the camera's currently applied value unchanged                         |
| `triggerselector`       | enum   | `UNCHANGED`| `TriggerSelector`: `UNCHANGED`, `AcquisitionStart`, `AcquisitionEnd`, `AcquisitionActive`, `FrameStart`, `FrameEnd`, `FrameActive`, `FrameBurstStart`, `FrameBurstEnd`, `FrameBurstActive`, `LineStart`, `ExposureStart`, `ExposureEnd`, `ExposureActive` |
| `triggermode`           | enum   | `UNCHANGED`| `TriggerMode` for the selected trigger: `UNCHANGED`, `Off`, `On`                                       |
| `triggersource`         | enum   | `UNCHANGED`| `TriggerSource` for the selected trigger: `UNCHANGED`, `Line0`, `Line1`, `Line2`, `Line3`, `Action0`, `Action1`, `Action2`, `Action3`. More complex sources (e.g. `Software`, counters, timers) require an XML settings file |
| `triggeractivation`     | enum   | `UNCHANGED`| `TriggerActivation` for the selected trigger: `UNCHANGED`, `RisingEdge`, `FallingEdge`, `AnyEdge`, `LevelHigh`, `LevelLow` |
| `incompleteframehandling`| enum  | `Drop`     | How incomplete frames are handled: `Drop`, `Submit`                                                    |
| `allocationmode`        | enum   | `AnnounceFrame` | Frame buffer allocation strategy: `AnnounceFrame`, `AllocAndAnnounceFrame`                        |
| `framebuffers`          | int    | `5`        | Number of frame buffers allocated for transmission from the device to the host                        |

Since `triggerselector`, `triggermode`, `triggersource` and `triggeractivation` each default to
`UNCHANGED`, setting up a trigger requires passing all of them together, e.g.
```
gst-launch-1.0 vmbsrc camera=DEV_1AB22D01BBB8 triggerselector=FrameStart triggermode=On triggersource=Line1 triggeractivation=RisingEdge ! ...
```

For details on `settingsfile` and `userset`, including why they cannot be combined with the other
feature properties, see the README.

## Saving camera frames as images

Recording pictures from a camera and saving them to some common image format allows for quick
inspections of the field of view, brightness and sharpness of the image. GStreamer provides image
encoders for different image formats. The example below uses the `png` encoder. A step by step
explanation of the elements in the pipeline is given below.
```
gst-launch-1.0 vmbsrc camera=DEV_1AB22D01BBB8 num-buffers=1 ! pngenc ! filesink location=out.png
```

- `vmbsrc camera=DEV_1AB22D01BBB8 num-buffers=1`: uses the `vmbsrc` element to grab one single frame
  from the camera with the given ID and halt the pipeline afterwards
- `pngenc`: takes the input image and encodes it into a `png` file
- `filesink location=out.png`: writes the data it receives (the encoded `png` image) to the file
  `out.png` in the current working directory

Similarly it is possible to save a number of camera frames to separate image files. This can be
achieved by using the `multifilesink` element to save the images.
```
gst-launch-1.0 vmbsrc camera=DEV_1AB22D01BBB8 num-buffers=10 ! pngenc ! multifilesink location=out_%03d.png
```

Similarly to the previous example, this pipeline uses the `vmbsrc` element to record images from the
camera. Here however 10 images are recorded. The `multifilesink` saves these images to separate
files, named `out_000.png`, `out_001.png`, ... , `out_009.png`.

Further changes to the pipeline are possible to, for example, change the format of the recorded
images to ensure RGB images, or adjust the exposure time of the image acquisition process. For more
details see the README of the `vmbsrc` element.

## Saving camera stream to a video file

To save a stream of images recorded by a camera to a video file the images should be encoded in some
video format and stored in an appropriate container format. This saves a lot of space compared to
just saving the raw image data. This example uses `h264` encoding for the image data and saves the
resulting video to an `avi` file. An explanation for the separate elements of the pipeline can be
found below.
```
gst-launch-1.0 vmbsrc camera=DEV_000F315B91E2 ! video/x-raw,format=RGB ! videorate ! video/x-raw,framerate=30/1 ! videoconvert ! queue ! x264enc ! avimux ! filesink location=output.avi
```

- `vmbsrc camera=DEV_000F315B91E2`: uses the `vmbsrc` element to grab camera frames from the Vimba X
  compatible camera with the given ID. For more information on the functionality of `vmbsrc`, see
  the README
- `video/x-raw,format=RGB`: a gst capsfilter element that limits the available data formats to `RGB`
  to ensure color images for the resulting video stream. Without this, the pipeline may negotiate
  grayscale images
- `videorate ! video/x-raw,framerate=30/1`: `vmbsrc` provides image data in a variable framerate
  (due to effects like possible hardware triggers or frame jitter). Because `avi` files only support
  fixed framerates, it needs to be modified via the `videorate` plugin. This guarantees a fixed
  framerate output by either copying the input data if more frames are requested than received, or
  dropping unnecessary frames if more frames are received than requested.
- `videoconvert ! queue`: converts the input image to a compatible video format for the following
  element
- `x264enc`: performs the encoding to h264 video
- `avimux`: multiplex the incoming video stream to save it as an `avi` file
- `filesink location=output.avi`: saves the resulting video into a file named `output.avi` in the
  current working directory

## Using  NVMM Memory for accelerated processing with Nvidia elements

If vmbsrc was compiled on a system that had the required libraries available and is able to load the
required libraries at runtime, it can produce frames that are stored in NvMM memory. These can be
passed to accelerated pipeline elements from Nvidia to speed up processing. The easiest way to
ensure that this is done is by using a `capsfilter` element in the pipeline:
```
gst-launch-1.0 vmbsrc camera=DEV_1AB22D01BBB8 ! video/x-raw(memory:NVMM) ! ...
```

## Increasing the number of framebuffers

If pipelines take a long time to process frames passed to them the vmbsrc might run out of buffers
to use for transmissions from the device. In such situations it can be helpful to increase the
number of buffers used by the element. For this the `framebuffers` property can be used:
```
gst-launch-1.0 vmbsrc camera=DEV_1AB22D01BBB8 framebuffers=25 ! ...
```

This property should not be confused with `num-buffers`, which is a standard GStreamer element
property that specifies the number of images the element should produce before halting execution.

## Stream video via RTSP server

RTSP (Real Time Streaming Protocol) is a network protocol designed to control streaming media
servers. It allows clients to send commands such as "play" or "pause" to the server, to enable
control of the media being streamed. The following example shows a minimal RTSP server using the
`vmbsrc` element to stream image data from a camera via the network to a client machine. This
example uses Python to start a pre-implemented RTSP server that can be imported via the PyGObject
package. To do this a few external dependencies must be installed.

### Dependencies

The following instructions assume an Ubuntu system. On other distributions different packages may be
required. It is also assumed, that a working GStreamer installation exists on the system and that
`vmbsrc` is available to that installation.

To have access to the GStreamer RTSP Server from python, the following system packages need to be
installed via the `apt` package manager:
- gir1.2-gst-rtsp-server-1.0
- libgirepository1.0-dev
- libcairo2-dev

Additionally the following python package needs to be installed. it is available via the Python
packaging index and can be installed as usual via `pip`:
- PyGObject

### Example code

The following python code will start an RTSP server on your machine listing on port `8554`. Be sure
to adjust the ID of the camera you want to use in the pipeline! As before the pipeline may be
adjusted to specify certain image formats to force for example color images or to change camera
settings like the exposure time.

```python
# import required GStreamer and GLib modules
import gi
gi.require_version('Gst', '1.0')
gi.require_version('GstRtspServer', '1.0')
from gi.repository import Gst, GLib, GstRtspServer

# initialize GStreamer and start the GLib Mainloop
Gst.init(None)
mainloop = GLib.MainLoop()

# Create the RTSP Server
server = GstRtspServer.RTSPServer()
mounts = server.get_mount_points()

# define the pipeline to record images ad attach it to the "stream1" endpoint
vmbsrc_factory = GstRtspServer.RTSPMediaFactory()
vmbsrc_factory.set_launch('vmbsrc camera=DEV_1AB22D01BBB8 ! videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency ! rtph264pay name=pay0')
mounts.add_factory("/stream1", vmbsrc_factory)
server.attach(None)

mainloop.run()
```

To start the server, simply save the code above to a file (e.g. `RTSP_Server.py`) and run it with
your python interpreter. The RTSP server can be stopped by halting the process. This is done by
pressing `CTRL + c` in the terminal that is running the python script.

### Displaying the stream

After starting the python example a client must connect to the running RTSP server to receive and
display the stream. This can for example be done with the VLC media player. To display the stream on
the same machine that is running the RTSP server, open `rtsp://127.0.0.1:8554/stream1` as a Network
Stream (in VLC open "Media" -> "Open Network Stream"). If you want to play the video on a different
computer, ensure that a network connection between the two systems exists and use the IP address of
the machine running the RTSP server for your Network Stream.

Upon starting playback in VLC the RTSP Server will start the GStreamer pipeline that was defined in
the python file and start streaming images. It may take some time for the stream to start. Stopping
playback will also stop the GStreamer pipeline and close the connection to the camera being used.
