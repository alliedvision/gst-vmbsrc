# Usage Examples

GStreamer provides a larges selection of plugins, which can be used to define flexible pipelines for
many uses. Some examples of common goals are provided in this file.

## Saving camera frames as picture

Recording pictures from a camera and saving them to some common image format allows for quick
inspections of the field of view, brightness and sharpness of the image. GStreamer provides image
encoders different image formats. The example below uses the `png` encoder. A step by step
explanation of the elements in the pipeline is given below.

```
gst-launch-1.0 vimbasrc camera=DEV_1AB22D01BBB8 num-buffers=1 ! pngenc ! filesink location=out.png
```

- `vimbasrc camera=DEV_1AB22D01BBB8 num-buffers=1`: uses the `gst-vimbasrc` element to grab one
  single frame from the camera with the given ID and halt the pipeline afterwards
- `pngenc`: takes the input image and encodes it into a `png` file
- `filesink location=out.png`: saves the input data (the encoded `png` image) and saves it to the
  file `out.png` in the current working directory

Similarly it is possible to save a number of camera frames to separate image files. This can be
achieved by using the `multifilesink` element to save the images.

```
gst-launch-1.0 vimbasrc camera=DEV_1AB22D01BBB8 num-buffers=10 ! pngenc ! multifilesink location=tmp/out_%03d.png
```

Similarly to the previous example, this pipeline uses the `gst-vimbasrc` element to record images
from the camera. Here however 10 images are recorded. The `multifilesink` saves these images to
separate files, named `out_000.png`, `out_001.png`, ... , `out_009.png`.

Further changes to the pipeline are possible to for example change the format of the recorded images
to ensure RGB images, or adjust the exposure time of the image acquisition process. For more details
see the README of the `gst-vimbasrc` element.

## Saving camera stream to a video file

To save a stream of images recorded by a camera to a video file the images should be encoded in some
video format and stored in an appropriate container format. This saves a loot of space compared to
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
  framerate (due to effects like possible hardware triggers or frame jitter). Because `avi` file
  only support fixed framerates, it needs to be modified via the `videorate` plugin. This guarantees
  a fixed framerate output by either copying the input data if more frames are requested than
  received, or dropping unnecessary frames if more frames are received than requested.
- `videoconvert ! queue`: converts the input image to a compatible video format for the following
  element
- `x264enc`: performs the encoding to h264 video
- `avimux`: multiplex the incoming video stream to save it as an `avi` file
- `filesink location=output.avi`: saves the resulting video into a file named `output.avi` in the
  current working directory
