# Usage Examples

GStreamer provides a larges selection of plugins, which can be used to define flexible pipelines for
many uses. Some examples of common goals are provided in this file.

## Saving camera frames as pictures

Recording pictures from a camera and saving them to some common image format allows for quick
inspections of the field of view, brightness and sharpness of the image. GStreamer provides image
encoders for different image formats. The example below uses the `png` encoder. A step by step
explanation of the elements in the pipeline is given below.
```
gst-launch-1.0 vimbasrc camera=DEV_1AB22D01BBB8 num-buffers=1 ! pngenc ! filesink location=out.png
```

- `vimbasrc camera=DEV_1AB22D01BBB8 num-buffers=1`: uses the `gst-vimbasrc` element to grab one
  single frame from the camera with the given ID and halt the pipeline afterwards
- `pngenc`: takes the input image and encodes it into a `png` file
- `filesink location=out.png`: writes the data it receives (the encoded `png` image) to the file
  `out.png` in the current working directory

Similarly it is possible to save a number of camera frames to separate image files. This can be
achieved by using the `multifilesink` element to save the images.
```
gst-launch-1.0 vimbasrc camera=DEV_1AB22D01BBB8 num-buffers=10 ! pngenc ! multifilesink location=out_%03d.png
```

Similarly to the previous example, this pipeline uses the `gst-vimbasrc` element to record images
from the camera. Here however 10 images are recorded. The `multifilesink` saves these images to
separate files, named `out_000.png`, `out_001.png`, ... , `out_009.png`.

Further changes to the pipeline are possible to for example change the format of the recorded images
to ensure RGB images, or adjust the exposure time of the image acquisition process. For more details
see the README of the `gst-vimbasrc` element.

## Saving camera stream to a video file

To save a stream of images recorded by a camera to a video file the images should be encoded in some
video format and stored in an appropriate container format. This saves a lot of space compared to
just saving the raw image data. This example uses `h264` encoding for the image data and saves the
resulting video to an `avi` file. An explanation for the separate elements of the pipeline can be
found below.
```
gst-launch-1.0 vimbasrc camera=DEV_000F315B91E2 ! video/x-raw,format=RGB ! videorate ! video/x-raw,framerate=30/1 ! videoconvert ! queue ! x264enc ! avimux ! filesink location=output.avi
```

- `vimbasrc camera=DEV_000F315B91E2`: uses the `gst-vimbasrc` element to grab camera frames from the
  Vimba compatible camera with the given ID. For more information on the functionality of
  `gst-vimbasrc` see the README
- `video/x-raw,format=RGB`: a gst capsfilter element that limits the available data formats to `RGB`
  to ensure color images for the resulting video stream. Without this the pipeline may negotiate
  grayscale images
- `videorate ! video/x-raw,framerate=30/1`: `gst-vimbasrc` provides image data in a variable
  framerate (due to effects like possible hardware triggers or frame jitter). Because `avi` files
  only support fixed framerates, it needs to be modified via the `videorate` plugin. This guarantees
  a fixed framerate output by either copying the input data if more frames are requested than
  received, or dropping unnecessary frames if more frames are received than requested.
- `videoconvert ! queue`: converts the input image to a compatible video format for the following
  element
- `x264enc`: performs the encoding to h264 video
- `avimux`: multiplex the incoming video stream to save it as an `avi` file
- `filesink location=output.avi`: saves the resulting video into a file named `output.avi` in the
  current working directory

## Stream video via RTSP server

RTSP (Real Time Streaming Protocol) is a network protocol designed to control streaming media
servers. It allows clients to send commands such as "play" or "pause" to the server, to enable
control of the media being streamed. The following example shows a minimal RTSP server using the
`gst-vimbasrc` element to stream image data from a camera via the network to a client machine. This
example uses Python to start a pre-implemented RTSP server that can be imported via the PyGObject
package. To do this a few external dependencies must be installed.

### Dependencies

The following instructions assume an Ubuntu system. On other distributions different packages may be
required. It is also assumed, that a working GStreamer installation exists on the system and that
`gst-vimbasrc` is available to that installation.

To have access to the GStreamer RTSP Server from python the following system packages need to be
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
vimbasrc_factory = GstRtspServer.RTSPMediaFactory()
vimbasrc_factory.set_launch('vimbasrc camera=DEV_1AB22D01BBB8 ! videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency ! rtph264pay name=pay0')
mounts.add_factory("/stream1", vimbasrc_factory)
server.attach(None)

mainloop.run()
```

To start the server simply save the code above to a file (e.g. `RTSP_Server.py`) and run it with
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
